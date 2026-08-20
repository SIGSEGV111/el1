#include "io_net_http.hpp"
#include "io_collection_list.hpp"
#include "system_task.hpp"

#include <nghttp3/nghttp3.h>
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
		static TString Http3String(const nghttp3_rcbuf* const buffer)
		{
			const nghttp3_vec vec = nghttp3_rcbuf_get_buf(buffer);
			return TString((const char*)vec.base, vec.len);
		}

		static EMethod Http3Method(const TStringView method)
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
			EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"unknown HTTP/3 :method");
		}

		static void ParseHttp3Target(THttpRequest& request, TString target)
		{
			EL_ERROR(target.Length() == 0, THttpProcessingException, EStatus::BAD_REQUEST, U"empty HTTP/3 :path");
			request.url = std::move(target);
			const usys_t pos_args = request.url.Find('?');
			if(pos_args != NEG1)
			{
				EL_ERROR(pos_args == 0, THttpProcessingException, EStatus::BAD_REQUEST, U"empty HTTP/3 path");
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

		struct THttp3Connection;

		class THttp3RequestBody final : public IHttpRequestBody
		{
			class TInputWaitable final : public IWaitable
			{
				const THttp3RequestBody* const body;
				public:
					explicit TInputWaitable(const THttp3RequestBody* const body) : body(body) {}
					bool IsReady() const final override { return body->buffer.Count() != 0 || body->complete; }
			};

			TList<byte_t> buffer;
			THttp3Connection* const connection;
			bool complete = false;
			THttpHeaderFields trailers;
			const TInputWaitable input_waitable;

			public:
				static constexpr usys_t MAX_BUFFERED = 64U * 1024U;

				THttp3RequestBody(THttp3Connection* const connection, const int64_t) :
					connection(connection), input_waitable(this) {}

				void Append(const u8_t* const data, const usys_t size)
				{
					buffer.Append(reinterpret_cast<const byte_t*>(data), size);
				}

				void Finish() { complete = true; }
				THttpHeaderFields& TrailersMutable() { return trailers; }
				bool CanReceive() const { return buffer.Count() < MAX_BUFFERED; }

				bool Complete() const final override { return complete && buffer.Count() == 0; }
				usys_t Remaining() const final override { return complete ? buffer.Count() : NEG1; }
				const THttpHeaderFields& Trailers() const final override { return trailers; }

				usys_t Read(byte_t* const items, const usys_t count) final override;

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

		struct THttp3Stream
		{
			THttp3Connection* const connection;
			std::unique_ptr<quic::TStream> transport;
			const int64_t id;
			THttpRequest request;
			THttp3RequestBody body;
			std::unique_ptr<TFiber> handler_fiber;
			std::unique_ptr<ISource<byte_t>> response_body;
			TList<std::unique_ptr<TList<byte_t>>> response_chunks;
			usys_t response_acked = 0;
			bool request_headers_complete = false;
			bool response_submitted = false;
			bool response_data_deferred = false;
			bool input_fin_reported = false;
			bool output_fin_reported = false;
			bool closed = false;
			usys_t header_size = 0;

			THttp3Stream(THttp3Connection* const connection, std::unique_ptr<quic::TStream> transport, const ipport_t remote_address) :
				connection(connection), transport(std::move(transport)), id(this->transport->Id()), body(connection, id)
			{
				request.version = EVersion::HTTP30;
				request.remote_address = remote_address;
				request.body = &body;
			}
		};

		struct THttp3Connection
		{
			quic::TConnection& transport;
			const THttpServer::request_handler_t handler;
			const ipport_t remote_address;
			nghttp3_conn* session = nullptr;
			TList<std::unique_ptr<THttp3Stream>> streams;
			u8_t wakeup = 0;
			int64_t blocked_output_stream_id = -1;
			TMemoryWaitable<u8_t> wakeup_waitable;

			THttp3Stream* FindStream(const int64_t id)
			{
				for(auto& stream : streams)
					if(stream->id == id)
						return stream.get();
				return nullptr;
			}

			void Wake() { wakeup = 1; }

			static int BeginHeaders(nghttp3_conn* const session, int64_t stream_id, void* const user_data, void*)
			{
				auto* const self = static_cast<THttp3Connection*>(user_data);
				THttp3Stream* const stream = self->FindStream(stream_id);
				if(stream == nullptr)
					return NGHTTP3_ERR_CALLBACK_FAILURE;
				return nghttp3_conn_set_stream_user_data(session, stream_id, stream);
			}

			static int RecvHeader(nghttp3_conn*, int64_t, int32_t, nghttp3_rcbuf* const name, nghttp3_rcbuf* const value, const u8_t, void*, void* const stream_user_data)
			{
				auto* const stream = static_cast<THttp3Stream*>(stream_user_data);
				if(stream == nullptr)
					return NGHTTP3_ERR_CALLBACK_FAILURE;
				TString str_name = Http3String(name);
				TString str_value = Http3String(value);
				stream->header_size += str_name.Length() + str_value.Length();
				if(stream->header_size > 64U * 1024U)
					return NGHTTP3_ERR_CALLBACK_FAILURE;

				if(str_name == U":method")
					stream->request.method = Http3Method(str_value);
				else if(str_name == U":path")
					ParseHttp3Target(stream->request, std::move(str_value));
				else if(str_name == U":authority")
					stream->request.header_fields.Set(U"host", std::move(str_value));
				else if(str_name.Length() != 0 && str_name[0] != ':')
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
				return 0;
			}

			static int RecvTrailer(nghttp3_conn*, int64_t, int32_t, nghttp3_rcbuf* const name, nghttp3_rcbuf* const value, const u8_t, void*, void* const stream_user_data)
			{
				auto* const stream = static_cast<THttp3Stream*>(stream_user_data);
				if(stream == nullptr)
					return NGHTTP3_ERR_CALLBACK_FAILURE;
				stream->body.TrailersMutable().Set(Http3String(name), Http3String(value));
				return 0;
			}

			static int RecvData(nghttp3_conn*, int64_t, const u8_t* const data, const size_t size, void*, void* const stream_user_data)
			{
				auto* const stream = static_cast<THttp3Stream*>(stream_user_data);
				if(stream == nullptr)
					return NGHTTP3_ERR_CALLBACK_FAILURE;
				stream->body.Append(data, size);
				return 0;
			}

			void StartHandler(THttp3Stream& stream)
			{
				if(stream.handler_fiber != nullptr)
					return;
				stream.handler_fiber = el1::New<TFiber>([this, &stream]()
				{
					THttpServer::response_t response;
					response.status = EStatus::INTERNAL_SERVER_ERROR;
					response.version = EVersion::HTTP30;
					try
					{
						handler(stream.request, response);
					}
					catch(const THttpProcessingException& exception)
					{
						response = THttpServer::response_t{};
						response.status = exception.status;
						response.version = EVersion::HTTP30;
					}
					catch(const IException&)
					{
						response = THttpServer::response_t{};
						response.status = EStatus::INTERNAL_SERVER_ERROR;
						response.version = EVersion::HTTP30;
					}
					SubmitResponse(stream, response);
				});
			}

			static int EndHeaders(nghttp3_conn*, int64_t, const int fin, void* const user_data, void* const stream_user_data)
			{
				auto* const self = static_cast<THttp3Connection*>(user_data);
				auto* const stream = static_cast<THttp3Stream*>(stream_user_data);
				if(stream == nullptr)
					return NGHTTP3_ERR_CALLBACK_FAILURE;
				stream->request_headers_complete = true;
				if(fin)
				{
					stream->body.Finish();
					stream->request.body = nullptr;
				}
				self->StartHandler(*stream);
				return 0;
			}

			static int EndStream(nghttp3_conn*, int64_t, void*, void* const stream_user_data)
			{
				if(auto* const stream = static_cast<THttp3Stream*>(stream_user_data); stream != nullptr)
					stream->body.Finish();
				return 0;
			}

			static int StreamClose(nghttp3_conn*, int64_t, uint64_t, void*, void* const stream_user_data)
			{
				if(auto* const stream = static_cast<THttp3Stream*>(stream_user_data); stream != nullptr)
				{
					stream->closed = true;
					stream->body.Finish();
				}
				return 0;
			}

			static int StopSending(nghttp3_conn*, int64_t, uint64_t error, void*, void* const stream_user_data)
			{
				if(auto* const stream = static_cast<THttp3Stream*>(stream_user_data); stream != nullptr)
					stream->transport->Reset(error);
				return 0;
			}

			static int ResetStream(nghttp3_conn*, int64_t, uint64_t error, void*, void* const stream_user_data)
			{
				if(auto* const stream = static_cast<THttp3Stream*>(stream_user_data); stream != nullptr)
					stream->transport->Reset(error);
				return 0;
			}

			static nghttp3_ssize ReadResponseData(nghttp3_conn*, int64_t, nghttp3_vec* const vec, const size_t veccnt, u32_t* const flags, void*, void* const stream_user_data)
			{
				auto* const stream = static_cast<THttp3Stream*>(stream_user_data);
				if(stream == nullptr || veccnt == 0)
					return NGHTTP3_ERR_CALLBACK_FAILURE;
				if(stream->response_body == nullptr)
				{
					*flags |= NGHTTP3_DATA_FLAG_EOF;
					return 0;
				}
				byte_t buffer[4096];
				const usys_t n = stream->response_body->Read(buffer, sizeof(buffer));
				if(n == 0)
				{
					if(stream->response_body->OnInputReady() != nullptr)
					{
						stream->response_data_deferred = true;
						return NGHTTP3_ERR_WOULDBLOCK;
					}
					*flags |= NGHTTP3_DATA_FLAG_EOF;
					return 0;
				}
				auto chunk = el1::New<TList<byte_t>>();
				chunk->Append(buffer, n);
				vec[0].base = reinterpret_cast<u8_t*>(chunk->ItemPtr(0));
				vec[0].len = n;
				stream->response_chunks.MoveAppend(std::move(chunk));
				return 1;
			}

			static int AckedStreamData(nghttp3_conn*, int64_t, uint64_t datalen, void*, void* const stream_user_data)
			{
				auto* const stream = static_cast<THttp3Stream*>(stream_user_data);
				if(stream == nullptr)
					return 0;
				stream->response_acked += datalen;
				while(stream->response_chunks.Count() != 0 && stream->response_acked >= stream->response_chunks[0]->Count())
				{
					stream->response_acked -= stream->response_chunks[0]->Count();
					stream->response_chunks.Remove(0);
				}
				return 0;
			}

			void SubmitResponse(THttp3Stream& stream, THttpServer::response_t& response)
			{
				if(stream.response_submitted || stream.closed)
					return;
				stream.response_submitted = true;

				TList<nghttp3_nv> fields(response.header_fields.Items().Count() + 1U);
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
					fields.Append({ const_cast<u8_t*>(reinterpret_cast<const u8_t*>(n.ItemPtr(0))), const_cast<u8_t*>(reinterpret_cast<const u8_t*>(v.ItemPtr(0))), n.Count(), v.Count(), NGHTTP3_NV_FLAG_NONE });
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
				nghttp3_data_reader reader = { .read_data = &ReadResponseData };
				const int result = nghttp3_conn_submit_response(session, stream.id, fields.ItemPtr(0), fields.Count(), stream.response_body == nullptr ? nullptr : &reader);
				EL_ERROR(result != 0, TException, TString::Format(U"nghttp3_conn_submit_response failed: %s", nghttp3_strerror(result)));
				Wake();
			}

			void AcceptStreams(bool& progress)
			{
				for(;;)
				{
					auto stream = transport.TryAcceptStream();
					if(stream == nullptr)
						break;
					streams.MoveAppend(el1::New<THttp3Stream>(this, std::move(stream), remote_address));
					progress = true;
				}
			}

			void ReadStreams(bool& progress)
			{
				byte_t buffer[4096];
				for(auto& stream : streams)
				{
					if(stream->transport->IsLocal() || !stream->transport->CanRead() || stream->input_fin_reported)
						continue;
					if(stream->request_headers_complete && !stream->body.CanReceive())
						continue;

					const usys_t n = stream->transport->Read(buffer, sizeof(buffer));
					if(n != 0)
					{
						const nghttp3_ssize consumed = nghttp3_conn_read_stream(session, stream->id, reinterpret_cast<const u8_t*>(buffer), n, 0);
						EL_ERROR(consumed < 0, TException, TString::Format(U"invalid HTTP/3 input: %s", nghttp3_strerror((int)consumed)));
						progress = true;
					}
					else if(stream->transport->OnInputReady() == nullptr)
					{
						const nghttp3_ssize consumed = nghttp3_conn_read_stream(session, stream->id, nullptr, 0, 1);
						EL_ERROR(consumed < 0, TException, TString::Format(U"invalid HTTP/3 FIN: %s", nghttp3_strerror((int)consumed)));
						stream->input_fin_reported = true;
						progress = true;
					}
				}
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
						const int result = nghttp3_conn_resume_stream(session, stream->id);
						EL_ERROR(result != 0, TException, TString::Format(U"nghttp3_conn_resume_stream failed: %s", nghttp3_strerror(result)));
					}
				}
			}

			void FlushOutput(bool& progress)
			{
				blocked_output_stream_id = -1;
				for(usys_t attempt = 0; attempt < 64U; attempt++)
				{
					int64_t stream_id = -1;
					int fin = 0;
					nghttp3_vec vec[1];
					const nghttp3_ssize nvec = nghttp3_conn_writev_stream(session, &stream_id, &fin, vec, 1);
					EL_ERROR(nvec < 0, TException, TString::Format(U"nghttp3_conn_writev_stream failed: %s", nghttp3_strerror((int)nvec)));
					if(stream_id < 0)
						break;
					THttp3Stream* const stream = FindStream(stream_id);
					EL_ERROR(stream == nullptr, TLogicException);
					if(nvec == 0)
					{
						if(fin && !stream->output_fin_reported)
						{
							EL_ERROR(!stream->transport->CloseOutput(), TException, U"failed to close HTTP/3 QUIC stream output");
							stream->output_fin_reported = true;
							EL_ERROR(nghttp3_conn_add_write_offset(session, stream_id, 0) != 0, TException, U"nghttp3_conn_add_write_offset failed");
							progress = true;
						}
						break;
					}

					const usys_t n = stream->transport->Write(reinterpret_cast<const byte_t*>(vec[0].base), vec[0].len);
					if(n == 0)
					{
						blocked_output_stream_id = stream_id;
						break;
					}
					EL_ERROR(nghttp3_conn_add_write_offset(session, stream_id, n) != 0, TException, U"nghttp3_conn_add_write_offset failed");
					// OpenSSL QUIC copies accepted stream bytes into its own retransmission buffer.
					// Tell nghttp3 they are no longer referenced by the transport so application body buffers can be released.
					EL_ERROR(nghttp3_conn_add_ack_offset(session, stream_id, n) != 0, TException, U"nghttp3_conn_add_ack_offset failed");
					if(n < vec[0].len)
						blocked_output_stream_id = stream_id;
					else if(fin && !stream->output_fin_reported)
					{
						EL_ERROR(!stream->transport->CloseOutput(), TException, U"failed to close HTTP/3 QUIC stream output");
						stream->output_fin_reported = true;
					}
					progress = true;
					if(blocked_output_stream_id >= 0)
						break;
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
				nghttp3_callbacks callbacks{};
				callbacks.acked_stream_data = &AckedStreamData;
				callbacks.stream_close = &StreamClose;
				callbacks.recv_data = &RecvData;
				callbacks.begin_headers = &BeginHeaders;
				callbacks.recv_header = &RecvHeader;
				callbacks.end_headers = &EndHeaders;
				callbacks.recv_trailer = &RecvTrailer;
				callbacks.stop_sending = &StopSending;
				callbacks.end_stream = &EndStream;
				callbacks.reset_stream = &ResetStream;
				nghttp3_settings settings;
				nghttp3_settings_default(&settings);
				settings.max_field_section_size = 64U * 1024U;
				settings.qpack_max_dtable_capacity = 4096U;
				settings.qpack_encoder_max_dtable_capacity = 4096U;
				settings.qpack_blocked_streams = 16U;
				EL_ERROR(nghttp3_conn_server_new(&session, &callbacks, &settings, nullptr, this) != 0, TException, U"failed to create nghttp3 server connection");
				nghttp3_conn_set_max_concurrent_streams(session, 100U);

				auto control = transport.OpenStream(true);
				auto qenc = transport.OpenStream(true);
				auto qdec = transport.OpenStream(true);
				const int64_t control_id = control->Id();
				const int64_t qenc_id = qenc->Id();
				const int64_t qdec_id = qdec->Id();
				streams.MoveAppend(el1::New<THttp3Stream>(this, std::move(control), remote_address));
				streams.MoveAppend(el1::New<THttp3Stream>(this, std::move(qenc), remote_address));
				streams.MoveAppend(el1::New<THttp3Stream>(this, std::move(qdec), remote_address));
				EL_ERROR(nghttp3_conn_bind_control_stream(session, control_id) != 0, TException, U"failed to bind HTTP/3 control stream");
				EL_ERROR(nghttp3_conn_bind_qpack_streams(session, qenc_id, qdec_id) != 0, TException, U"failed to bind HTTP/3 QPACK streams");

				for(;;)
				{
					bool progress = false;
					AcceptStreams(progress);
					ReadStreams(progress);
					ResumeDeferredBodies();
					FlushOutput(progress);
					CleanupStreams();
					if(progress)
						continue;

					wakeup = 0;
					TList<const IWaitable*> waitables;
					waitables.Append(&wakeup_waitable);
					waitables.Append(&transport.OnStreamAvailable());
					for(auto& stream : streams)
					{
						if(!stream->transport->IsLocal() && stream->transport->CanRead() && !stream->input_fin_reported && (!stream->request_headers_complete || stream->body.CanReceive()))
							if(const IWaitable* const waitable = stream->transport->OnInputReady(); waitable != nullptr)
								waitables.Append(waitable);
						if(stream->response_data_deferred && stream->response_body != nullptr)
							if(const IWaitable* const waitable = stream->response_body->OnInputReady(); waitable != nullptr)
								waitables.Append(waitable);
					}
					if(blocked_output_stream_id >= 0)
						if(THttp3Stream* const stream = FindStream(blocked_output_stream_id); stream != nullptr)
							if(const IWaitable* const waitable = stream->transport->OnOutputReady(); waitable != nullptr)
								waitables.Append(waitable);
					TFiber::WaitForMany(waitables);
				}
			}

			THttp3Connection(quic::TConnection& transport, THttpServer::request_handler_t handler, const ipport_t remote_address) :
				transport(transport), handler(std::move(handler)), remote_address(remote_address), wakeup_waitable(&wakeup, nullptr, 0xff) {}

			~THttp3Connection()
			{
				if(session != nullptr)
					nghttp3_conn_del(session);
			}
		};

		usys_t THttp3RequestBody::Read(byte_t* const items, const usys_t count)
		{
			const usys_t n = util::Min(count, buffer.Count());
			if(n == 0)
				return 0;
			memcpy(items, buffer.ItemPtr(0), n);
			buffer.Remove(0, n);
			connection->Wake();
			return n;
		}
	}

	void THttpServer::HandleHttp3Connection(quic::TConnection& transport, request_handler_t handler, const ipport_t remote_address)
	{
		EL_ERROR(transport.ApplicationProtocol() != U"h3", TInvalidArgumentException, "transport", "HTTP/3 requires QUIC ALPN h3");
		auto connection = el1::New<THttp3Connection>(transport, std::move(handler), remote_address);
		connection->Run();
	}
}
