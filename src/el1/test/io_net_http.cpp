#include <gtest/gtest.h>
#include <el1/error.hpp>
#include <el1/io_net_http.hpp>
#include <el1/io_net_ip.hpp>
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
		});

		EXPECT_TRUE(handler_called);

		fifo_s2c.CloseOutput();
		const TString str_response = fifo_s2c.Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_response, L"HTTP/1.1 200 OK\nContent-Length: 0\n\n");
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
		EXPECT_EQ(str_response, L"HTTP/2 200 OK\nContent-Length: 0\n\n");
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
