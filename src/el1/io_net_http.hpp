#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_text_string.hpp"
#include "system_task.hpp"
#include "system_waitable.hpp"
#include "util_function.hpp"
#include "io_net_ip.hpp"
#include "io_net_tls.hpp"
#include "system_time.hpp"

#undef EOF
#undef OK

namespace el1::io::net::http
{
	using namespace types;

	enum class EStatus : u16_t
	{
		EOF = 0,	// used by HandleSingleRequest() to indicate that the ISource reported EOF

		OK = 200,
		CREATED = 201,
		ACCEPTED = 202,
		NO_CONTENT = 204,
		PARTIAL_CONTENT = 206,
		BAD_REQUEST = 400,
		UNAUTHORIZED = 401,
		FORBIDDEN = 403,
		NOT_FOUND = 404,
		METHOD_NOT_ALLOWED = 405,
		CONFLICT = 409,
		RANGE_NOT_SATISFIABLE = 416,
		REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
		INTERNAL_SERVER_ERROR = 500,
	};

	struct THttpProcessingException : error::IException
	{
		const EStatus status;
		const text::string::TString msg;

		text::string::TString Message() const final override;
		IException* Clone() const override;

		THttpProcessingException(const EStatus status, const text::string::TString msg) : status(status), msg(msg) {}
	};

	class THttpHeaderFields : public collection::map::TSortedMap<text::string::TString, text::string::TString>
	{
		public:
			usys_t ContentLength() const EL_GETTER;
			void ContentLength(const usys_t) EL_SETTER;
	};

	enum class EMethod : u8_t
	{
		GET,
		POST,
		HEAD,
		PUT,
		PATCH,
		DELETE,
		TRACE,
		OPTIONS,
		CONNECT
	};

	enum class EVersion : u8_t
	{
		HTTP10 = 10,
		HTTP11 = 11,
		HTTP20 = 20,
		HTTP30 = 30,
	};

	struct request_meta_t
	{
		EMethod method;
		EVersion version;
		text::string::TString url;
		collection::map::TSortedMap<text::string::TString, text::string::TString> args;
		THttpHeaderFields header_fields;
	};

	struct response_meta_t
	{
		EStatus status;
		EVersion version;
		THttpHeaderFields header_fields;
	};

	class IHttpRequestBody : public stream::ISource<byte_t>
	{
		public:
			virtual bool Complete() const EL_GETTER = 0;
			virtual usys_t Remaining() const EL_GETTER = 0;
			virtual const THttpHeaderFields& Trailers() const EL_GETTER = 0;
			using stream::ISource<byte_t>::Discard;
			virtual void Discard() = 0;
	};

	class THttpRequestBody final : public IHttpRequestBody
	{
		friend class THttpRequestDecoder;

		protected:
			enum class EEncoding : u8_t
			{
				NONE,
				CONTENT_LENGTH,
				CHUNKED,
			};

			enum class EChunkState : u8_t
			{
				SIZE,
				DATA,
				DATA_CR,
				DATA_LF,
				TRAILER,
				DONE,
			};

			stream::ISource<byte_t>* source = nullptr;
			EEncoding encoding = EEncoding::NONE;
			EChunkState chunk_state = EChunkState::DONE;
			usys_t n_remaining = 0;
			usys_t trailer_size = 0;
			text::string::TString line;
			THttpHeaderFields trailers;

			void Reset(stream::ISource<byte_t>* source, const usys_t content_length, const bool chunked);
			bool ReadByte(byte_t& byte);
			bool ReadLine(text::string::TString& result);
			void ParseChunkSize(text::string::TStringView line);

		public:
			using stream::ISource<byte_t>::Discard;

			bool Complete() const override EL_GETTER;
			usys_t Remaining() const override EL_GETTER;
			const THttpHeaderFields& Trailers() const override EL_GETTER { return trailers; }
			void Discard() override;

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnInputReady() const final override;
	};

	struct THttpRequest : request_meta_t
	{
		ip::ipport_t remote_address;
		IHttpRequestBody* body = nullptr;

		bool KeepAlive() const EL_GETTER;
	};

	class THttpRequestDecoder final : public stream::ISource<THttpRequest>
	{
		public:
			using request_t = THttpRequest;

		protected:
			enum class EState : u8_t
			{
				REQUEST_LINE,
				HEADERS,
				EOF_REACHED,
			};

			stream::ISource<byte_t>* const source;
			const ip::ipport_t remote_address;
			const usys_t header_char_limit;
			EState state = EState::REQUEST_LINE;
			text::string::TString line;
			request_t request;
			THttpRequestBody body;
			usys_t header_size = 0;
			bool body_outstanding = false;

			enum class ELineResult : u8_t { COMPLETE, BLOCKED, EOF_REACHED };
			ELineResult ReadLine();
			void ParseRequestLine();
			void ParseHeaderLine();
			void FinishHeaders();

		public:
			explicit THttpRequestDecoder(stream::ISource<byte_t>& source, const ip::ipport_t remote_address = {}, const usys_t header_char_limit = 8192U);

			usys_t Read(request_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnInputReady() const final override;
			THttpRequestBody* ActiveBody() EL_GETTER { return body_outstanding ? &body : nullptr; }
	};

	class THttpResponseEncoder
	{
		public:
			struct response_t : response_meta_t
			{
				std::unique_ptr<stream::ISource<byte_t>> body;
			};

		protected:
			stream::ISink<byte_t>* const sink;

		public:
			explicit THttpResponseEncoder(stream::ISink<byte_t>& sink) : sink(&sink) {}

			void WriteResponse(response_t& response, const bool suppress_body = false);
			const system::waitable::IWaitable* OnOutputReady() const { return sink->OnOutputReady(); }
	};

	class THttpServer
	{
		public:
			enum class EProtocol : u8_t
			{
				AUTO,
				HTTP1,
				HTTP2,
			};

			static bool DEBUG;

			using request_t = THttpRequestDecoder::request_t;
			using response_t = THttpResponseEncoder::response_t;
			using request_handler_t = util::function::TFunction<void, const request_t&, response_t&>;

		protected:
			ip::IStreamServer* const stream_server;
			request_handler_t handler;
			const EProtocol protocol;
			system::task::TFiber fiber;

			void FiberMain();

		public:
			static EStatus HandleSingleRequest(
				stream::ISource<byte_t>&,
				stream::ISink<byte_t>&,
				request_handler_t handler,
				const ip::ipport_t remote_address = ip::ipport_t{}
			);

			static void HandleHttp2Connection(
				stream::ISource<byte_t>& source,
				stream::ISink<byte_t>& sink,
				request_handler_t handler,
				const ip::ipport_t remote_address = ip::ipport_t{}
			);

			THttpServer(ip::IStreamServer* const stream_server, request_handler_t handler, const EProtocol protocol = EProtocol::AUTO);
			THttpServer(ip::TTcpServer* const tcp_server, request_handler_t handler, const EProtocol protocol = EProtocol::AUTO);
			~THttpServer();
	};

	class THttpClient
	{
		public:
			struct cookie_t
			{
				text::string::TString name;
				text::string::TString value;
				text::string::TString domain;
				text::string::TString path;
				s64_t expires_unix = -1;
				bool secure = false;
				bool http_only = false;
				bool host_only = true;
			};

			struct request_t
			{
				EMethod method = EMethod::GET;
				text::string::TString url = U"/";
				THttpHeaderFields header_fields;
				stream::ISource<byte_t>* body = nullptr;
				usys_t content_length = 0;
			};

			struct response_header_t : response_meta_t
			{
				using header_line_t = kv_pair_tt<text::string::TString, text::string::TString>;

				collection::list::TList<header_line_t> header_lines;

				const text::string::TString* FindHeader(const text::string::TStringView name) const EL_GETTER;
				collection::list::TList<text::string::TString> FindHeaders(const text::string::TStringView name) const EL_GETTER;
			};

			struct response_t : response_header_t
			{
				collection::list::TList<byte_t> body;
			};

			static constexpr usys_t DEFAULT_RESPONSE_BODY_LIMIT = 16U * 1024U * 1024U;

		protected:
			text::string::TString host;
			ip::port_t port;
			bool use_tls;
			tls::client_config_t tls_config;
			std::unique_ptr<ip::IStreamClient> connection;
			collection::list::TList<cookie_t> cookies;

			void Connect();
			void ProcessSetCookie(const text::string::TStringView value, const text::string::TStringView request_path);
			text::string::TString BuildCookieHeader(const text::string::TStringView request_path);

		public:
			THttpHeaderFields request_headers;

			THttpClient(text::string::TString host, const ip::port_t port = 80);
			THttpClient(text::string::TString host, const ip::port_t port, tls::client_config_t tls_config);
			THttpClient(THttpClient&&) noexcept = default;
			THttpClient(const THttpClient&) = delete;

			response_t Request(request_t request, stream::ISink<byte_t>* const response_body_sink = nullptr, const usys_t body_limit = DEFAULT_RESPONSE_BODY_LIMIT);
			response_t Get(text::string::TString url, stream::ISink<byte_t>* const response_body_sink = nullptr, const usys_t body_limit = DEFAULT_RESPONSE_BODY_LIMIT);
			std::unique_ptr<stream::ISource<byte_t>> Get(text::string::TString url, response_header_t* const response_header, const usys_t body_limit = DEFAULT_RESPONSE_BODY_LIMIT);

			response_t Post(
				text::string::TString url,
				collection::list::array_t<const byte_t> body,
				THttpHeaderFields header_fields = {},
				const usys_t body_limit = DEFAULT_RESPONSE_BODY_LIMIT
			);

			response_t Post(
				text::string::TString url,
				stream::ISource<byte_t>& body,
				THttpHeaderFields header_fields = {},
				const usys_t body_limit = DEFAULT_RESPONSE_BODY_LIMIT
			);

			response_t Post(
				text::string::TString url,
				stream::ISource<byte_t>& body,
				const usys_t content_length,
				THttpHeaderFields header_fields = {},
				const usys_t body_limit = DEFAULT_RESPONSE_BODY_LIMIT
			);

			void SetHeader(text::string::TString name, text::string::TString value);
			const text::string::TString* FindHeader(const text::string::TStringView name) const EL_GETTER;
			bool RemoveHeader(const text::string::TStringView name);

			const collection::list::TList<cookie_t>& ListCookies() const EL_GETTER { return cookies; }
			void ClearCookies() { cookies.Clear(); }
			void Close();
	};

	text::string::TString UrlDecode(text::string::TStringView url);
	text::string::TString UrlEncode(text::string::TStringView url);
}
