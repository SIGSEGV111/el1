#include "io_net_http.hpp"
#include "io_text.hpp"
#include "io_bcd.hpp"
#include "io_text_encoding_utf8.hpp"

#include <stdio.h>
#include <string.h>

#define IF_DEBUG_PRINTF(...) if(EL_UNLIKELY(DEBUG)) fprintf(stderr, __VA_ARGS__)

namespace el1::io::net::http
{
	using namespace stream;
	using namespace text;
	using namespace text::string;
	using namespace text::encoding;
	using namespace text::encoding::utf8;
	using namespace bcd;
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
	bool THttpServer::DEBUG = false;

	TString THttpProcessingException::Message() const
	{
		return msg;
	}

	IException* THttpProcessingException::Clone() const
	{
		return new THttpProcessingException(*this);
	}

	static EMethod MethodFromString(const TStringView str)
	{
		     if(str == U"GET") return EMethod::GET;
		else if(str == U"POST") return EMethod::POST;
		else if(str == U"HEAD") return EMethod::HEAD;
		else if(str == U"PUT") return EMethod::PUT;
		else if(str == U"PATCH") return EMethod::PATCH;
		else if(str == U"DELETE") return EMethod::DELETE;
		else if(str == U"TRACE") return EMethod::TRACE;
		else if(str == U"OPTIONS") return EMethod::OPTIONS;
		else if(str == U"CONNECT") return EMethod::CONNECT;
		else EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"unknown method");
	}

	static EVersion VersionFromString(const TStringView str)
	{
		     if(str == U"HTTP/1.1") return EVersion::HTTP11;
		else if(str == U"HTTP/1.0") return EVersion::HTTP10;
		else if(str == U"HTTP/2")   return EVersion::HTTP20;
		else if(str == U"HTTP/2.0") return EVersion::HTTP20;
		else EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"unknown http version");
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
			case EStatus::CREATED: return "Created";
			case EStatus::ACCEPTED: return "Accepted";
			case EStatus::NO_CONTENT: return "No Content";
			case EStatus::PARTIAL_CONTENT: return "Partial Content";
			case EStatus::BAD_REQUEST: return "Bad Request";
			case EStatus::UNAUTHORIZED: return "Unauthorized";
			case EStatus::FORBIDDEN: return "Forbidden";
			case EStatus::NOT_FOUND: return "Not Found";
			case EStatus::METHOD_NOT_ALLOWED: return "Method Not Allowed";
			case EStatus::CONFLICT: return "Conflict";
			case EStatus::RANGE_NOT_SATISFIABLE: return "Range Not Satisfiable";
			case EStatus::REQUEST_HEADER_FIELDS_TOO_LARGE: return "Request Header Fields Too Large";
			case EStatus::INTERNAL_SERVER_ERROR: return "Internal Server Error";
			case EStatus::EOF: break;
		}

		EL_THROW(TException, U"unsupported status code");
	}

	static const char* MethodToString(const EMethod method)
	{
		switch(method)
		{
			case EMethod::GET: return "GET";
			case EMethod::POST: return "POST";
			case EMethod::HEAD: return "HEAD";
			case EMethod::PUT: return "PUT";
			case EMethod::PATCH: return "PATCH";
			case EMethod::DELETE: return "DELETE";
			case EMethod::TRACE: return "TRACE";
			case EMethod::OPTIONS: return "OPTIONS";
			case EMethod::CONNECT: return "CONNECT";
		}

		EL_THROW(TLogicException);
	}

	static bool HeaderNameEquals(const TStringView a, const TStringView b)
	{
		TString aa(a);
		TString bb(b);
		aa.ToLower();
		bb.ToLower();
		return aa == bb;
	}

	static TString* FindHeaderField(THttpHeaderFields& fields, const TStringView name)
	{
		for(auto& field : fields.Items())
			if(HeaderNameEquals(field.key, name))
				return &field.value;
		return nullptr;
	}

	static const TString* FindHeaderField(const THttpHeaderFields& fields, const TStringView name)
	{
		for(const auto& field : fields.Items())
			if(HeaderNameEquals(field.key, name))
				return &field.value;
		return nullptr;
	}

	static void SetHeaderField(THttpHeaderFields& fields, TString name, TString value)
	{
		for(auto& field : fields.Items())
			if(HeaderNameEquals(field.key, name))
			{
				field.value = std::move(value);
				return;
			}

		fields.Add(std::move(name), std::move(value));
	}

	static bool RemoveHeaderField(THttpHeaderFields& fields, const TStringView name)
	{
		for(usys_t i = 0; i < fields.Items().Count(); i++)
			if(HeaderNameEquals(fields.Items()[i].key, name))
			{
				const TString key = fields.Items()[i].key;
				fields.Remove(key);
				return true;
			}
		return false;
	}

	static bool IsHeaderNameChar(const char32_t chr)
	{
		if(chr <= 0x20 || chr >= 0x7f)
			return false;

		switch(chr)
		{
			case '(': case ')': case '<': case '>': case '@':
			case ',': case ';': case ':': case '\\': case '"':
			case '/': case '[': case ']': case '?': case '=':
			case '{': case '}':
				return false;
		}
		return true;
	}

	static void ValidateHeaderField(const TStringView name, const TStringView value)
	{
		EL_ERROR(name.Length() == 0, TInvalidArgumentException, "header name", "HTTP header name must not be empty");
		for(const char32_t chr : name)
			EL_ERROR(!IsHeaderNameChar(chr), TInvalidArgumentException, "header name", "HTTP header name contains an invalid character");

		for(const char32_t chr : value)
			EL_ERROR(chr == '\r' || chr == '\n' || chr == 0, TInvalidArgumentException, "header value", "HTTP header value contains CR, LF or NUL");
	}

	static void ValidateRequestTarget(const TStringView target)
	{
		EL_ERROR(target.Length() == 0, TInvalidArgumentException, "url", "request URL must not be empty");
		for(const char32_t chr : target)
			EL_ERROR(chr <= 0x20 || chr == 0x7f, TInvalidArgumentException, "url", "HTTP request target contains whitespace or a control character");
	}

	static void ValidateDecodedRequestPath(const TStringView path)
	{
		for(const char32_t chr : path)
			EL_ERROR(chr < 0x20 || chr == 0x7f, TInvalidArgumentException, "url", "decoded HTTP request path contains a control character");
	}

	static bool HeaderHasToken(const TStringView value, const TStringView token)
	{
		TString token_lower(token);
		token_lower.ToLower();
		for(TString item : TString(value).Split(','))
		{
			item.Trim();
			item.ToLower();
			if(item == token_lower)
				return true;
		}
		return false;
	}

	static void WriteString(ISink<byte_t>& sink, const TStringView str)
	{
		auto cstr = str.MakeCStr();
		sink.WriteAll(reinterpret_cast<const byte_t*>(cstr.get()), strlen(cstr.get()));
	}

	static bool ReadByteBlocking(ISource<byte_t>& source, byte_t& byte)
	{
		for(;;)
		{
			if(source.Read(&byte, 1) == 1)
				return true;

			const IWaitable* const waitable = source.OnInputReady();
			if(waitable == nullptr)
				return false;
			waitable->WaitFor();
		}
	}

	static bool ReadHttpLine(ISource<byte_t>& source, TString& line, const usys_t limit = HEADER_CHAR_LIMIT)
	{
		line.chars.Clear();
		for(;;)
		{
			byte_t byte = 0;
			if(!ReadByteBlocking(source, byte))
				return line.Length() != 0;

			if(byte == '\n')
			{
				if(line.Length() != 0 && line[-1] == '\r')
					line.Cut(0, 1);
				return true;
			}

			EL_ERROR(line.Length() >= limit, TException, U"HTTP line exceeds configured limit");
			line += char32_t((u32_t)byte);
		}
	}

	static usys_t ReadSomeBlocking(ISource<byte_t>& source, byte_t* const buffer, const usys_t size)
	{
		for(;;)
		{
			const usys_t n = source.Read(buffer, size);
			if(n != 0)
				return n;

			const IWaitable* const waitable = source.OnInputReady();
			if(waitable == nullptr)
				return 0;
			waitable->WaitFor();
		}
	}

	static void PumpExactBlocking(ISource<byte_t>& source, ISink<byte_t>& sink, usys_t count)
	{
		byte_t buffer[4U * 1024U];
		while(count != 0)
		{
			const usys_t n_want = util::Min<usys_t>(count, sizeof(buffer));
			const usys_t n = ReadSomeBlocking(source, buffer, n_want);
			EL_ERROR(n == 0, TStreamDryException);
			sink.WriteAll(buffer, n);
			count -= n;
		}
	}

	static void PumpUntilEofBlocking(ISource<byte_t>& source, ISink<byte_t>& sink)
	{
		byte_t buffer[4U * 1024U];
		for(;;)
		{
			const usys_t n = ReadSomeBlocking(source, buffer, sizeof(buffer));
			if(n == 0)
				break;
			sink.WriteAll(buffer, n);
		}
	}

	static usys_t ParseHex(const TStringView str)
	{
		EL_ERROR(str.Length() == 0, TException, U"empty HTTP chunk size");
		usys_t value = 0;
		for(usys_t i = 0; i < str.Length(); i++)
		{
			const char32_t chr = str[i];
			u8_t digit;
			if(chr >= '0' && chr <= '9') digit = chr - '0';
			else if(chr >= 'a' && chr <= 'f') digit = chr - 'a' + 10;
			else if(chr >= 'A' && chr <= 'F') digit = chr - 'A' + 10;
			else EL_THROW(TException, U"invalid HTTP chunk size");
			EL_ERROR(value > (NEG1 - digit) / 16U, TException, U"HTTP chunk size overflow");
			value = value * 16U + digit;
		}
		return value;
	}

	class TChunkedRequestSource final : public ISource<byte_t>
	{
		private:
			enum class EState : u8_t
			{
				SIZE,
				DATA,
				DATA_CR,
				DATA_LF,
				TRAILER,
				DONE,
			};

			ISource<byte_t>& source;
			EState state = EState::SIZE;
			usys_t chunk_remaining = 0;
			usys_t trailer_size = 0;
			TString line;

			bool ReadByte(byte_t& byte)
			{
				const usys_t n = source.Read(&byte, 1);
				if(n != 0)
					return true;
				EL_ERROR(source.OnInputReady() == nullptr, THttpProcessingException, EStatus::BAD_REQUEST, U"unexpected EOF in chunked request body");
				return false;
			}

			bool ReadLine(TString& result)
			{
				for(;;)
				{
					byte_t byte;
					if(!ReadByte(byte))
						return false;
					if(byte == '\n')
					{
						EL_ERROR(line.Length() == 0 || line[-1] != '\r', THttpProcessingException, EStatus::BAD_REQUEST, U"invalid chunked request line ending");
						line.chars.Remove(-1);
						result = std::move(line);
						line = TString();
						return true;
					}
					EL_ERROR(line.Length() >= HEADER_CHAR_LIMIT, THttpProcessingException, EStatus::BAD_REQUEST, U"HTTP chunk line exceeds configured limit");
					line += char32_t((u32_t)byte);
				}
			}

			void ParseSize(const TStringView input)
			{
				const usys_t pos_extension = input.Find(';');
				TString str_size = pos_extension == NEG1 ? TString(input) : TString(input.SliceBE(0, pos_extension));
				str_size.Trim();
				try
				{
					chunk_remaining = ParseHex(str_size);
				}
				catch(const IException&)
				{
					EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"invalid HTTP chunk size");
				}
				state = chunk_remaining == 0 ? EState::TRAILER : EState::DATA;
			}

		public:
			explicit TChunkedRequestSource(ISource<byte_t>& source) : source(source) {}

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override
			{
				if(n_items_max == 0 || state == EState::DONE)
					return 0;

				for(;;)
				{
					switch(state)
					{
						case EState::SIZE:
						{
							TString size_line;
							if(!ReadLine(size_line))
								return 0;
							ParseSize(size_line);
							break;
						}

						case EState::DATA:
						{
							const usys_t n = source.Read(arr_items, util::Min(n_items_max, chunk_remaining));
							if(n == 0)
							{
								EL_ERROR(source.OnInputReady() == nullptr, THttpProcessingException, EStatus::BAD_REQUEST, U"unexpected EOF in HTTP chunk data");
								return 0;
							}
							chunk_remaining -= n;
							if(chunk_remaining == 0)
								state = EState::DATA_CR;
							return n;
						}

						case EState::DATA_CR:
						case EState::DATA_LF:
						{
							byte_t byte;
							if(!ReadByte(byte))
								return 0;
							const byte_t expected = state == EState::DATA_CR ? '\r' : '\n';
							EL_ERROR(byte != expected, THttpProcessingException, EStatus::BAD_REQUEST, U"invalid HTTP chunk terminator");
							state = state == EState::DATA_CR ? EState::DATA_LF : EState::SIZE;
							break;
						}

						case EState::TRAILER:
						{
							TString trailer;
							if(!ReadLine(trailer))
								return 0;
							if(trailer.Length() == 0)
							{
								state = EState::DONE;
								return 0;
							}
							trailer_size += trailer.Length();
							EL_ERROR(trailer_size > HEADER_CHAR_LIMIT || trailer.Find(':') == NEG1, THttpProcessingException, EStatus::BAD_REQUEST, U"invalid HTTP chunk trailer");
							break;
						}

						case EState::DONE:
							return 0;
					}
				}
			}

			bool Complete() const EL_GETTER
			{
				return state == EState::DONE;
			}

			const IWaitable* OnInputReady() const final override
			{
				return state == EState::DONE ? nullptr : source.OnInputReady();
			}
	};

	static void SendResponse(ISink<byte_t>& sink, THttpServer::response_t& response)
	{
		const char* const str_version = VersionToString(response.version);
		const char* const str_status_text = StatusToString(response.status);
		auto str_status_code = TString::Format(U"%d", (u16_t)response.status).MakeCStr();

		if(response.body == nullptr && response.header_fields.ContentLength() == NEG1)
			response.header_fields.ContentLength(0);
		else if(response.header_fields.ContentLength() == NEG1 && response.version != EVersion::HTTP10)
			response.header_fields.Set(U"Connection", U"close");

		for(const auto& field : response.header_fields.Items())
			ValidateHeaderField(field.key, field.value);

		sink.WriteAll((const byte_t*)str_version, strlen(str_version));
		sink.WriteAll((const byte_t*)" ", 1);
		sink.WriteAll((const byte_t*)str_status_code.get(), strlen(str_status_code.get()));
		sink.WriteAll((const byte_t*)" ", 1);
		sink.WriteAll((const byte_t*)str_status_text, strlen(str_status_text));
		sink.WriteAll((const byte_t*)"\r\n", 2);

		for(const auto& field : response.header_fields.Items())
		{
			auto str_key = field.key.MakeCStr();
			auto str_value = field.value.MakeCStr();

			sink.WriteAll((const byte_t*)str_key.get(), strlen(str_key.get()));
			sink.WriteAll((const byte_t*)": ", 2);
			sink.WriteAll((const byte_t*)str_value.get(), strlen(str_value.get()));
			sink.WriteAll((const byte_t*)"\r\n", 2);
		}

		sink.WriteAll((const byte_t*)"\r\n", 2);

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
		const TString* const value = FindHeaderField(*this, U"Content-Length");
		if(value == nullptr)
			return NEG1;

		const s64_t length = value->ToInteger();
		EL_ERROR(length < 0, TException, U"negative HTTP Content-Length");
		return (usys_t)length;
	}

	void THttpHeaderFields::ContentLength(const usys_t new_content_length)
	{
		SetHeaderField(*this, U"Content-Length", TString::Format(U"%d", new_content_length));
	}

	EStatus THttpServer::HandleSingleRequest(ISource<byte_t>& source, ISink<byte_t>& sink, request_handler_t handler, const ipport_t remote_address)
	{
		IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): @1\n");
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
			auto ds = ss.Map([](byte_t chr){ return char32_t((u32_t)chr); });
			auto lr = ds.Transform(TLineReader(HEADER_CHAR_LIMIT));

			IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): reading first line of request\n");
			const TString* request_line = lr.NextItem();
			if(request_line == nullptr)
			{
				IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): EOF\n");
				return EStatus::EOF;
			}

			EL_ERROR(request_line->Length() < 14U, THttpProcessingException, EStatus::BAD_REQUEST, U"request too short");
			sz_header += request_line->Length();

			auto arr_req = request_line->Split(' ', 3U);
			EL_ERROR(arr_req.Count() != 3U, THttpProcessingException, EStatus::BAD_REQUEST, U"request METHOD/URL/VERSION malformed");

			request_t request;
			request.remote_address = remote_address;
			request.method = MethodFromString(arr_req[0]);
			request.version = VersionFromString(arr_req[2]);
			try
			{
				ValidateRequestTarget(arr_req[1]);
			}
			catch(const IException&)
			{
				EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"invalid request target");
			}
			request.url = std::move(arr_req[1]);

			const usys_t pos_args = request.url.Find('?');
			if(pos_args != NEG1)
			{
				EL_ERROR(pos_args == 0, THttpProcessingException, EStatus::BAD_REQUEST, U"empty URU");
				TList<TString> arg_strs = request.url.SliceSL(pos_args + 1).Split('&');
				for(auto& s : arg_strs)
				{
					EL_ERROR(s.Length() == 0, THttpProcessingException, EStatus::BAD_REQUEST, U"empty request parameter");
					if(s.Contains('='))
					{
						auto kv = s.SplitKV('=');
						request.args.Add(UrlDecode(std::move(kv.key)), UrlDecode(std::move(kv.value)));
					}
					else
					{
						request.args.Add(UrlDecode(std::move(s)), U"");
					}
				}
				request.url = request.url.SliceBE(0, pos_args);
			}

			request.url = UrlDecode(std::move(request.url));
			try
			{
				ValidateDecodedRequestPath(request.url);
			}
			catch(const IException&)
			{
				EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"invalid decoded request path");
			}

			arr_req.Clear();

			IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): reading header values\n");
			while((request_line = lr.NextItem()) != nullptr && request_line->Length() > 0U)
			{
				sz_header += request_line->Length();
				EL_ERROR(sz_header > HEADER_CHAR_LIMIT, THttpProcessingException, EStatus::REQUEST_HEADER_FIELDS_TOO_LARGE, TString::Format(U"maximum request header length of %d characters exeeded (linebreaks do not count against this limit)", HEADER_CHAR_LIMIT));

				arr_req = request_line->Split(':', 2U);
				EL_ERROR(arr_req.Count() != 2U, THttpProcessingException, EStatus::BAD_REQUEST, U"invalid header field encountered");
				try
				{
					ValidateHeaderField(arr_req[0], arr_req[1]);
				}
				catch(const IException&)
				{
					EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"invalid header field encountered");
				}
				arr_req[0].ToLower();
				arr_req[1].Trim();
				TString* const existing_header = request.header_fields.Get(arr_req[0]);
				if(existing_header == nullptr)
				{
					request.header_fields.Add(std::move(arr_req[0]), std::move(arr_req[1]));
				}
				else
				{
					// RFC 9110 sections 5.2/5.3 define a repeated field's combined value as
					// the field-line values joined in order with comma+SP. Keep framing,
					// routing and credential fields strict to avoid ambiguous request
					// interpretation; Cookie is the one request field whose values are
					// conventionally combined using semicolon+SP instead.
					EL_ERROR(
						arr_req[0] == U"content-length" ||
						arr_req[0] == U"host" ||
						arr_req[0] == U"authorization" ||
						arr_req[0] == U"proxy-authorization",
						THttpProcessingException, EStatus::BAD_REQUEST, U"duplicate singleton header field encountered"
					);
					if(arr_req[0] == U"cookie")
						*existing_header += TStringView(U"; ");
					else
						*existing_header += TStringView(U", ");
					*existing_header += arr_req[1];
				}
			}
			arr_req.Clear();
			EL_ERROR(request_line == nullptr, THttpProcessingException, EStatus::BAD_REQUEST, U"header not correctly terminated by empty line");

			usys_t content_length;
			try
			{
				content_length = request.header_fields.ContentLength();
			}
			catch(const IException&)
			{
				EL_THROW(THttpProcessingException, EStatus::BAD_REQUEST, U"invalid Content-Length header");
			}
			const TString* const transfer_encoding = request.header_fields.Get(U"transfer-encoding");
			IF_DEBUG_PRINTF("got Content-Length %zu%s\n", (size_t)content_length, transfer_encoding != nullptr ? " with Transfer-Encoding" : "");
			EL_ERROR(transfer_encoding != nullptr && content_length != NEG1, THttpProcessingException, EStatus::BAD_REQUEST, U"request contains both Content-Length and Transfer-Encoding");

			TLimitSource<byte_t> limited_body(&ss, content_length);
			TChunkedRequestSource chunked(ss);
			if(transfer_encoding != nullptr)
			{
				TString encoding = *transfer_encoding;
				encoding.Trim();
				encoding.ToLower();
				EL_ERROR(encoding != U"chunked", THttpProcessingException, EStatus::BAD_REQUEST, U"unsupported HTTP request transfer encoding");
				request.body = &chunked;
			}
			else
			{
				request.body = content_length == NEG1 ? nullptr : &limited_body;
			}

			EL_ERROR(request.method == EMethod::TRACE, THttpProcessingException, EStatus::METHOD_NOT_ALLOWED, U"TRACE is not supported");

			// call handler
			response_t response;
			response.status = EStatus::INTERNAL_SERVER_ERROR;
			response.version = request.version;

			IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): calling handler\n");
			handler(request, response);
			IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): returned from handler\n");

			const bool request_body_consumed = transfer_encoding != nullptr ? chunked.Complete() : (content_length == NEG1 || limited_body.Remaining() == 0);
			const TString* const connection = request.header_fields.Get(U"connection");
			const bool request_close = connection != nullptr && HeaderHasToken(*connection, U"close");
			if(!request_body_consumed || request_close)
				response.header_fields.Set(U"Connection", U"close");

			if(request.method == EMethod::HEAD)
				response.body.reset(nullptr);

			TFile* file = nullptr;
			if(response.body != nullptr && response.header_fields.ContentLength() == NEG1 && (file = dynamic_cast<TFile*>(response.body.get())) != nullptr)
				response.header_fields.ContentLength(file->Size() - file->Offset());

			// Responses without a declared length and requests with an unread body are delimited by closing the connection.
			const bool close_after_response = !request_body_consumed || request_close || (response.body != nullptr && response.header_fields.ContentLength() == NEG1);

			// send response
			response_in_progress = true;
			SendResponse(sink, response);
			if(close_after_response)
			{
				source.Close();
				sink.Close();
			}

			IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): return response.status\n");
			return response.status;
		}
		catch(const IException& e1)
		{
			if(EL_UNLIKELY(DEBUG))
				e1.Print("THttpServer::HandleSingleRequest(): caught exception");

			if(response_in_progress)
			{
				IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): response already in progress => closing streams\n");
				// just terminate the connection (in the next loop iteration EStatus::EOF will be returned immediately)
				source.Close();
				sink.Close();
			}
			else
			{
				IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): generating proper error response\n");
				const THttpProcessingException* const http_error = dynamic_cast<const THttpProcessingException*>(&e1);

				try
				{
					if(http_error != nullptr)
					{
						response_t response;
						response.status = http_error->status;
						response.version = EVersion::HTTP11;
						response.body = nullptr;
						response.header_fields.Set(U"Connection", U"close");
						response_in_progress = true;
						SendResponse(sink, response);
					}
					else
					{
						// HTTP 500
						response_t response;
						response.status = EStatus::INTERNAL_SERVER_ERROR;
						response.version = EVersion::HTTP11;
						response.header_fields.Set(U"Connection", U"close");
						response_in_progress = true;
						SendResponse(sink, response);
					}

					source.Close();
					sink.Close();
				}
				catch(const IException&)
				{
					// this usually happens when the client closes the connection while we are still trying to inform it about the previous error
					source.Close();
					sink.Close();
				}

				if(http_error != nullptr)
				{
					IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): return http_error->status\n");
					return http_error->status;
				}
			}

			IF_DEBUG_PRINTF("THttpServer::HandleSingleRequest(): return INTERNAL_SERVER_ERROR\n");
			return EStatus::INTERNAL_SERVER_ERROR;
		}
	}

	void THttpServer::FiberMain()
	{
		IF_DEBUG_PRINTF("THttpServer::FiberMain(): enter\n");
		TList<std::unique_ptr<TFiber>> handlers;
		u8_t cleanup_handlers = 0;
		TMemoryWaitable<u8_t> wait_cleanup(&cleanup_handlers, nullptr, 0xff);

		for(;;)
		{
			// accept new client
			IF_DEBUG_PRINTF("calling AcceptClient()\n");
			std::unique_ptr<IStreamClient> stream_client = this->stream_server->AcceptStreamClient();
			if(stream_client == nullptr)
			{
				IF_DEBUG_PRINTF("no new client waiting, just cleaning up\n");
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
				IF_DEBUG_PRINTF("waiting ...\n");
				TFiber::WaitForMany({ &this->stream_server->OnClientConnect(), &wait_cleanup });
			}
			else
			{
				IF_DEBUG_PRINTF("accepted new client, spawning handler\n");
				// start handler and handoff client
				handlers.MoveAppend(New<TFiber>([this, stream_client = std::move(stream_client), &cleanup_handlers](){
					// TODO: add some kind of output buffer to prevent excessive amounts of small write()-syscalls

					// process all requests from client
					while(HandleSingleRequest(*stream_client.get(), *stream_client.get(), this->handler, stream_client->RemoteAddress()) != EStatus::EOF);

					// notify controller to cleanup handler
					cleanup_handlers = 1;
				}));
			}
		}
	}

	THttpServer::THttpServer(IStreamServer* const stream_server, request_handler_t handler) :
		stream_server(stream_server),
		handler(handler),
		fiber(TFunction<void>(this, &THttpServer::FiberMain), true)
	{
		EL_ERROR(stream_server == nullptr, TInvalidArgumentException, "stream_server", "stream_server must not be null");
		IF_DEBUG_PRINTF("THttpServer constructor\n");
	}

	THttpServer::THttpServer(TTcpServer* const tcp_server, request_handler_t handler) :
		THttpServer(static_cast<IStreamServer*>(tcp_server), std::move(handler))
	{
	}

	THttpServer::~THttpServer()
	{
		IF_DEBUG_PRINTF("THttpServer destructor\n");
	}

	static void WriteChunkedBody(ISource<byte_t>& source, ISink<byte_t>& sink)
	{
		byte_t buffer[4U * 1024U];
		for(;;)
		{
			const usys_t n = ReadSomeBlocking(source, buffer, sizeof(buffer));
			if(n == 0)
				break;

			char chunk_header[32];
			const int n_header = snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", (size_t)n);
			EL_ERROR(n_header <= 0 || (usys_t)n_header >= sizeof(chunk_header), TLogicException);
			sink.WriteAll(reinterpret_cast<const byte_t*>(chunk_header), n_header);
			sink.WriteAll(buffer, n);
			sink.WriteAll(reinterpret_cast<const byte_t*>("\r\n"), 2);
		}

		sink.WriteAll(reinterpret_cast<const byte_t*>("0\r\n\r\n"), 5);
	}

	static void AddResponseHeader(THttpClient::response_header_t& response, TString name, TString value)
	{
		response.header_lines.Append({ name, value });
		TString* const existing = FindHeaderField(response.header_fields, name);
		if(existing == nullptr)
		{
			response.header_fields.Add(std::move(name), std::move(value));
		}
		else if(!HeaderNameEquals(name, U"set-cookie"))
		{
			*existing += U", ";
			*existing += value;
		}
	}


	static void ParseHeaderLine(const TStringView line, TString& name, TString& value)
	{
		const usys_t pos_colon = line.Find(':');
		EL_ERROR(pos_colon == NEG1 || pos_colon == 0, TException, U"invalid HTTP header line");
		name = TString(line.SliceBE(0, pos_colon));
		value = TString(line.SliceSL(pos_colon + 1));
		name.Trim();
		value.Trim();
		EL_ERROR(name.Length() == 0, TException, U"empty HTTP header name");
	}

	static void ReadChunkedBody(ISource<byte_t>& source, ISink<byte_t>& sink, THttpClient::response_t& response)
	{
		for(;;)
		{
			TString line;
			EL_ERROR(!ReadHttpLine(source, line), TStreamDryException);
			const usys_t pos_extension = line.Find(';');
			TString str_size = pos_extension == NEG1 ? line : line.SliceBE(0, pos_extension);
			str_size.Trim();
			const usys_t chunk_size = ParseHex(str_size);

			if(chunk_size == 0)
			{
				for(;;)
				{
					EL_ERROR(!ReadHttpLine(source, line), TStreamDryException);
					if(line.Length() == 0)
						return;

					TString name;
					TString value;
					ParseHeaderLine(line, name, value);
					AddResponseHeader(response, std::move(name), std::move(value));
				}
			}

			PumpExactBlocking(source, sink, chunk_size);
			byte_t terminator[2];
			source.ReadAll(terminator, 2);
			EL_ERROR(terminator[0] != '\r' || terminator[1] != '\n', TException, U"invalid HTTP chunk terminator");
		}
	}

	static TString RequestPath(const TStringView url)
	{
		const usys_t pos_query = url.Find('?');
		TString path = pos_query == NEG1 ? TString(url) : TString(url.SliceBE(0, pos_query));
		if(path.Length() == 0 || path[0] != '/')
			return U"/";
		return path;
	}

	static TString DefaultCookiePath(const TStringView request_path)
	{
		if(request_path.Length() == 0 || request_path[0] != '/')
			return U"/";

		const usys_t pos_slash = request_path.Find('/', -1, true);
		if(pos_slash == NEG1 || pos_slash == 0)
			return U"/";
		return request_path.SliceBE(0, pos_slash);
	}

	static bool DomainMatches(const TStringView host_input, const TStringView domain_input)
	{
		TString host(host_input);
		TString domain(domain_input);
		host.ToLower();
		domain.ToLower();
		while(domain.Length() != 0 && domain[0] == '.')
			domain.chars.Remove(0);

		if(host == domain)
			return true;
		if(host.Length() <= domain.Length())
			return false;

		const usys_t offset = host.Length() - domain.Length();
		return offset > 0 && host[offset - 1] == '.' && host.SliceSL(offset) == domain;
	}

	static s64_t DaysFromCivil(int year, const unsigned month, const unsigned day)
	{
		year -= month <= 2;
		const int era = (year >= 0 ? year : year - 399) / 400;
		const unsigned year_of_era = (unsigned)(year - era * 400);
		const unsigned adjusted_month = month > 2 ? month - 3U : month + 9U;
		const unsigned day_of_year = (153U * adjusted_month + 2U) / 5U + day - 1U;
		const unsigned day_of_era = year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
		return (s64_t)era * 146097 + (s64_t)day_of_era - 719468;
	}

	static bool AsciiEqualsIgnoreCase(const char* a, const char* b)
	{
		while(*a != '\0' && *b != '\0')
		{
			char ca = *a++;
			char cb = *b++;
			if(ca >= 'A' && ca <= 'Z')
				ca = (char)(ca - 'A' + 'a');
			if(cb >= 'A' && cb <= 'Z')
				cb = (char)(cb - 'A' + 'a');
			if(ca != cb)
				return false;
		}
		return *a == *b;
	}

	static bool ParseCookieExpires(const TStringView value, s64_t& unix_time)
	{
		auto cstr = value.MakeCStr();
		char weekday[4] = {};
		char month_name[4] = {};
		char zone[8] = {};
		int day = 0;
		int year = 0;
		int hour = 0;
		int minute = 0;
		int second = 0;
		if(sscanf(cstr.get(), "%3[^,], %d %3s %d %d:%d:%d %7s", weekday, &day, month_name, &year, &hour, &minute, &second, zone) != 8)
			return false;

		static const char* const MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
		unsigned month = 0;
		for(unsigned i = 0; i < 12; i++)
			if(AsciiEqualsIgnoreCase(month_name, MONTHS[i]))
			{
				month = i + 1;
				break;
			}

		if(month == 0 || day < 1 || day > 31 || year < 1601 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60 || !AsciiEqualsIgnoreCase(zone, "GMT"))
			return false;

		unix_time = DaysFromCivil(year, month, (unsigned)day) * 86400 + hour * 3600 + minute * 60 + second;
		return true;
	}

	static bool CookiePathMatches(const TStringView request_path, const TStringView cookie_path)
	{
		if(cookie_path == U"/")
			return true;
		if(!request_path.BeginsWith(cookie_path))
			return false;
		if(request_path.Length() == cookie_path.Length())
			return true;
		if(cookie_path.Length() != 0 && cookie_path[-1] == '/')
			return true;
		return request_path[cookie_path.Length()] == '/';
	}

	const TString* THttpClient::response_header_t::FindHeader(const TStringView name) const
	{
		for(const auto& field : header_lines)
			if(HeaderNameEquals(field.key, name))
				return &field.value;
		return nullptr;
	}

	TList<TString> THttpClient::response_header_t::FindHeaders(const TStringView name) const
	{
		TList<TString> values;
		for(const auto& field : header_lines)
			if(HeaderNameEquals(field.key, name))
				values.Append(field.value);
		return values;
	}

	THttpClient::THttpClient(TString host, const port_t port) :
		host(std::move(host)),
		port(port),
		use_tls(false)
	{
		EL_ERROR(this->host.Length() == 0, TInvalidArgumentException, "host", "host must not be empty");
		EL_ERROR(port == 0, TInvalidArgumentException, "port", "port must not be zero");
	}

	THttpClient::THttpClient(TString host, const port_t port, tls::client_config_t tls_config) :
		host(std::move(host)),
		port(port),
		use_tls(true),
		tls_config(std::move(tls_config))
	{
		EL_ERROR(this->host.Length() == 0, TInvalidArgumentException, "host", "host must not be empty");
		EL_ERROR(port == 0, TInvalidArgumentException, "port", "port must not be zero");
	}

	void THttpClient::Connect()
	{
		if(connection != nullptr)
			return;

		if(use_tls)
			connection.reset(new tls::TClient(host, port, tls_config));
		else
			connection.reset(new TTcpClient(host, port));
	}

	void THttpClient::SetHeader(TString name, TString value)
	{
		ValidateHeaderField(name, value);
		SetHeaderField(request_headers, std::move(name), std::move(value));
	}

	const TString* THttpClient::FindHeader(const TStringView name) const
	{
		return FindHeaderField(request_headers, name);
	}

	bool THttpClient::RemoveHeader(const TStringView name)
	{
		return RemoveHeaderField(request_headers, name);
	}

	void THttpClient::Close()
	{
		if(connection != nullptr)
		{
			connection->Close();
			connection.reset();
		}
	}

	void THttpClient::ProcessSetCookie(const TStringView value, const TStringView request_path)
	{
		TList<TString> parts = TString(value).Split(';');
		if(parts.Count() == 0)
			return;

		parts[0].Trim();
		const usys_t pos_equals = parts[0].Find('=');
		if(pos_equals == NEG1 || pos_equals == 0)
			return;

		cookie_t cookie;
		cookie.name = parts[0].SliceBE(0, pos_equals);
		cookie.value = parts[0].SliceSL(pos_equals + 1);
		cookie.name.Trim();
		cookie.value.Trim();
		cookie.domain = host;
		cookie.domain.ToLower();
		cookie.path = DefaultCookiePath(request_path);

		bool delete_cookie = false;
		bool has_max_age = false;
		for(usys_t i = 1; i < parts.Count(); i++)
		{
			parts[i].Trim();
			if(parts[i].Length() == 0)
				continue;

			const usys_t pos_attr_equals = parts[i].Find('=');
			TString attr_name = pos_attr_equals == NEG1 ? parts[i] : parts[i].SliceBE(0, pos_attr_equals);
			TString attr_value = pos_attr_equals == NEG1 ? TString() : parts[i].SliceSL(pos_attr_equals + 1);
			attr_name.Trim();
			attr_name.ToLower();
			attr_value.Trim();

			if(attr_name == U"domain")
			{
				attr_value.ToLower();
				while(attr_value.Length() != 0 && attr_value[0] == '.')
					attr_value.chars.Remove(0);
				if(attr_value.Length() == 0 || !DomainMatches(host, attr_value))
					return;
				cookie.domain = std::move(attr_value);
				cookie.host_only = false;
			}
			else if(attr_name == U"path")
			{
				if(attr_value.Length() != 0 && attr_value[0] == '/')
					cookie.path = std::move(attr_value);
			}
			else if(attr_name == U"max-age")
			{
				try
				{
					const s64_t seconds = attr_value.ToInteger();
					has_max_age = true;
					delete_cookie = seconds <= 0;
					if(seconds > 0)
						cookie.expires_unix = system::time::TTime::Now().Seconds() + seconds;
				}
				catch(const IException&)
				{
				}
			}
			else if(attr_name == U"expires" && !has_max_age)
			{
				s64_t expires_unix = -1;
				if(ParseCookieExpires(attr_value, expires_unix))
				{
					cookie.expires_unix = expires_unix;
					if(expires_unix <= system::time::TTime::Now().Seconds())
						delete_cookie = true;
				}
			}
			else if(attr_name == U"secure")
			{
				cookie.secure = true;
			}
			else if(attr_name == U"httponly")
			{
				cookie.http_only = true;
			}
		}

		for(ssys_t i = cookies.Count() - 1; i >= 0; i--)
			if(cookies[i].name == cookie.name && cookies[i].domain == cookie.domain && cookies[i].path == cookie.path)
			{
				cookies.Remove(i);
				break;
			}

		if(!delete_cookie)
			cookies.Append(std::move(cookie));
	}

	TString THttpClient::BuildCookieHeader(const TStringView request_path)
	{
		const s64_t now = system::time::TTime::Now().Seconds();
		TString result;

		for(ssys_t i = cookies.Count() - 1; i >= 0; i--)
			if(cookies[i].expires_unix >= 0 && cookies[i].expires_unix <= now)
				cookies.Remove(i);

		for(const cookie_t& cookie : cookies)
		{
			const bool domain_match = cookie.host_only ? HeaderNameEquals(host, cookie.domain) : DomainMatches(host, cookie.domain);
			if(!domain_match || !CookiePathMatches(request_path, cookie.path) || (cookie.secure && !use_tls))
				continue;

			if(result.Length() != 0)
				result += U"; ";
			result += cookie.name;
			result += U"=";
			result += cookie.value;
		}

		return result;
	}

	THttpClient::response_t THttpClient::Get(TString url, ISink<byte_t>* const response_body_sink, const usys_t body_limit)
	{
		request_t request;
		request.method = EMethod::GET;
		request.url = std::move(url);
		return Request(std::move(request), response_body_sink, body_limit);
	}

	std::unique_ptr<ISource<byte_t>> THttpClient::Get(TString url, response_header_t* const response_header, const usys_t body_limit)
	{
		response_t response = Get(std::move(url), static_cast<ISink<byte_t>*>(nullptr), body_limit);
		if(response_header != nullptr)
			*response_header = std::move(static_cast<response_header_t&>(response));
		return New<TListSource<byte_t>, ISource<byte_t>>(std::move(response.body));
	}

	THttpClient::response_t THttpClient::Post(TString url, array_t<const byte_t> body, THttpHeaderFields header_fields, const usys_t body_limit)
	{
		TArraySource<byte_t> source(body);
		return Post(std::move(url), source, body.Count(), std::move(header_fields), body_limit);
	}

	THttpClient::response_t THttpClient::Post(TString url, ISource<byte_t>& body, THttpHeaderFields header_fields, const usys_t body_limit)
	{
		return Post(std::move(url), body, NEG1, std::move(header_fields), body_limit);
	}

	THttpClient::response_t THttpClient::Post(TString url, ISource<byte_t>& body, const usys_t content_length, THttpHeaderFields header_fields, const usys_t body_limit)
	{
		request_t request;
		request.method = EMethod::POST;
		request.url = std::move(url);
		request.header_fields = std::move(header_fields);
		request.body = &body;
		request.content_length = content_length;
		return Request(std::move(request), nullptr, body_limit);
	}

	THttpClient::response_t THttpClient::Request(request_t request, ISink<byte_t>* const response_body_sink, const usys_t body_limit)
	{
		ValidateRequestTarget(request.url);
		EL_ERROR(request.body == nullptr && request.content_length != 0, TInvalidArgumentException, "content_length", "non-zero content length requires a request body");

		try
		{
			THttpHeaderFields headers = request_headers;
			for(const auto& field : request.header_fields.Items())
				SetHeaderField(headers, field.key, field.value);

			if(FindHeaderField(headers, U"Host") == nullptr)
			{
				const bool default_port = (!use_tls && port == 80) || (use_tls && port == 443);
				SetHeaderField(headers, U"Host", default_port ? host : TString::Format(U"%s:%d", host, port));
			}

			const TString request_path = RequestPath(request.url);
			TString jar_cookie = BuildCookieHeader(request_path);
			if(jar_cookie.Length() != 0)
			{
				TString* const explicit_cookie = FindHeaderField(headers, U"Cookie");
				if(explicit_cookie != nullptr && explicit_cookie->Length() != 0)
				{
					jar_cookie += U"; ";
					jar_cookie += *explicit_cookie;
				}
				SetHeaderField(headers, U"Cookie", std::move(jar_cookie));
			}

			if(request.body != nullptr)
			{
				if(request.content_length == NEG1)
				{
					RemoveHeaderField(headers, U"Content-Length");
					SetHeaderField(headers, U"Transfer-Encoding", U"chunked");
				}
				else
				{
					RemoveHeaderField(headers, U"Transfer-Encoding");
					SetHeaderField(headers, U"Content-Length", TString::Format(U"%d", request.content_length));
				}
			}

			for(const auto& field : headers.Items())
				ValidateHeaderField(field.key, field.value);

			Connect();
			connection->WriteAll(reinterpret_cast<const byte_t*>(MethodToString(request.method)), strlen(MethodToString(request.method)));
			connection->WriteAll(reinterpret_cast<const byte_t*>(" "), 1);
			WriteString(*connection, request.url);
			connection->WriteAll(reinterpret_cast<const byte_t*>(" HTTP/1.1\r\n"), 11);

			for(const auto& field : headers.Items())
			{
				WriteString(*connection, field.key);
				connection->WriteAll(reinterpret_cast<const byte_t*>(": "), 2);
				WriteString(*connection, field.value);
				connection->WriteAll(reinterpret_cast<const byte_t*>("\r\n"), 2);
			}
			connection->WriteAll(reinterpret_cast<const byte_t*>("\r\n"), 2);

			if(request.body != nullptr)
			{
				if(request.content_length == NEG1)
					WriteChunkedBody(*request.body, *connection);
				else
					PumpExactBlocking(*request.body, *connection, request.content_length);
			}
			connection->Flush();

			response_t response;
			for(;;)
			{
				TString line;
				EL_ERROR(!ReadHttpLine(*connection, line), TStreamDryException);
				TList<TString> status_parts = line.Split(' ', 3);
				EL_ERROR(status_parts.Count() < 2, TException, U"invalid HTTP status line");
				response.version = VersionFromString(status_parts[0]);
				const s64_t status_code = status_parts[1].ToInteger();
				EL_ERROR(status_code < 100 || status_code > 999, TException, U"invalid HTTP status code");
				response.status = static_cast<EStatus>((u16_t)status_code);
				response.header_fields.Clear();
				response.header_lines.Clear();

				usys_t header_size = line.Length();
				for(;;)
				{
					EL_ERROR(!ReadHttpLine(*connection, line), TStreamDryException);
					if(line.Length() == 0)
						break;
					header_size += line.Length();
					EL_ERROR(header_size > HEADER_CHAR_LIMIT, TException, U"HTTP response header exceeds configured limit");

					TString name;
					TString value;
					ParseHeaderLine(line, name, value);
					AddResponseHeader(response, std::move(name), std::move(value));
				}

				if(status_code < 100 || status_code >= 200 || status_code == 101)
					break;
			}

			for(const TString& set_cookie : response.FindHeaders(U"Set-Cookie"))
				ProcessSetCookie(set_cookie, request_path);

			TListSink<byte_t> buffer_sink(&response.body);
			ISink<byte_t>* const raw_body_sink = response_body_sink == nullptr ? static_cast<ISink<byte_t>*>(&buffer_sink) : response_body_sink;
			TLimitSink<byte_t> limit_sink(raw_body_sink, body_limit);
			ISink<byte_t>* const body_sink = &limit_sink;
			const u16_t status_code = (u16_t)response.status;
			const bool no_body = request.method == EMethod::HEAD || (status_code >= 100 && status_code < 200) || status_code == 204 || status_code == 304;
			bool reusable = true;

			if(!no_body)
			{
				const TString* const transfer_encoding = response.FindHeader(U"Transfer-Encoding");
				const usys_t content_length = response.header_fields.ContentLength();
				if(body_limit != NEG1 && transfer_encoding == nullptr && content_length != NEG1)
					EL_ERROR(content_length > body_limit, TException, U"HTTP response body exceeds configured limit");
				if(transfer_encoding != nullptr)
				{
					TString encoding = *transfer_encoding;
					encoding.Trim();
					encoding.ToLower();
					EL_ERROR(encoding != U"chunked", TException, U"unsupported HTTP transfer encoding");
					ReadChunkedBody(*connection, *body_sink, response);
				}
				else if(content_length != NEG1)
				{
					PumpExactBlocking(*connection, *body_sink, content_length);
				}
				else
				{
					PumpUntilEofBlocking(*connection, *body_sink);
					reusable = false;
				}
			}

			const TString* const connection_header = response.FindHeader(U"Connection");
			if(connection_header != nullptr && HeaderHasToken(*connection_header, U"close"))
				reusable = false;
			if(response.version == EVersion::HTTP10 && (connection_header == nullptr || !HeaderHasToken(*connection_header, U"keep-alive")))
				reusable = false;

			if(!reusable)
				Close();
			return response;
		}
		catch(...)
		{
			Close();
			throw;
		}
	}


	TString UrlDecode(const TStringView input)
	{
		TString url(input);
		if(url.Length() == 0) return url;
		const TString hex = U"0123456789abcdef";
		for(usys_t i = 0; i + 2 < url.Length(); i++)
			if(url[i] == '%')
			{
				auto str = url.SliceSL(i + 1, 2).ToLower().Reverse();
				url.chars[i] = TBCD::FromString(str, hex).ToUnsignedInt();
				url.chars.Remove(i + 1, 2);
			}
		return url.chars.Pipe().Map([](char32_t chr){ return (byte_t)chr; }).Transform(TUTF8Decoder()).Collect();
	}

	TString UrlEncode(const TStringView url)
	{
		EL_NOT_IMPLEMENTED;
	}
}

#undef IF_DEBUG_PRINTF
