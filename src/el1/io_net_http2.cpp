#include "io_net_http.hpp"
#include "io_collection_list.hpp"
#include "system_task.hpp"

#include <nghttp2/nghttp2.h>
#include <string.h>

namespace el1::io::net::http
{
	using namespace io::collection::list;
	using namespace io::net::ip;
	using namespace io::stream;
	using namespace io::text::string;
	using namespace system::task;
	using namespace system::waitable;

	namespace
	{
		static TString Http2String(const u8_t* const data, const usys_t size)
		{
			TList<char> buffer(size + 1U);
			buffer.Append(reinterpret_cast<const char*>(data), size);
			buffer.Append('\0');
			return TString(buffer.ItemPtr(0));
		}

		static EMethod Http2Method(const TStringView method)
		{
			if(method == U"GET") return EMethod::GET;
			if(method == U"POST") return EMethod::POST;
			if(method == U"HEAD") return EMethod::HEAD;
			if(method == U"PUT") return EMethod::PUT;
			if(method == U"PATCH") return EMethod::PATCH;
			if(method == U"DELETE") return EMethod::DELETE;
			if(method == U"TRACE") return EMethod::TRACE;
			if(method == U"OPTIONS") return EMethod::OPTIONS;
			if(method == U"CONNECT") return EMethod::CONNECT;
			EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"unknown HTTP/2 :method");
		}

		static void ParseHttp2Target(THttpRequest& request, TString target)
		{
			EL_ERROR(target.Length() == 0, THttpProcessingException, EStatus::BAD_REQUEST, U"empty HTTP/2 :path");
			request.url = std::move(target);
			const usys_t pos_args = request.url.Find('?');
			if(pos_args != NEG1)
			{
				EL_ERROR(pos_args == 0, THttpProcessingException, EStatus::BAD_REQUEST, U"empty HTTP/2 path");
				TList<TString> arg_strs = request.url.SliceSL(pos_args + 1).Split('&');
				for(auto& arg : arg_strs)
				{
					EL_ERROR(arg.Length() == 0, THttpProcessingException, EStatus::BAD_REQUEST, U"empty request parameter");
					if(arg.Contains('='))
					{
						auto kv = arg.SplitKV('=');
						request.args.Add(UrlDecode(std::move(kv.key)), UrlDecode(std::move(kv.value)));
					}
					else
						request.args.Add(UrlDecode(std::move(arg)), U"");
				}
				request.url = request.url.SliceBE(0, pos_args);
			}
			request.url = UrlDecode(std::move(request.url));
		}

		class THttp2RequestBody final : public IHttpRequestBody
		{
			class TInputWaitable final : public IWaitable
			{
				const THttp2RequestBody* const body;
				public:
					explicit TInputWaitable(const THttp2RequestBody* const body) : body(body) {}
					bool IsReady() const final override { return body->buffer.Count() != 0 || body->complete; }
			};

			TList<byte_t> buffer;
			nghttp2_session** const session;
			const s32_t stream_id;
			u8_t* const wakeup;
			bool complete = false;
			THttpHeaderFields trailers;
			const TInputWaitable input_waitable;

			public:
				THttp2RequestBody(nghttp2_session** const session, const s32_t stream_id, u8_t* const wakeup) :
					session(session), stream_id(stream_id), wakeup(wakeup), input_waitable(this) {}

				void Append(const u8_t* const data, const usys_t size)
				{
					buffer.Append(reinterpret_cast<const byte_t*>(data), size);
				}

				void Finish() { complete = true; }
				THttpHeaderFields& TrailersMutable() { return trailers; }

				bool Complete() const final override { return complete && buffer.Count() == 0; }
				usys_t Remaining() const final override { return complete ? buffer.Count() : NEG1; }
				const THttpHeaderFields& Trailers() const final override { return trailers; }

				usys_t Read(byte_t* const items, const usys_t count) final override
				{
					const usys_t n = util::Min(count, buffer.Count());
					if(n == 0)
						return 0;
					memcpy(items, buffer.ItemPtr(0), n);
					buffer.Remove(0, n);
					if(*session != nullptr)
					{
						const int result = nghttp2_session_consume(*session, stream_id, n);
						EL_ERROR(result != 0, TException, U"nghttp2_session_consume failed");
						*wakeup = 1;
					}
					return n;
				}

				const IWaitable* OnInputReady() const final override
				{
					return !complete && buffer.Count() == 0 ? &input_waitable : nullptr;
				}

				void Discard() final override
				{
					byte_t tmp[4096];
					while(!Complete())
					{
						if(Read(tmp, sizeof(tmp)) != 0)
							continue;
						const IWaitable* const waitable = OnInputReady();
						if(waitable != nullptr)
							waitable->WaitFor();
					}
				}
		};

		struct THttp2Connection;

		struct THttp2Stream
		{
			THttp2Connection* const connection;
			const s32_t id;
			THttpRequest request;
			THttp2RequestBody body;
			std::unique_ptr<TFiber> handler_fiber;
			std::unique_ptr<ISource<byte_t>> response_body;
			bool request_headers_complete = false;
			bool response_submitted = false;
			bool response_data_deferred = false;
			bool closed = false;
			usys_t header_size = 0;

			THttp2Stream(THttp2Connection* const connection, const s32_t id, const ipport_t remote_address, nghttp2_session** const session, u8_t* const wakeup) :
				connection(connection), id(id), body(session, id, wakeup)
			{
				request.version = EVersion::HTTP20;
				request.remote_address = remote_address;
				request.body = &body;
			}
		};

		struct THttp2Connection
		{
			ISource<byte_t>& source;
			ISink<byte_t>& sink;
			const THttpServer::request_handler_t handler;
			const ipport_t remote_address;
			nghttp2_session* session = nullptr;
			TList<std::unique_ptr<THttp2Stream>> streams;
			TList<byte_t> output;
			u8_t wakeup = 0;
			TMemoryWaitable<u8_t> wakeup_waitable;

			THttp2Stream* FindStream(const s32_t id)
			{
				for(auto& stream : streams)
					if(stream->id == id)
						return stream.get();
				return nullptr;
			}

			static int OnBeginHeaders(nghttp2_session*, const nghttp2_frame* const frame, void* const user_data)
			{
				auto* const self = static_cast<THttp2Connection*>(user_data);
				if(frame->hd.type != NGHTTP2_HEADERS)
					return 0;

				if(frame->headers.cat == NGHTTP2_HCAT_REQUEST)
				{
					EL_ERROR(self->FindStream(frame->hd.stream_id) != nullptr, TLogicException);
					self->streams.MoveAppend(el1::New<THttp2Stream>(self, frame->hd.stream_id, self->remote_address, &self->session, &self->wakeup));
				}
				return 0;
			}

			static int OnHeader(nghttp2_session*, const nghttp2_frame* const frame, const u8_t* const name, const size_t name_len, const u8_t* const value, const size_t value_len, u8_t, void* const user_data)
			{
				auto* const self = static_cast<THttp2Connection*>(user_data);
				THttp2Stream* const stream = self->FindStream(frame->hd.stream_id);
				if(stream == nullptr)
					return 0;

				stream->header_size += name_len + value_len;
				if(stream->header_size > 64U * 1024U)
					return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;

				TString str_name = Http2String(name, name_len);
				TString str_value = Http2String(value, value_len);

				if(frame->headers.cat == NGHTTP2_HCAT_REQUEST)
				{
					if(str_name == U":method")
						stream->request.method = Http2Method(str_value);
					else if(str_name == U":path")
						ParseHttp2Target(stream->request, std::move(str_value));
					else if(str_name == U":authority")
						stream->request.header_fields.Set(U"host", std::move(str_value));
					else if(str_name[0] != ':')
					{
						TString* const existing = stream->request.header_fields.Get(str_name);
						if(existing == nullptr)
							stream->request.header_fields.Add(std::move(str_name), std::move(str_value));
						else
						{
							*existing += str_name == U"cookie" ? TStringView(U"; ") : TStringView(U", ");
							*existing += str_value;
						}
					}
				}
				else if(frame->headers.cat == NGHTTP2_HCAT_HEADERS && str_name[0] != ':')
					stream->body.TrailersMutable().Set(std::move(str_name), std::move(str_value));
				return 0;
			}

			static int OnDataChunk(nghttp2_session*, u8_t, const s32_t stream_id, const u8_t* const data, const size_t len, void* const user_data)
			{
				auto* const self = static_cast<THttp2Connection*>(user_data);
				THttp2Stream* const stream = self->FindStream(stream_id);
				if(stream != nullptr)
					stream->body.Append(data, len);
				return 0;
			}

			void StartHandler(THttp2Stream& stream)
			{
				if(stream.handler_fiber != nullptr)
					return;
				stream.handler_fiber = el1::New<TFiber>([this, &stream]()
				{
					THttpServer::response_t response;
					response.status = EStatus::INTERNAL_SERVER_ERROR;
					response.version = EVersion::HTTP20;
					try
					{
						handler(stream.request, response);
					}
					catch(const THttpProcessingException& exception)
					{
						response = THttpServer::response_t{};
						response.status = exception.status;
						response.version = EVersion::HTTP20;
					}
					catch(const IException&)
					{
						response = THttpServer::response_t{};
						response.status = EStatus::INTERNAL_SERVER_ERROR;
						response.version = EVersion::HTTP20;
					}
					SubmitResponse(stream, response);
				});
			}

			static int OnFrameRecv(nghttp2_session*, const nghttp2_frame* const frame, void* const user_data)
			{
				auto* const self = static_cast<THttp2Connection*>(user_data);
				THttp2Stream* const stream = self->FindStream(frame->hd.stream_id);
				if(stream == nullptr)
					return 0;

				if(frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST)
				{
					stream->request_headers_complete = true;
					if(frame->hd.flags & NGHTTP2_FLAG_END_STREAM)
					{
						stream->body.Finish();
						stream->request.body = nullptr;
					}
					self->StartHandler(*stream);
				}
				else if((frame->hd.type == NGHTTP2_DATA || frame->hd.type == NGHTTP2_HEADERS) && (frame->hd.flags & NGHTTP2_FLAG_END_STREAM))
					stream->body.Finish();
				return 0;
			}

			static int OnStreamClose(nghttp2_session*, const s32_t stream_id, u32_t, void* const user_data)
			{
				auto* const self = static_cast<THttp2Connection*>(user_data);
				THttp2Stream* const stream = self->FindStream(stream_id);
				if(stream != nullptr)
				{
					stream->closed = true;
					stream->body.Finish();
				}
				return 0;
			}

			static ssize_t ReadResponseData(nghttp2_session*, const s32_t, u8_t* const buffer, const size_t length, u32_t* const data_flags, nghttp2_data_source* const source, void*)
			{
				auto* const stream = static_cast<THttp2Stream*>(source->ptr);
				if(stream->response_body == nullptr)
				{
					*data_flags |= NGHTTP2_DATA_FLAG_EOF;
					return 0;
				}
				const usys_t n = stream->response_body->Read(reinterpret_cast<byte_t*>(buffer), length);
				if(n != 0)
					return static_cast<ssize_t>(n);
				if(stream->response_body->OnInputReady() != nullptr)
				{
					stream->response_data_deferred = true;
					return NGHTTP2_ERR_DEFERRED;
				}
				*data_flags |= NGHTTP2_DATA_FLAG_EOF;
				return 0;
			}

			void SubmitResponse(THttp2Stream& stream, THttpServer::response_t& response)
			{
				if(stream.response_submitted || stream.closed)
					return;
				stream.response_submitted = true;

				TList<nghttp2_nv> fields(response.header_fields.Items().Count() + 1U);
				TList<TList<byte_t>> storage(response.header_fields.Items().Count() * 2U + 2U);
				auto add_field = [&fields, &storage](const TStringView name, const TStringView value)
				{
					auto name_cstr = name.MakeCStr();
					auto value_cstr = value.MakeCStr();
					TList<byte_t> name_data;
					name_data.Append(reinterpret_cast<const byte_t*>(name_cstr.get()), strlen(name_cstr.get()));
					TList<byte_t> value_data;
					value_data.Append(reinterpret_cast<const byte_t*>(value_cstr.get()), strlen(value_cstr.get()));
					storage.MoveAppend(std::move(name_data));
					storage.MoveAppend(std::move(value_data));
					const auto& n = storage[storage.Count() - 2U];
					const auto& v = storage[storage.Count() - 1U];
					nghttp2_nv field = { const_cast<u8_t*>(reinterpret_cast<const u8_t*>(n.ItemPtr(0))), const_cast<u8_t*>(reinterpret_cast<const u8_t*>(v.ItemPtr(0))), n.Count(), v.Count(), NGHTTP2_NV_FLAG_NONE };
					fields.Append(field);
				};

				add_field(U":status", TString::Format(U"%d", static_cast<u16_t>(response.status)));
				for(const auto& field : response.header_fields.Items())
				{
					TString name = field.key;
					name.ToLower();
					if(name == U"connection" || name == U"transfer-encoding" || name == U"keep-alive" || name == U"upgrade")
						continue;
					add_field(name, field.value);
				}

				const bool suppress_body = stream.request.method == EMethod::HEAD;
				stream.response_body = suppress_body ? nullptr : std::move(response.body);
				nghttp2_data_provider provider{};
				nghttp2_data_provider* provider_ptr = nullptr;
				if(stream.response_body != nullptr)
				{
					provider.source.ptr = &stream;
					provider.read_callback = &ReadResponseData;
					provider_ptr = &provider;
				}
				const int result = nghttp2_submit_response(session, stream.id, fields.ItemPtr(0), fields.Count(), provider_ptr);
				EL_ERROR(result != 0, TException, U"nghttp2_submit_response failed");
				wakeup = 1;
			}

			void ResumeDeferredBodies()
			{
				for(auto& stream : streams)
				{
					if(!stream->response_data_deferred || stream->response_body == nullptr)
						continue;
					const IWaitable* const waitable = stream->response_body->OnInputReady();
					if(waitable == nullptr || waitable->IsReady())
					{
						stream->response_data_deferred = false;
						const int result = nghttp2_session_resume_data(session, stream->id);
						EL_ERROR(result != 0, TException, U"nghttp2_session_resume_data failed");
					}
				}
			}

			void FlushOutput(bool& progress)
			{
				if(output.Count() == 0)
				{
					const u8_t* data = nullptr;
					const ssize_t size = nghttp2_session_mem_send(session, &data);
					EL_ERROR(size < 0, TException, U"nghttp2_session_mem_send failed");
					if(size > 0)
						output.Append(reinterpret_cast<const byte_t*>(data), size);
				}
				if(output.Count() != 0)
				{
					const usys_t n = sink.Write(output.ItemPtr(0), output.Count());
					if(n != 0)
					{
						output.Remove(0, n);
						progress = true;
					}
				}
			}

			void CleanupStreams()
			{
				for(ssys_t i = streams.Count() - 1; i >= 0; i--)
				{
					if(!streams[i]->closed)
						continue;
					if(streams[i]->handler_fiber != nullptr && streams[i]->handler_fiber->IsAlive())
						continue;
					if(streams[i]->handler_fiber != nullptr)
						(void)streams[i]->handler_fiber->Join();
					streams.Remove(i);
				}
			}

			void Run()
			{
				nghttp2_session_callbacks* callbacks = nullptr;
				EL_ERROR(nghttp2_session_callbacks_new(&callbacks) != 0, TException, U"failed to allocate nghttp2 callbacks");
				nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, &OnBeginHeaders);
				nghttp2_session_callbacks_set_on_header_callback(callbacks, &OnHeader);
				nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, &OnDataChunk);
				nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, &OnFrameRecv);
				nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, &OnStreamClose);
				nghttp2_option* options = nullptr;
				EL_ERROR(nghttp2_option_new(&options) != 0, TException, U"failed to allocate nghttp2 options");
				nghttp2_option_set_no_auto_window_update(options, 1);
				const int create_result = nghttp2_session_server_new2(&session, callbacks, this, options);
				nghttp2_option_del(options);
				nghttp2_session_callbacks_del(callbacks);
				EL_ERROR(create_result != 0, TException, U"failed to create nghttp2 server session");

				const nghttp2_settings_entry settings[] = {
					{ NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100U },
					{ NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE, 64U * 1024U },
				};
				EL_ERROR(nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, settings, sizeof(settings) / sizeof(settings[0])) != 0, TException, U"failed to submit HTTP/2 SETTINGS");

				byte_t input[4U * 1024U];
				for(;;)
				{
					bool progress = false;
					ResumeDeferredBodies();
					FlushOutput(progress);

					if(nghttp2_session_want_read(session))
					{
						const usys_t n = source.Read(input, sizeof(input));
						if(n != 0)
						{
							const ssize_t consumed = nghttp2_session_mem_recv(session, reinterpret_cast<const u8_t*>(input), n);
							EL_ERROR(consumed < 0 || static_cast<usys_t>(consumed) != n, TException, U"invalid HTTP/2 input");
							progress = true;
						}
						else if(source.OnInputReady() == nullptr)
							break;
					}

					FlushOutput(progress);
					CleanupStreams();
					if(!nghttp2_session_want_read(session) && !nghttp2_session_want_write(session) && output.Count() == 0)
						break;
					if(progress)
						continue;

					wakeup = 0;
					TList<const IWaitable*> waitables;
					waitables.Append(&wakeup_waitable);
					if(nghttp2_session_want_read(session))
						if(const IWaitable* const waitable = source.OnInputReady(); waitable != nullptr)
							waitables.Append(waitable);
					if(output.Count() != 0)
						if(const IWaitable* const waitable = sink.OnOutputReady(); waitable != nullptr)
							waitables.Append(waitable);
					for(auto& stream : streams)
						if(stream->response_data_deferred && stream->response_body != nullptr)
							if(const IWaitable* const waitable = stream->response_body->OnInputReady(); waitable != nullptr)
								waitables.Append(waitable);
					if(waitables.Count() == 0)
						break;
					TFiber::WaitForMany(waitables);
				}

				nghttp2_session_del(session);
				session = nullptr;
			}

			THttp2Connection(ISource<byte_t>& source, ISink<byte_t>& sink, THttpServer::request_handler_t handler, const ipport_t remote_address) :
				source(source), sink(sink), handler(std::move(handler)), remote_address(remote_address),
				wakeup_waitable(&wakeup, nullptr, 0xff)
			{}

			~THttp2Connection()
			{
				if(session != nullptr)
					nghttp2_session_del(session);
			}
		};
	}

	void THttpServer::HandleHttp2Connection(ISource<byte_t>& source, ISink<byte_t>& sink, request_handler_t handler, const ipport_t remote_address)
	{
		auto connection = el1::New<THttp2Connection>(source, sink, std::move(handler), remote_address);
		connection->Run();
	}
}
