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
				stream::ISource<byte_t>* body;
			};

			struct response_t : response_meta_t
			{
				std::unique_ptr<stream::ISource<byte_t>> body;
			};

			using request_handler_t = util::function::TFunction<void, const request_t&, response_t&>;

		protected:
			ip::TTcpServer* const tcp_server;
			request_handler_t handler;
			system::task::TFiber fiber;

			void FiberMain();

		public:

			// blocking
			static EStatus HandleSingleRequest(stream::ISource<byte_t>&, stream::ISink<byte_t>&, request_handler_t handler);

			THttpServer(ip::TTcpServer* const tcp_server, request_handler_t handler);
			~THttpServer();
	};
}
