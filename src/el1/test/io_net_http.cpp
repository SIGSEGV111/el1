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
			auto body = el1::New<TFifo<byte_t>>();
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
			auto file = el1::New<TFile>(L"gen/testdata/test1.json");
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(U"http://localhost:%d/", tcp_server.LocalAddress().port);
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
			auto file = el1::New<TFile>(L"gen/testdata/freecad_v1_0_0.gcode");
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(U"https://localhost:%d/secure", tls_server.LocalAddress().port);
		const TString str_curl = TProcess::Execute(L"/usr/bin/curl", { L"--silent", L"--fail", L"--cacert", L"support/tls-test-cert.pem", L"--tlsv1.2", url });
		const TString str_ref = TFile(L"gen/testdata/freecad_v1_0_0.gcode").Pipe().Transform(TUTF8Decoder()).Collect();
		EXPECT_EQ(str_curl, str_ref);
	}

	TEST(io_net_http, THttpServer_curl_https_unknown_length)
	{
		TTcpServer tcp_server;
		tls::TServer tls_server(&tcp_server, L"support/tls-test-cert.pem", L"support/tls-test-key.pem");
		THttpServer http_server(&tls_server, [](const THttpServer::request_t&, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto body = el1::New<TFifo<byte_t>>();
			body->WriteAll(reinterpret_cast<const byte_t*>("secure-stream"), 13);
			body->CloseOutput();
			response.body = std::move(body);
		});

		const TString url = TString::Format(U"https://localhost:%d/stream", tls_server.LocalAddress().port);
		const TString str_curl = TProcess::Execute(L"/usr/bin/curl", { L"--silent", L"--fail", L"--cacert", L"support/tls-test-cert.pem", url });
		EXPECT_EQ(str_curl, L"secure-stream");
	}

	TEST(io_net_http, THttpServer_curl_error)
	{
		bool fail = false;
		TTcpServer tcp_server;
		THttpServer http_server(&tcp_server, [&fail](const THttpServer::request_t& request, THttpServer::response_t& response) {
			response.status = EStatus::OK;
			auto file = el1::New<TFile>(L"non-existent file"); // this should throw
			fail = true; // this should never be reached
			response.header_fields.ContentLength(file->Size());
			response.body = std::move(file);
		});

		const TString url = TString::Format(U"http://localhost:%d/", tcp_server.LocalAddress().port);
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

		const TString url = TString::Format(U"http://localhost:%d/test?foo=bar&abc", tcp_server.LocalAddress().port);
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

		const TString url = TString::Format(U"http://localhost:%d/test?foo=bar&abc&", tcp_server.LocalAddress().port);
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

		const TString url = TString::Format(U"http://localhost:%d/test%%2f?ü=1%%2f%%5f%%3f", tcp_server.LocalAddress().port);
		TProcess::Execute(L"/usr/bin/curl", { L"--verbose", L"--fail", url });
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
			EXPECT_EQ(request.header_fields[L"x-default"], L"persistent");

			if(request_index == 1)
			{
				EXPECT_EQ(request.method, EMethod::POST);
				EXPECT_EQ(request.url, L"/login");
				EXPECT_EQ(request.header_fields[L"x-once"], L"one");
				EXPECT_EQ(request.header_fields.ContentLength(), sizeof(upload));
				byte_t received[sizeof(upload)] = {};
				request.body->ReadAll(received, sizeof(received));
				EXPECT_EQ(memcmp(received, upload, sizeof(upload)), 0);

				response.status = EStatus::CREATED;
				response.header_fields.Set(L"Set-Cookie", L"session=abc; Path=/api; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly; Max-Age=3600");
				response.header_fields.Set(L"X-Reply", L"first");
				response.header_fields.ContentLength(sizeof(download));
				auto body = el1::New<TFifo<byte_t>>();
				body->WriteAll(download, sizeof(download));
				body->CloseOutput();
				response.body = std::move(body);
			}
			else if(request_index == 2)
			{
				EXPECT_EQ(request.url, L"/outside");
				EXPECT_EQ(request.header_fields.Get(L"cookie"), nullptr);
				EXPECT_EQ(request.header_fields.Get(L"x-once"), nullptr);
				response.status = EStatus::OK;
			}
			else if(request_index == 3)
			{
				EXPECT_EQ(request.url, L"/api/data");
				ASSERT_NE(request.header_fields.Get(L"cookie"), nullptr);
				EXPECT_EQ(*request.header_fields.Get(L"cookie"), L"session=abc");
				response.status = EStatus::OK;
				response.header_fields.Set(L"Set-Cookie", L"session=deleted; Path=/api; Max-Age=0");
			}
			else if(request_index == 4)
			{
				EXPECT_EQ(request.url, L"/api/data");
				EXPECT_EQ(request.header_fields.Get(L"cookie"), nullptr);
				response.status = EStatus::OK;
			}
		});

		THttpClient client(L"localhost", tcp_server.LocalAddress().port);
		client.SetHeader(L"X-Default", L"persistent");
		ASSERT_NE(client.FindHeader(L"x-default"), nullptr);
		EXPECT_EQ(*client.FindHeader(L"x-default"), L"persistent");

		TFifo<byte_t> upload_source;
		upload_source.WriteAll(upload, sizeof(upload));
		upload_source.CloseOutput();
		THttpClient::request_t request;
		request.method = EMethod::POST;
		request.url = L"/login";
		request.header_fields.Set(L"X-Once", L"one");
		request.body = &upload_source;
		request.content_length = sizeof(upload);
		auto response = client.Request(std::move(request));
		EXPECT_EQ(response.status, EStatus::CREATED);
		ASSERT_NE(response.FindHeader(L"x-reply"), nullptr);
		EXPECT_EQ(*response.FindHeader(L"x-reply"), L"first");
		ASSERT_EQ(response.body.Count(), sizeof(download));
		EXPECT_EQ(memcmp(response.body.ItemPtr(0), download, sizeof(download)), 0);
		ASSERT_EQ(client.ListCookies().Count(), 1U);
		EXPECT_EQ(client.ListCookies()[0].name, L"session");
		EXPECT_TRUE(client.ListCookies()[0].http_only);

		client.Get(L"/outside");
		client.Get(L"/api/data");
		EXPECT_EQ(client.ListCookies().Count(), 0U);
		client.Get(L"/api/data");
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
				EXPECT_EQ(request.url, L"/post");
				EXPECT_EQ(request.header_fields[L"x-test"], L"yes");
				byte_t received[sizeof(upload)] = {};
				request.body->ReadAll(received, sizeof(received));
				EXPECT_EQ(memcmp(received, upload, sizeof(upload)), 0);
				response.header_fields.Set(L"X-Reply", L"post");
				response.header_fields.ContentLength(sizeof(download));
				auto body = el1::New<TFifo<byte_t>>();
				body->WriteAll(download, sizeof(download));
				body->CloseOutput();
				response.body = std::move(body);
			}
			else if(request_index == 2)
			{
				EXPECT_EQ(request.method, EMethod::GET);
				EXPECT_EQ(request.url, L"/get");
				response.header_fields.Set(L"X-Reply", L"get");
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

		THttpClient client(L"localhost", tcp_server.LocalAddress().port);
		THttpHeaderFields request_headers;
		request_headers.Set(L"X-Test", L"yes");
		auto post_response = client.Post(L"/post", array_t<const byte_t>(upload, sizeof(upload)), std::move(request_headers));
		ASSERT_NE(post_response.FindHeader(L"x-reply"), nullptr);
		EXPECT_EQ(*post_response.FindHeader(L"x-reply"), L"post");
		ASSERT_EQ(post_response.body.Count(), sizeof(download));
		EXPECT_EQ(memcmp(post_response.body.ItemPtr(0), download, sizeof(download)), 0);

		THttpClient::response_header_t response_header;
		auto source = client.Get(L"/get", &response_header);
		ASSERT_NE(response_header.FindHeader(L"x-reply"), nullptr);
		EXPECT_EQ(*response_header.FindHeader(L"x-reply"), L"get");
		TList<byte_t> source_body = source->Pipe().Collect();
		ASSERT_EQ(source_body.Count(), sizeof(download));
		EXPECT_EQ(memcmp(source_body.ItemPtr(0), download, sizeof(download)), 0);

		EXPECT_THROW(client.Get(L"/too-large", static_cast<ISink<byte_t>*>(nullptr), 32), TException);
		EXPECT_EQ(THttpClient::DEFAULT_RESPONSE_BODY_LIMIT, 16U * 1024U * 1024U);
	}

	TEST(io_net_http, THttpClient_rejects_request_injection)
	{
		THttpClient client(L"localhost", 1);
		EXPECT_THROW(client.SetHeader(L"X-Test", L"valid\r\nInjected: yes"), TInvalidArgumentException);
		EXPECT_THROW(client.SetHeader(L"Bad Header", L"value"), TInvalidArgumentException);

		THttpClient::request_t request;
		request.url = L"/test\r\nInjected: yes";
		EXPECT_THROW(client.Request(std::move(request)), TInvalidArgumentException);

		THttpClient::request_t request_header;
		request_header.header_fields.Set(L"X-Test", L"valid\nInjected: yes");
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

		THttpClient client(L"localhost", tcp_server.LocalAddress().port);
		TList<byte_t> received;
		TListSink<byte_t> sink(&received);
		auto response = client.Get(L"/large.bin", &sink);
		EXPECT_EQ(response.status, EStatus::OK);
		EXPECT_EQ(response.body.Count(), 0U);
		ASSERT_EQ(received.Count(), expected.Count());
		EXPECT_EQ(memcmp(received.ItemPtr(0), expected.ItemPtr(0), expected.Count()), 0);
	}

	TEST(io_net_http, THttpClient_https)
	{
		TTcpServer tcp_server;
		tls::TServer tls_server(&tcp_server, L"support/tls-test-cert.pem", L"support/tls-test-key.pem");
		usys_t request_index = 0;
		THttpServer http_server(&tls_server, [&](const THttpServer::request_t& request, THttpServer::response_t& response) {
			request_index++;
			response.status = EStatus::OK;
			if(request_index == 1)
			{
				response.header_fields.Set(L"Set-Cookie", L"secure=yes; Path=/; Secure");
			}
			else
			{
				ASSERT_NE(request.header_fields.Get(L"cookie"), nullptr);
				EXPECT_EQ(*request.header_fields.Get(L"cookie"), L"secure=yes");
			}
		});

		tls::client_config_t config;
		config.ca_certificates = tls::TPemSource(TPath(L"support/tls-test-cert.pem"));
		THttpClient client(L"localhost", tls_server.LocalAddress().port, std::move(config));
		EXPECT_EQ(client.Get(L"/one").status, EStatus::OK);
		EXPECT_EQ(client.Get(L"/two").status, EStatus::OK);
		EXPECT_EQ(request_index, 2U);
	}

	TEST(io_net_http, THttpClient_https_credentials_from_memory)
	{
		TList<byte_t> certificate = TFile(L"support/tls-test-cert.pem").Pipe().Collect();
		TList<byte_t> private_key = TFile(L"support/tls-test-key.pem").Pipe().Collect();

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
		THttpClient client(L"localhost", tls_server.LocalAddress().port, std::move(client_config));
		EXPECT_EQ(client.Get(L"/").status, EStatus::OK);
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
			EXPECT_TRUE(request_header.Contains(L"Transfer-Encoding: chunked"));

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
					EXPECT_EQ(read_line(*client), L"");
					break;
				}

				const usys_t offset = received.Count();
				received.Inflate(chunk_size, 0);
				client->ReadAll(received.ItemPtr(offset), chunk_size);
				EXPECT_EQ(read_line(*client), L"");
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

		THttpClient client(L"localhost", tcp_server.LocalAddress().port);
		auto response = client.Post(L"/chunked", upload_source);

		const char expected[] = "Wikipedia";
		ASSERT_EQ(response.body.Count(), sizeof(expected) - 1);
		EXPECT_EQ(memcmp(response.body.ItemPtr(0), expected, sizeof(expected) - 1), 0);
		ASSERT_NE(response.FindHeader(L"x-trailer"), nullptr);
		EXPECT_EQ(*response.FindHeader(L"x-trailer"), L"done");
		EXPECT_EQ(raw_server.Join(), nullptr);
	}

}
