#include "io_net_http.hpp"
#include "io_text.hpp"
#include "io_text_encoding_utf8.hpp"

namespace el1::io::net::http
{
	using namespace stream;
	using namespace text;
	using namespace text::string;
	using namespace text::encoding;
	using namespace text::encoding::utf8;
	using namespace ip;
	using namespace file;
	using namespace collection::list;
	using namespace collection::map;
	using namespace system::task;
	using namespace system::waitable;

	// https://stackoverflow.com/questions/1097651/is-there-a-practical-http-header-length-limit
	// => 8192 characters (UTF32) seems to be reasonable limit
	// this limit is applied PER LINE *AND* FOR THE WHOLE HEADER
	static const usys_t HEADER_CHAR_LIMIT = 8192U;

	TString THttpProcessingException::Message() const
	{
		return msg;
	}

	IException* THttpProcessingException::Clone() const
	{
		return new THttpProcessingException(*this);
	}

	static EMethod MethodFromString(const TString& str)
	{
		     if(str == L"GET") return EMethod::GET;
		else if(str == L"POST") return EMethod::POST;
		else if(str == L"HEAD") return EMethod::HEAD;
		else if(str == L"PUT") return EMethod::PUT;
		else if(str == L"PATCH") return EMethod::PATCH;
		else if(str == L"DELETE") return EMethod::DELETE;
		else if(str == L"TRACE") return EMethod::TRACE;
		else if(str == L"OPTIONS") return EMethod::OPTIONS;
		else if(str == L"CONNECT") return EMethod::CONNECT;
		else EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, "unknown method");
	}

	static EVersion VersionFromString(const TString& str)
	{
		     if(str == L"HTTP/1.1") return EVersion::HTTP11;
		else if(str == L"HTTP/1.0") return EVersion::HTTP10;
		else if(str == L"HTTP/2")   return EVersion::HTTP20;
		else if(str == L"HTTP/2.0") return EVersion::HTTP20;
		else EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, "unknown http version");
	}

	static const char* VersionToString(const EVersion version)
	{
		switch(version)
		{
			case EVersion::HTTP20: return "HTTP/2";
			case EVersion::HTTP11: return "HTTP/1.1";
			case EVersion::HTTP10: return "HTTP/1.0";
		}

		EL_THROW(TLogicException);
	}

	static const char* StatusToString(const EStatus status)
	{
		switch(status)
		{
			case EStatus::OK: return "OK";
			case EStatus::CREATED: return "CREATED";
			case EStatus::BAD_REQUEST: return "BAD_REQUEST";
			case EStatus::UNAUTHORIZED: return "UNAUTHORIZED";
			case EStatus::FORBIDDEN: return "FORBIDDEN";
			case EStatus::NOT_FOUND: return "NOT FOUND";
			case EStatus::METHOD_NOT_ALLOWED: return "METHOD NOT ALLOWED";
			case EStatus::REQUEST_HEADER_FIELDS_TOO_LARGE: return "REQUEST HEADER FIELDS TOO LARGE";
			case EStatus::INTERNAL_SERVER_ERROR: return "INTERNAL SERVER ERROR";
			case EStatus::EOF: break;
		}

		EL_THROW(TException, "unsupported status code");
	}

	static void SendResponse(ISink<byte_t>& sink, const THttpServer::response_t& response)
	{
		const char* const str_version = VersionToString(response.version);
		const char* const str_status_text = StatusToString(response.status);
		auto str_status_code = TString::Format("%d", (u16_t)response.status).MakeCStr();

		sink.WriteAll((const byte_t*)str_version, strlen(str_version));
		sink.WriteAll((const byte_t*)" ", 1);
		sink.WriteAll((const byte_t*)str_status_code.get(), strlen(str_status_code.get()));
		sink.WriteAll((const byte_t*)" ", 1);
		sink.WriteAll((const byte_t*)str_status_text, strlen(str_status_text));
		sink.WriteAll((const byte_t*)"\n", 1);

		for(const auto& field : response.header_fields.Items())
		{
			auto str_key = field.key.MakeCStr();
			auto str_value = field.value.MakeCStr();

			sink.WriteAll((const byte_t*)str_key.get(), strlen(str_key.get()));
			sink.WriteAll((const byte_t*)": ", 2);
			sink.WriteAll((const byte_t*)str_value.get(), strlen(str_value.get()));
			sink.WriteAll((const byte_t*)"\n", 1);
		}

		sink.WriteAll((const byte_t*)"\n", 1);

		if(response.body != nullptr)
		{
			const usys_t n_content_length = response.header_fields.ContentLength();
			Pump(*response.body, sink, n_content_length, true);
			if(n_content_length == NEG1)
				if(!sink.CloseOutput())
					sink.Close();
		}
	}

	usys_t THttpHeaderFields::ContentLength() const
	{
		const TString* v = this->Get(L"content-length");
		if(v == nullptr)
			return NEG1;
		else
			return v->ToInteger();
	}

	void THttpHeaderFields::ContentLength(const usys_t new_content_length)
	{
		this->Set(L"content-length", TString::Format("%d", new_content_length));
	}

	EStatus THttpServer::HandleSingleRequest(ISource<byte_t>& source, ISink<byte_t>& sink, request_handler_t handler)
	{
		bool response_in_progress = false;
		try
		{
			// read line-by-line
			// first line must be something like: <METHOD> <URL> "HTTP/"<HTTP VERSION> ("GET /index.html HTTP/1.1")
			// the following lines must be key:value style
			// an empty line terminates the header
			// after that follows the request body
			// if the header has a content length field, then this terminates the body
			// otherwise all following data from the source is considered to be part of the body and no further requests can be processed

			usys_t sz_header = 0;
			auto ss = source.Pipe();
			auto ds = ss.Transform(TUTF8Decoder());
			auto lr = ds.Transform(TLineReader(HEADER_CHAR_LIMIT));

			// read request line
			const TString* request_line = lr.NextItem();
			if(request_line == nullptr)
				return EStatus::EOF;

			EL_ERROR(request_line->Length() < 14U, THttpProcessingException, EStatus::BAD_REQUEST, "request too short");
			sz_header += request_line->Length();

			auto arr_req = request_line->Split(' ', 3U);
			EL_ERROR(arr_req.Count() != 3U, THttpProcessingException, EStatus::BAD_REQUEST, "request METHOD/URL/VERSION malformed");

			request_t request;
			request.method = MethodFromString(arr_req[0]);
			request.version = VersionFromString(arr_req[2]);
			request.url = std::move(arr_req[1]);
			arr_req.Clear();

			// read header values
			while((request_line = lr.NextItem()) != nullptr && request_line->Length() > 0U)
			{
				sz_header += request_line->Length();
				EL_ERROR(sz_header > HEADER_CHAR_LIMIT, THttpProcessingException, EStatus::REQUEST_HEADER_FIELDS_TOO_LARGE, TString::Format("maximum request header length of %d characters exeeded (linebreaks do not count against this limit)", HEADER_CHAR_LIMIT));

				arr_req = request_line->Split(':', 2U);
				EL_ERROR(arr_req.Count() != 2U, THttpProcessingException, EStatus::BAD_REQUEST, "invalid header field encountered");
				arr_req[0].Trim();
				arr_req[0].ToLower();
				EL_ERROR(request.header_fields.Get(arr_req[0]) != nullptr, THttpProcessingException, EStatus::BAD_REQUEST, "duplicate header field encountered");
				arr_req[1].Trim();
				request.header_fields.Add(std::move(arr_req[0]), std::move(arr_req[1]));
			}
			arr_req.Clear();
			EL_ERROR(request_line == nullptr, THttpProcessingException, EStatus::BAD_REQUEST, "header not correctly terminated by empty line");
			request.body = &source; // FIXME use limited source when content-length is provided

			EL_ERROR(request.method == EMethod::TRACE, THttpProcessingException, EStatus::METHOD_NOT_ALLOWED, "TRACE is not supported");

			// call handler
			response_t response;
			response.status = EStatus::INTERNAL_SERVER_ERROR;
			response.version = request.version;
			handler(request, response);

			if(request.method == EMethod::HEAD)
				response.body.reset(nullptr);

			TFile* file = nullptr;
			if(response.body != nullptr && response.header_fields.ContentLength() == NEG1 && (file = dynamic_cast<TFile*>(response.body.get())) != nullptr)
				response.header_fields.ContentLength(file->Size() - file->Offset());

			// send response
			response_in_progress = true;
			SendResponse(sink, response);

			return response.status;
		}
		catch(const IException& e1)
		{
			if(response_in_progress)
			{
				// just terminate the connection (in the next loop iteration EStatus::EOF will be returned immediately)
				source.Close();
				sink.Close();
			}
			else
			{
				const THttpProcessingException* const http_error = dynamic_cast<const THttpProcessingException*>(&e1);

				try
				{
					if(http_error != nullptr)
					{
						response_t response;
						response.status = http_error->status;
						response.version = EVersion::HTTP11;
						response.body = nullptr;
						response_in_progress = true;
						SendResponse(sink, response);
					}
					else
					{
						// HTTP 500
						response_t response;
						response.status = EStatus::INTERNAL_SERVER_ERROR;
						response.version = EVersion::HTTP11;
						response_in_progress = true;
						SendResponse(sink, response);
					}
				}
				catch(const IException&)
				{
					// this usually happens when the client closes the connection while we are still trying to inform it about the previous error
					source.Close();
					sink.Close();
				}

				if(http_error != nullptr)
					return http_error->status;
			}

			return EStatus::INTERNAL_SERVER_ERROR;
		}
	}

	void THttpServer::FiberMain()
	{
		TList<std::unique_ptr<TFiber>> handlers;
		u8_t cleanup_handlers = 0;
		TMemoryWaitable<u8_t> wait_cleanup(&cleanup_handlers, nullptr, 0xff);

		for(;;)
		{
			// accept new client
			std::unique_ptr<TTcpClient> tcp_client = this->tcp_server->AcceptClient();
			if(tcp_client == nullptr)
			{
				// cleanup
				for(ssys_t i = handlers.Count() - 1; i >= 0; i--)
					if(!handlers[i]->IsAlive())
					{
						auto e = handlers[i]->Join();
						if(e != nullptr)
							EL_FORWARD(*e.get(), TLogicException);

						handlers.Remove(i);
					}

				cleanup_handlers = 0;

				// wait
				TFiber::WaitForMany({ &this->tcp_server->OnClientConnect(), &wait_cleanup });
			}
			else
			{
				// start handler and handoff client
				handlers.MoveAppend(std::unique_ptr<TFiber>(new TFiber([this, tcp_client = std::move(tcp_client), &cleanup_handlers](){
					// TODO: add some kind of output buffer to prevent excessive amounts of small write()-syscalls

					// process all requests from client
					while(HandleSingleRequest(*tcp_client.get(), *tcp_client.get(), this->handler) != EStatus::EOF);

					// notify controller to cleanup handler
					cleanup_handlers = 1;
				})));
			}
		}
	}

	THttpServer::THttpServer(TTcpServer* const tcp_server, request_handler_t handler) :
		tcp_server(tcp_server),
		handler(handler),
		fiber(TFunction(this, &THttpServer::FiberMain), true)
	{
	}
}
