#include <gtest/gtest.h>
#include <el1/error.hpp>
#include <el1/io_net_http.hpp>
#include <el1/io_net_ip.hpp>
#include <el1/io_net_tls.hpp>
#include <el1/io_stream.hpp>
#include <el1/io_file.hpp>
#include <el1/io_stream_fifo.hpp>
#include <el1/io_text_encoding_utf8.hpp>
#include <el1/io_collection_list.hpp>
#include <el1/system_task.hpp>
#include <string.h>
#include <stdlib.h>

using namespace ::testing;

#ifndef EL1_TEST_OUT_DIR
#define EL1_TEST_OUT_DIR U"gen"
#endif

namespace
{
	static const el1::io::file::TPath TESTDATA_DIR = el1::io::file::TPath(EL1_TEST_OUT_DIR) + U"testdata";
	using namespace el1::io::net::http;
	using namespace el1::io::collection::list;
	using namespace el1::io::stream;
	using namespace el1::io::stream::fifo;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::io::net::ip;
	namespace tls = el1::io::net::tls;
	using namespace el1::io::file;
	using namespace el1::error;
	using namespace el1::system::task;

	TEST(io_net_http, HandleSingleRequest_simple)
	{
		const char* str_src = "GET / HTTP/1.1\nContent-Length: 0\n\n";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;

		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t& request, THttpServer::response_t& response) {
			handler_called = true;
			response.status = EStatus::OK;
			response.header_fields.Add(U"Content-Length", U"0");
			EXPECT_EQ(request.method, EMethod::GET);
			EXPECT_EQ(request.version, EVersion::HTTP11);
			EXPECT_EQ(request.url, U"/");
			EXPECT_EQ(request.header_fields.ContentLength(), 0U);
		});

		EXPECT_TRUE(handler_called);

		fifo_s2c.CloseOutput();
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, U"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
	}

	TEST(io_net_http, HandleSingleRequest_realworld_http2)
	{
		const char* str_src = "GET /wiki/COBOL HTTP/2\nHost: en.wikipedia.org\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:107.0) Gecko/20100101 Firefox/107.0\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\nAccept-Language: de,en;q=0.5\nAccept-Encoding: gzip, deflate, br\nDNT: 1\nConnection: keep-alive\nCookie: WMF-Last-Access-Global=21-Dec-2022; GeoIP=DE:HE:Frankfurt_am_Main:50.10:8.63:v4\nUpgrade-Insecure-Requests: 1\nSec-Fetch-Dest: document\nSec-Fetch-Mode: navigate\nSec-Fetch-Site: same-origin\nTE: trailers\n\n";
		TFifo<byte_t, 8192> fifo_c2s;
		TFifo<byte_t, 8192> fifo_s2c;

		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t& request, THttpServer::response_t& response) {
			handler_called = true;
			response.status = EStatus::OK;
			response.header_fields.Add(U"Content-Length", U"0");
			EXPECT_EQ(request.method, EMethod::GET);
			EXPECT_EQ(request.version, EVersion::HTTP20);
			EXPECT_EQ(request.url, U"/wiki/COBOL");
			EXPECT_EQ(request.header_fields[U"accept-encoding"], U"gzip, deflate, br");
		});

		EXPECT_TRUE(handler_called);

		fifo_s2c.CloseOutput();
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, U"HTTP/2 200 OK\r\nContent-Length: 0\r\n\r\n");
	}

	TEST(io_net_http, HandleSingleRequest_content_length_and_remote_address)
	{
		const char* str_src = "POST /upload HTTP/1.1\r\nContent-Length: 4\r\n\r\ntest";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		const ipport_t remote_address = { ipaddr_t(TString(U"127.0.0.42")), 4242 };
		THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.method, EMethod::POST);
			EXPECT_EQ(request.header_fields.ContentLength(), 4U);
			EXPECT_EQ(request.remote_address.ip, ipaddr_t(TString(U"127.0.0.42")));
			EXPECT_EQ(request.remote_address.port, 4242);
			byte_t body[8] = {};
			EXPECT_EQ(request.body->Read(body, sizeof(body)), 4U);
			EXPECT_EQ(memcmp(body, "test", 4), 0);
			EXPECT_EQ(request.body->Read(body, sizeof(body)), 0U);
			response.status = EStatus::OK;
		}, remote_address);

	}

	TEST(io_net_http, HandleSingleRequest_chunked_request)
	{
		const char* str_src =
			"PATCH /upload HTTP/1.1\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"4\r\nWiki\r\n"
			"5;extension=yes\r\npedia\r\n"
			"0\r\nX-Trailer: done\r\n\r\n";
		TFifo<byte_t, 1024> fifo_c2s;
		TFifo<byte_t, 1024> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		TString body;
		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&body](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.method, EMethod::PATCH);
			EXPECT_EQ(request.url, U"/upload");
			body = TString(request.body->Pipe().Transform(TUTF8Decoder()).Collect());
			response.status = EStatus::ACCEPTED;
		});
		EXPECT_EQ(status, EStatus::ACCEPTED);
		EXPECT_EQ(body, U"Wikipedia");
	}

	TEST(io_net_http, HandleSingleRequest_combines_repeated_accept_fields)
	{
		const char* str_src =
			"GET /v2/example/image/manifests/latest HTTP/1.1\r\n"
			"Host: registry.example.test\r\n"
			"Accept: application/vnd.oci.image.manifest.v1+json\r\n"
			"Accept: application/vnd.docker.distribution.manifest.v2+json\r\n"
			"Accept: application/vnd.docker.distribution.manifest.v1+prettyjws\r\n"
			"Accept: application/vnd.docker.distribution.manifest.v1+json\r\n"
			"Accept: application/vnd.docker.distribution.manifest.list.v2+json\r\n"
			"Accept: application/vnd.oci.image.index.v1+json\r\n"
			"Content-Length: 0\r\n"
			"\r\n";
		TFifo<byte_t, 4096> fifo_c2s;
		TFifo<byte_t, 4096> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t& request, THttpServer::response_t& response)
		{
			handler_called = true;
			const TString* const accept = request.header_fields.Get(U"accept");
			ASSERT_NE(accept, nullptr);
			EXPECT_EQ(
				*accept,
				U"application/vnd.oci.image.manifest.v1+json, "
				U"application/vnd.docker.distribution.manifest.v2+json, "
				U"application/vnd.docker.distribution.manifest.v1+prettyjws, "
				U"application/vnd.docker.distribution.manifest.v1+json, "
				U"application/vnd.docker.distribution.manifest.list.v2+json, "
				U"application/vnd.oci.image.index.v1+json"
			);
			response.status = EStatus::OK;
		});
		EXPECT_EQ(status, EStatus::OK);
		EXPECT_TRUE(handler_called);
	}

	TEST(io_net_http, HandleSingleRequest_rejects_duplicate_authorization)
	{
		const char* str_src =
			"GET / HTTP/1.1\r\n"
			"Authorization: Basic Zm9vOmJhcg==\r\n"
			"Authorization: Basic YmF6OnF1dXg=\r\n"
			"Content-Length: 0\r\n"
			"\r\n";
		TFifo<byte_t, 1024> fifo_c2s;
		TFifo<byte_t, 1024> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t&, THttpServer::response_t&)
		{
			handler_called = true;
		});
		EXPECT_EQ(status, EStatus::BAD_REQUEST);
		EXPECT_FALSE(handler_called);
	}

	TEST(io_net_http, HandleSingleRequest_rejects_duplicate_content_length)
	{
		const char* str_src =
			"GET / HTTP/1.1\r\n"
			"Content-Length: 0\r\n"
			"Content-Length: 0\r\n"
			"\r\n";
		TFifo<byte_t, 1024> fifo_c2s;
		TFifo<byte_t, 1024> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t&, THttpServer::response_t&)
		{
			handler_called = true;
		});
		EXPECT_EQ(status, EStatus::BAD_REQUEST);
		EXPECT_FALSE(handler_called);
	}

	TEST(io_net_http, HandleSingleRequest_rejects_content_length_with_transfer_encoding)
	{
		const char* str_src =
			"POST /upload HTTP/1.1\r\n"
			"Content-Length: 4\r\n"
			"Transfer-Encoding: chunked\r\n"
			"\r\n"
			"0\r\n\r\n";
		TFifo<byte_t, 1024> fifo_c2s;
		TFifo<byte_t, 1024> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t&, THttpServer::response_t&) {
			handler_called = true;
		});
		EXPECT_EQ(status, EStatus::BAD_REQUEST);
		EXPECT_FALSE(handler_called);
	}

	TEST(io_net_http, HandleSingleRequest_rejects_request_target_controls)
	{
		const char str_src[] = "GET /\x01 HTTP/1.1\r\nContent-Length: 0\r\n\r\n";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), sizeof(str_src) - 1);
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t&, THttpServer::response_t&)
		{
			handler_called = true;
		});
		EXPECT_EQ(status, EStatus::BAD_REQUEST);
		EXPECT_FALSE(handler_called);
	}

	TEST(io_net_http, HandleSingleRequest_rejects_percent_encoded_path_controls)
	{
		const char* str_src = "GET /safe%0ainjected HTTP/1.1\r\nContent-Length: 0\r\n\r\n";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t&, THttpServer::response_t&)
		{
			handler_called = true;
		});
		EXPECT_EQ(status, EStatus::BAD_REQUEST);
		EXPECT_FALSE(handler_called);
	}

	TEST(io_net_http, HandleSingleRequest_rejects_whitespace_before_header_colon)
	{
		const char* str_src = "GET / HTTP/1.1\r\nContent-Length : 0\r\n\r\n";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		bool handler_called = false;
		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [&handler_called](const THttpServer::request_t&, THttpServer::response_t&)
		{
			handler_called = true;
		});
		EXPECT_EQ(status, EStatus::BAD_REQUEST);
		EXPECT_FALSE(handler_called);
	}

	TEST(io_net_http, HandleSingleRequest_closes_connection_for_unread_request_body)
	{
		const char* str_src = "POST /ignore HTTP/1.1\r\nContent-Length: 4\r\n\r\ntest";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [](const THttpServer::request_t&, THttpServer::response_t& response)
		{
			response.status = EStatus::OK;
		});
		EXPECT_EQ(status, EStatus::OK);
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, U"HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
	}

	TEST(io_net_http, HandleSingleRequest_rejects_response_header_injection)
	{
		const char* str_src = "GET / HTTP/1.1\r\nContent-Length: 0\r\n\r\n";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		const EStatus status = THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [](const THttpServer::request_t&, THttpServer::response_t& response)
		{
			response.status = EStatus::OK;
			response.header_fields.Set(U"X-Test", U"safe\r\nInjected: yes");
		});
		EXPECT_EQ(status, EStatus::INTERNAL_SERVER_ERROR);
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, U"");
	}

	TEST(io_net_http, HandleSingleRequest_unknown_length_response_closes_stream)
	{
		const char* str_src = "GET /preview HTTP/1.1\r\nContent-Length: 0\r\n\r\n";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [](const THttpServer::request_t&, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto body = el1::New<TFifo<byte_t>>();
			body->WriteAll(reinterpret_cast<const byte_t*>("stream"), 6);
			body->CloseOutput();
			response.body = std::move(body);
		});

		EXPECT_EQ(fifo_s2c.OnInputReady(), nullptr);
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, U"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nstream");
	}

	TEST(io_net_http, THttpServer_curl_simple)
	{
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto file = el1::New<TFile>(TESTDATA_DIR + U"test1.json");
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(U"http://localhost:%d/", tcp_server.LocalAddress().port);
		TString str_curl = TProcess::Execute(U"/usr/bin/curl", { U"--silent", U"--fail", url, url, url });
		str_curl.Cut(0, str_curl.Length() / 3 * 2);
		TFile reference_file(TESTDATA_DIR + U"test1.json");
		const TString str_ref = reference_file.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_curl, str_ref);
	}

	TEST(io_net_http, THttpServer_curl_https)
	{
		TTcpServer tcp_server;
		tls::TServer tls_server(&tcp_server, U"support/tls-test-cert.pem", U"support/tls-test-key.pem");
		THttpServer http_server(&tls_server, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.url, U"/secure");
			response.status = EStatus::OK;
			auto file = el1::New<TFile>(TESTDATA_DIR + U"freecad_v1_0_0.gcode");
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(U"https://localhost:%d/secure", tls_server.LocalAddress().port);
		const TString str_curl = TProcess::Execute(U"/usr/bin/curl", { U"--silent", U"--fail", U"--cacert", U"support/tls-test-cert.pem", U"--tlsv1.2", url });
		TFile reference_file(TESTDATA_DIR + U"freecad_v1_0_0.gcode");
		const TString str_ref = reference_file.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_curl, str_ref);
	}

	TEST(io_net_http, THttpServer_curl_https_unknown_length)
	{
		TTcpServer tcp_server;
		tls::TServer tls_server(&tcp_server, U"support/tls-test-cert.pem", U"support/tls-test-key.pem");
		THttpServer http_server(&tls_server, [](const THttpServer::request_t&, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto body = el1::New<TFifo<byte_t>>();
			body->WriteAll(reinterpret_cast<const byte_t*>("secure-stream"), 13);
			body->CloseOutput();
			response.body = std::move(body);
		});

		const TString url = TString::Format(U"https://localhost:%d/stream", tls_server.LocalAddress().port);
		const TString str_curl = TProcess::Execute(U"/usr/bin/curl", { U"--silent", U"--fail", U"--cacert", U"support/tls-test-cert.pem", url });
		EXPECT_EQ(str_curl, U"secure-stream");
	}

	TEST(io_net_http, THttpServer_curl_error)
	{
		bool fail = false;
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [&fail](const THttpServer::request_t& request, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto file = el1::New<TFile>(U"non-existent file"); // this should throw
			fail = true; // this should never be reached
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(U"http://localhost:%d/", tcp_server.LocalAddress().port);
		EXPECT_THROW(TProcess::Execute(U"/usr/bin/curl", { U"--verbose", U"--fail", url }), TProcess::TNonZeroExitException);
		EXPECT_FALSE(fail);
	}

	TEST(io_net_http, THttpServer_args)
	{
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.url, U"/test");
			EXPECT_EQ(request.args.Items().Count(), 2U);
			EXPECT_TRUE(request.args.Contains(U"abc"));
			EXPECT_TRUE(request.args.Contains(U"foo"));
			EXPECT_EQ(request.args[U"foo"], U"bar");
			response.status = EStatus::OK;
		});

		const TString url = TString::Format(U"http://localhost:%d/test?foo=bar&abc", tcp_server.LocalAddress().port);
		TProcess::Execute(U"/usr/bin/curl", { U"--verbose", U"--fail", url });
	}

	TEST(io_net_http, THttpServer_empty_arg)
	{
		bool fail = false;
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [&fail](const THttpServer::request_t& request, THttpServer::response_t& response) {
			fail = true;
			response.status = EStatus::OK;
		});

		const TString url = TString::Format(U"http://localhost:%d/test?foo=bar&abc&", tcp_server.LocalAddress().port);
		EXPECT_THROW(TProcess::Execute(U"/usr/bin/curl", { U"--verbose", U"--fail", url }), TProcess::TNonZeroExitException);
		EXPECT_FALSE(fail);
	}

	TEST(io_net_http, THttpServer_decode_url)
	{
		THttpServer::DEBUG = true;
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.url, U"/test/");
			EXPECT_EQ(request.args.Items().Count(), 1U);
			EXPECT_TRUE(request.args.Contains(U"ü"));
			EXPECT_EQ(request.args[U"ü"], U"1/_?");
			response.status = EStatus::OK;
		});

		const TString url = TString::Format(U"http://localhost:%d/test%%2f?ü=1%%2f%%5f%%3f", tcp_server.LocalAddress().port);
		TProcess::Execute(U"/usr/bin/curl", { U"--verbose", U"--fail", url });
		THttpServer::DEBUG = false;
	}

	TEST(io_net_http, THttpClient_stateful_headers_cookies_and_binary_body)
	{
		TTcpServer tcp_server;
		usys_t request_index = 0;
		port_t client_port = 0;
		const byte_t upload[] = { 0x00, 0x01, 0x7f, 0x80, 0xff };
		const byte_t download[] = { 0xff, 0x00, 0x42, 0x7f };

		THttpServer http_server(&tcp_server, [&](const THttpServer::request_t& request, THttpServer::response_t& response) {
			request_index++;
			if(client_port == 0)
				client_port = request.remote_address.port;
			EXPECT_EQ(request.remote_address.port, client_port);
			EXPECT_EQ(request.header_fields[U"x-default"], U"persistent");

			if(request_index == 1)
			{
				EXPECT_EQ(request.method, EMethod::POST);
				EXPECT_EQ(request.url, U"/login");
				EXPECT_EQ(request.header_fields[U"x-once"], U"one");
				EXPECT_EQ(request.header_fields.ContentLength(), sizeof(upload));
				byte_t received[sizeof(upload)] = {};
				request.body->ReadAll(received, sizeof(received));
				EXPECT_EQ(memcmp(received, upload, sizeof(upload)), 0);

				response.status = EStatus::CREATED;
				response.header_fields.Set(U"Set-Cookie", U"session=abc; Path=/api; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly; Max-Age=3600");
				response.header_fields.Set(U"X-Reply", U"first");
				response.header_fields.ContentLength(sizeof(download));
				auto body = el1::New<TFifo<byte_t>>();
				body->WriteAll(download, sizeof(download));
				body->CloseOutput();
				response.body = std::move(body);
			}
			else if(request_index == 2)
			{
				EXPECT_EQ(request.url, U"/outside");
				EXPECT_EQ(request.header_fields.Get(U"cookie"), nullptr);
				EXPECT_EQ(request.header_fields.Get(U"x-once"), nullptr);
				response.status = EStatus::OK;
			}
			else if(request_index == 3)
			{
				EXPECT_EQ(request.url, U"/api/data");
				ASSERT_NE(request.header_fields.Get(U"cookie"), nullptr);
				EXPECT_EQ(*request.header_fields.Get(U"cookie"), U"session=abc");
				response.status = EStatus::OK;
				response.header_fields.Set(U"Set-Cookie", U"session=deleted; Path=/api; Max-Age=0");
			}
			else if(request_index == 4)
			{
				EXPECT_EQ(request.url, U"/api/data");
				EXPECT_EQ(request.header_fields.Get(U"cookie"), nullptr);
				response.status = EStatus::OK;
			}
		});

		THttpClient client(U"localhost", tcp_server.LocalAddress().port);
		client.SetHeader(U"X-Default", U"persistent");
		ASSERT_NE(client.FindHeader(U"x-default"), nullptr);
		EXPECT_EQ(*client.FindHeader(U"x-default"), U"persistent");

		TFifo<byte_t> upload_source;
		upload_source.WriteAll(upload, sizeof(upload));
		upload_source.CloseOutput();
		THttpClient::request_t request;
		request.method = EMethod::POST;
		request.url = U"/login";
		request.header_fields.Set(U"X-Once", U"one");
		request.body = &upload_source;
		request.content_length = sizeof(upload);
		auto response = client.Request(std::move(request));
		EXPECT_EQ(response.status, EStatus::CREATED);
		ASSERT_NE(response.FindHeader(U"x-reply"), nullptr);
		EXPECT_EQ(*response.FindHeader(U"x-reply"), U"first");
		ASSERT_EQ(response.body.Count(), sizeof(download));
		EXPECT_EQ(memcmp(response.body.ItemPtr(0), download, sizeof(download)), 0);
		ASSERT_EQ(client.ListCookies().Count(), 1U);
		EXPECT_EQ(client.ListCookies()[0].name, U"session");
		EXPECT_TRUE(client.ListCookies()[0].http_only);

		client.Get(U"/outside");
		client.Get(U"/api/data");
		EXPECT_EQ(client.ListCookies().Count(), 0U);
		client.Get(U"/api/data");
		EXPECT_EQ(request_index, 4U);
	}

	TEST(io_net_http, THttpClient_body_limit_and_convenience_helpers)
	{
		TTcpServer tcp_server;
		usys_t request_index = 0;
		const byte_t upload[] = { 1, 2, 3, 4, 5 };
		const byte_t download[] = { 9, 8, 7, 6 };

		THttpServer http_server(&tcp_server, [&](const THttpServer::request_t& request, THttpServer::response_t& response) {
			request_index++;
			response.status = EStatus::OK;

			if(request_index == 1)
			{
				EXPECT_EQ(request.method, EMethod::POST);
				EXPECT_EQ(request.url, U"/post");
				EXPECT_EQ(request.header_fields[U"x-test"], U"yes");
				byte_t received[sizeof(upload)] = {};
				request.body->ReadAll(received, sizeof(received));
				EXPECT_EQ(memcmp(received, upload, sizeof(upload)), 0);
				response.header_fields.Set(U"X-Reply", U"post");
				response.header_fields.ContentLength(sizeof(download));
				auto body = el1::New<TFifo<byte_t>>();
				body->WriteAll(download, sizeof(download));
				body->CloseOutput();
				response.body = std::move(body);
			}
			else if(request_index == 2)
			{
				EXPECT_EQ(request.method, EMethod::GET);
				EXPECT_EQ(request.url, U"/get");
				response.header_fields.Set(U"X-Reply", U"get");
				response.header_fields.ContentLength(sizeof(download));
				auto body = el1::New<TFifo<byte_t>>();
				body->WriteAll(download, sizeof(download));
				body->CloseOutput();
				response.body = std::move(body);
			}
			else
			{
				response.header_fields.ContentLength(64);
				auto body = el1::New<TFifo<byte_t>>();
				byte_t data[64] = {};
				body->WriteAll(data, sizeof(data));
				body->CloseOutput();
				response.body = std::move(body);
			}
		});

		THttpClient client(U"localhost", tcp_server.LocalAddress().port);
		THttpHeaderFields request_headers;
		request_headers.Set(U"X-Test", U"yes");
		auto post_response = client.Post(U"/post", array_t<const byte_t>::FromUnsafePointer(upload, sizeof(upload)), std::move(request_headers));
		ASSERT_NE(post_response.FindHeader(U"x-reply"), nullptr);
		EXPECT_EQ(*post_response.FindHeader(U"x-reply"), U"post");
		ASSERT_EQ(post_response.body.Count(), sizeof(download));
		EXPECT_EQ(memcmp(post_response.body.ItemPtr(0), download, sizeof(download)), 0);

		THttpClient::response_header_t response_header;
		auto source = client.Get(U"/get", &response_header);
		ASSERT_NE(response_header.FindHeader(U"x-reply"), nullptr);
		EXPECT_EQ(*response_header.FindHeader(U"x-reply"), U"get");
		TList<byte_t> source_body = source->Pipe().Collect();
		ASSERT_EQ(source_body.Count(), sizeof(download));
		EXPECT_EQ(memcmp(source_body.ItemPtr(0), download, sizeof(download)), 0);

		EXPECT_THROW(client.Get(U"/too-large", static_cast<ISink<byte_t>*>(nullptr), 32), TException);
		EXPECT_EQ(THttpClient::DEFAULT_RESPONSE_BODY_LIMIT, 16U * 1024U * 1024U);
	}

	TEST(io_net_http, THttpClient_rejects_request_injection)
	{
		THttpClient client(U"localhost", 1);
		EXPECT_THROW(client.SetHeader(U"X-Test", U"valid\r\nInjected: yes"), TInvalidArgumentException);
		EXPECT_THROW(client.SetHeader(U"Bad Header", U"value"), TInvalidArgumentException);

		THttpClient::request_t request;
		request.url = U"/test\r\nInjected: yes";
		EXPECT_THROW(client.Request(std::move(request)), TInvalidArgumentException);

		THttpClient::request_t request_header;
		request_header.header_fields.Set(U"X-Test", U"valid\nInjected: yes");
		EXPECT_THROW(client.Request(std::move(request_header)), TInvalidArgumentException);
	}

	TEST(io_net_http, THttpClient_streaming_response)
	{
		TTcpServer tcp_server;
		TList<byte_t> expected;
		for(usys_t i = 0; i < 40U * 1024U + 17U; i++)
			expected.Append((byte_t)(i * 31U));

		THttpServer http_server(&tcp_server, [&](const THttpServer::request_t&, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			response.header_fields.ContentLength(expected.Count());
			auto body = el1::New<TFifo<byte_t, 65536>>();
			body->WriteAll(expected.ItemPtr(0), expected.Count());
			body->CloseOutput();
			response.body = std::move(body);
		});

		THttpClient client(U"localhost", tcp_server.LocalAddress().port);
		TList<byte_t> received;
		TListSink<byte_t> sink(&received);
		auto response = client.Get(U"/large.bin", &sink);
		EXPECT_EQ(response.status, EStatus::OK);
		EXPECT_EQ(response.body.Count(), 0U);
		ASSERT_EQ(received.Count(), expected.Count());
		EXPECT_EQ(memcmp(received.ItemPtr(0), expected.ItemPtr(0), expected.Count()), 0);
	}

	TEST(io_net_http, THttpClient_https)
	{
		TTcpServer tcp_server;
		tls::TServer tls_server(&tcp_server, U"support/tls-test-cert.pem", U"support/tls-test-key.pem");
		usys_t request_index = 0;
		THttpServer http_server(&tls_server, [&](const THttpServer::request_t& request, THttpServer::response_t& response) {
			request_index++;
			response.status = EStatus::OK;
			if(request_index == 1)
			{
				response.header_fields.Set(U"Set-Cookie", U"secure=yes; Path=/; Secure");
			}
			else
			{
				ASSERT_NE(request.header_fields.Get(U"cookie"), nullptr);
				EXPECT_EQ(*request.header_fields.Get(U"cookie"), U"secure=yes");
			}
		});

		tls::client_config_t config;
		config.ca_certificates = tls::TPemSource(TPath(U"support/tls-test-cert.pem"));
		THttpClient client(U"localhost", tls_server.LocalAddress().port, std::move(config));
		EXPECT_EQ(client.Get(U"/one").status, EStatus::OK);
		EXPECT_EQ(client.Get(U"/two").status, EStatus::OK);
		EXPECT_EQ(request_index, 2U);
	}

	TEST(io_net_http, THttpClient_https_credentials_from_memory)
	{
		TFile certificate_file(U"support/tls-test-cert.pem");
		TFile private_key_file(U"support/tls-test-key.pem");
		TList<byte_t> certificate = certificate_file.Pipe().Collect();
		TList<byte_t> private_key = private_key_file.Pipe().Collect();

		TTcpServer tcp_server;
		tls::server_config_t server_config;
		server_config.certificate_chain = tls::TPemSource(certificate);
		server_config.private_key = tls::TPemSource(std::move(private_key));
		tls::TServer tls_server(&tcp_server, std::move(server_config));
		THttpServer http_server(&tls_server, [](const THttpServer::request_t&, THttpServer::response_t& response) {
			response.status = EStatus::OK;
		});

		tls::client_config_t client_config;
		client_config.ca_certificates = tls::TPemSource(std::move(certificate));
		THttpClient client(U"localhost", tls_server.LocalAddress().port, std::move(client_config));
		EXPECT_EQ(client.Get(U"/").status, EStatus::OK);
	}

	TEST(io_net_http, THttpClient_chunked_upload_download_and_trailer)
	{
		TTcpServer tcp_server;
		TList<byte_t> upload;
		for(usys_t i = 0; i < 35U * 1024U + 3U; i++)
			upload.Append((byte_t)(i * 17U));

		TFiber raw_server([&]() {
			std::unique_ptr<TTcpClient> client;
			while((client = tcp_server.AcceptClient()) == nullptr)
				tcp_server.OnClientConnect().WaitFor();

			TString request_header;
			const char terminator[] = "\r\n\r\n";
			usys_t matched = 0;
			while(matched != 4)
			{
				byte_t byte = 0;
				client->ReadAll(&byte, 1);
				request_header += char32_t((u32_t)byte);
				if(byte == (byte_t)terminator[matched])
					matched++;
				else
					matched = byte == (byte_t)terminator[0] ? 1U : 0U;
			}
			EXPECT_TRUE(request_header.Contains(TStringView(U"Transfer-Encoding: chunked")));

			auto read_line = [&](TTcpClient& stream) {
				TString line;
				for(;;)
				{
					byte_t byte = 0;
					stream.ReadAll(&byte, 1);
					if(byte == '\n')
					{
						if(line.Length() != 0 && line[-1] == '\r')
							line.chars.Remove(-1);
						return line;
					}
					line += char32_t((u32_t)byte);
				}
			};

			TList<byte_t> received;
			for(;;)
			{
				TString size_line = read_line(*client);
				auto size_cstr = size_line.MakeCStr();
				const usys_t chunk_size = strtoul(size_cstr.get(), nullptr, 16);
				if(chunk_size == 0)
				{
					EXPECT_EQ(read_line(*client), U"");
					break;
				}

				const usys_t offset = received.Count();
				received.Inflate(chunk_size, 0);
				client->ReadAll(received.ItemPtr(offset), chunk_size);
				EXPECT_EQ(read_line(*client), U"");
			}

			ASSERT_EQ(received.Count(), upload.Count());
			EXPECT_EQ(memcmp(received.ItemPtr(0), upload.ItemPtr(0), upload.Count()), 0);

			const char response[] =
				"HTTP/1.1 200 OK\r\n"
				"Transfer-Encoding: chunked\r\n"
				"Connection: close\r\n"
				"\r\n"
				"4\r\nWiki\r\n"
				"5\r\npedia\r\n"
				"0\r\n"
				"X-Trailer: done\r\n"
				"\r\n";
			client->WriteAll(reinterpret_cast<const byte_t*>(response), sizeof(response) - 1);
			client->CloseOutput();
		});

		TFifo<byte_t, 65536> upload_source;
		upload_source.WriteAll(upload.ItemPtr(0), upload.Count());
		upload_source.CloseOutput();

		THttpClient client(U"localhost", tcp_server.LocalAddress().port);
		auto response = client.Post(U"/chunked", upload_source);

		const char expected[] = "Wikipedia";
		ASSERT_EQ(response.body.Count(), sizeof(expected) - 1);
		EXPECT_EQ(memcmp(response.body.ItemPtr(0), expected, sizeof(expected) - 1), 0);
		ASSERT_NE(response.FindHeader(U"x-trailer"), nullptr);
		EXPECT_EQ(*response.FindHeader(U"x-trailer"), U"done");
		EXPECT_EQ(raw_server.Join(), nullptr);
	}

}
