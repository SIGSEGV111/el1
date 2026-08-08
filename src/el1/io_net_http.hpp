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
		BAD_REQUEST = 400,
		UNAUTHORIZED = 401,
		FORBIDDEN = 403,
		NOT_FOUND = 404,
		METHOD_NOT_ALLOWED = 405,
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

	class THttpServer
	{
		public:
			static bool DEBUG;

			struct request_t : request_meta_t
			{
				ip::ipport_t remote_address;
				stream::ISource<byte_t>* body;
			};

			struct response_t : response_meta_t
			{
				std::unique_ptr<stream::ISource<byte_t>> body;
			};

			using request_handler_t = util::function::TFunction<void, const request_t&, response_t&>;

		protected:
			ip::IStreamServer* const stream_server;
			request_handler_t handler;
			system::task::TFiber fiber;

			void FiberMain();

		public:

			// blocking
			static EStatus HandleSingleRequest(
				stream::ISource<byte_t>&,
				stream::ISink<byte_t>&,
				request_handler_t handler,
				const ip::ipport_t remote_address = ip::ipport_t{}
			);

			THttpServer(ip::IStreamServer* const stream_server, request_handler_t handler);
			THttpServer(ip::TTcpServer* const tcp_server, request_handler_t handler);
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
				text::string::TString url = L"/";
				THttpHeaderFields header_fields;
				stream::ISource<byte_t>* body = nullptr;
				usys_t content_length = 0;
			};

			struct response_header_t : response_meta_t
			{
				using header_line_t = kv_pair_tt<text::string::TString, text::string::TString>;

				collection::list::TList<header_line_t> header_lines;

				const text::string::TString* FindHeader(const text::string::TString& name) const EL_GETTER;
				collection::list::TList<text::string::TString> FindHeaders(const text::string::TString& name) const EL_GETTER;
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
			void ProcessSetCookie(const text::string::TString& value, const text::string::TString& request_path);
			text::string::TString BuildCookieHeader(const text::string::TString& request_path);

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
			const text::string::TString* FindHeader(const text::string::TString& name) const EL_GETTER;
			bool RemoveHeader(const text::string::TString& name);

			const collection::list::TList<cookie_t>& ListCookies() const EL_GETTER { return cookies; }
			void ClearCookies() { cookies.Clear(); }
			void Close();
	};

	text::string::TString UrlDecode(text::string::TString url);
	text::string::TString UrlEncode(text::string::TString url);
}
