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

namespace
{
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
			response.header_fields.Add(L"Content-Length", L"0");
			EXPECT_EQ(request.method, EMethod::GET);
			EXPECT_EQ(request.version, EVersion::HTTP11);
			EXPECT_EQ(request.url, L"/");
			EXPECT_EQ(request.header_fields.ContentLength(), 0U);
		});

		EXPECT_TRUE(handler_called);

		fifo_s2c.CloseOutput();
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, L"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
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
			response.header_fields.Add(L"Content-Length", L"0");
			EXPECT_EQ(request.method, EMethod::GET);
			EXPECT_EQ(request.version, EVersion::HTTP20);
			EXPECT_EQ(request.url, L"/wiki/COBOL");
			EXPECT_EQ(request.header_fields[L"accept-encoding"], L"gzip, deflate, br");
		});

		EXPECT_TRUE(handler_called);

		fifo_s2c.CloseOutput();
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, L"HTTP/2 200 OK\r\nContent-Length: 0\r\n\r\n");
	}

	TEST(io_net_http, HandleSingleRequest_content_length_and_remote_address)
	{
		const char* str_src = "POST /upload HTTP/1.1\r\nContent-Length: 4\r\n\r\ntest";
		TFifo<byte_t> fifo_c2s;
		TFifo<byte_t> fifo_s2c;
		fifo_c2s.WriteAll(reinterpret_cast<const byte_t*>(str_src), strlen(str_src));
		fifo_c2s.CloseOutput();

		const ipport_t remote_address = { ipaddr_t(TString(L"127.0.0.42")), 4242 };
		THttpServer::HandleSingleRequest(fifo_c2s, fifo_s2c, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.method, EMethod::POST);
			EXPECT_EQ(request.header_fields.ContentLength(), 4U);
			EXPECT_EQ(request.remote_address.ip, ipaddr_t(TString(L"127.0.0.42")));
			EXPECT_EQ(request.remote_address.port, 4242);
			byte_t body[8] = {};
			EXPECT_EQ(request.body->Read(body, sizeof(body)), 4U);
			EXPECT_EQ(memcmp(body, "test", 4), 0);
			EXPECT_EQ(request.body->Read(body, sizeof(body)), 0U);
			response.status = EStatus::OK;
		}, remote_address);

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
			auto body = std::unique_ptr<TFifo<byte_t>>(new TFifo<byte_t>());
			body->WriteAll(reinterpret_cast<const byte_t*>("stream"), 6);
			body->CloseOutput();
			response.body = std::move(body);
		});

		EXPECT_EQ(fifo_s2c.OnInputReady(), nullptr);
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, L"HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nstream");
	}

	TEST(io_net_http, THttpServer_curl_simple)
	{
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto file = std::unique_ptr<TFile>(new TFile(L"gen/testdata/test1.json"));
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(L"http://localhost:%d/", tcp_server.LocalAddress().port);
		TString str_curl = TProcess::Execute(L"/usr/bin/curl", { L"--silent", L"--fail", url, url, url });
		str_curl.Cut(0, str_curl.Length() / 3 * 2);
		const TString str_ref = TFile(L"gen/testdata/test1.json").Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_curl, str_ref);
	}

	TEST(io_net_http, THttpServer_curl_https)
	{
		TTcpServer tcp_server;
		tls::TServer tls_server(&tcp_server, L"support/tls-test-cert.pem", L"support/tls-test-key.pem");
		THttpServer http_server(&tls_server, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.url, L"/secure");
			response.status = EStatus::OK;
			auto file = std::unique_ptr<TFile>(new TFile(L"gen/testdata/freecad_v1_0_0.gcode"));
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(L"https://localhost:%d/secure", tls_server.LocalAddress().port);
		const TString str_curl = TProcess::Execute(L"/usr/bin/curl", { L"--silent", L"--fail", L"--insecure", L"--tlsv1.2", url });
		const TString str_ref = TFile(L"gen/testdata/freecad_v1_0_0.gcode").Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_curl, str_ref);
	}

	TEST(io_net_http, THttpServer_curl_https_unknown_length)
	{
		TTcpServer tcp_server;
		tls::TServer tls_server(&tcp_server, L"support/tls-test-cert.pem", L"support/tls-test-key.pem");
		THttpServer http_server(&tls_server, [](const THttpServer::request_t&, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto body = std::unique_ptr<TFifo<byte_t>>(new TFifo<byte_t>());
			body->WriteAll(reinterpret_cast<const byte_t*>("secure-stream"), 13);
			body->CloseOutput();
			response.body = std::move(body);
		});

		const TString url = TString::Format(L"https://localhost:%d/stream", tls_server.LocalAddress().port);
		const TString str_curl = TProcess::Execute(L"/usr/bin/curl", { L"--silent", L"--fail", L"--insecure", url });
		EXPECT_EQ(str_curl, L"secure-stream");
	}

	TEST(io_net_http, THttpServer_curl_error)
	{
		bool fail = false;
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [&fail](const THttpServer::request_t& request, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto file = std::unique_ptr<TFile>(new TFile(L"non-existent file")); // this should throw
			fail = true; // this should never be reached
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(L"http://localhost:%d/", tcp_server.LocalAddress().port);
		EXPECT_THROW(TProcess::Execute(L"/usr/bin/curl", { L"--verbose", L"--fail", url }), TProcess::TNonZeroExitException);
		EXPECT_FALSE(fail);
	}

	TEST(io_net_http, THttpServer_args)
	{
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.url, "/test");
			EXPECT_EQ(request.args.Items().Count(), 2U);
			EXPECT_TRUE(request.args.Contains("abc"));
			EXPECT_TRUE(request.args.Contains("foo"));
			EXPECT_EQ(request.args["foo"], "bar");
			response.status = EStatus::OK;
		});

		const TString url = TString::Format(L"http://localhost:%d/test?foo=bar&abc", tcp_server.LocalAddress().port);
		TProcess::Execute(L"/usr/bin/curl", { L"--verbose", L"--fail", url });
	}

	TEST(io_net_http, THttpServer_empty_arg)
	{
		bool fail = false;
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [&fail](const THttpServer::request_t& request, THttpServer::response_t& response) {
			fail = true;
			response.status = EStatus::OK;
		});

		const TString url = TString::Format(L"http://localhost:%d/test?foo=bar&abc&", tcp_server.LocalAddress().port);
		EXPECT_THROW(TProcess::Execute(L"/usr/bin/curl", { L"--verbose", L"--fail", url }), TProcess::TNonZeroExitException);
		EXPECT_FALSE(fail);
	}

	TEST(io_net_http, THttpServer_decode_url)
	{
		THttpServer::DEBUG = true;
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [](const THttpServer::request_t& request, THttpServer::response_t& response) {
			EXPECT_EQ(request.url, "/test/");
			EXPECT_EQ(request.args.Items().Count(), 1U);
			EXPECT_TRUE(request.args.Contains("ü"));
			EXPECT_EQ(request.args["ü"], "1/_?");
			response.status = EStatus::OK;
		});

		const TString url = TString::Format(L"http://localhost:%d/test%%2f?ü=1%%2f%%5f%%3f", tcp_server.LocalAddress().port);
		TProcess::Execute(L"/usr/bin/curl", { L"--verbose", L"--fail", url });
		THttpServer::DEBUG = false;
	}
}
