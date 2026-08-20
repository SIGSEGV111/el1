#include <gtest/gtest.h>
#include <el1/io_net_quic.hpp>
#include <el1/io_file.hpp>
#include <el1/system_task.hpp>


using namespace ::testing;

namespace
{
	using namespace el1::io::net;
	using namespace el1::io::net::ip;
	using namespace el1::io::net::quic;
	using namespace el1::io::file;
	using namespace el1::system::task;

	static void WriteAll(TStream& stream, const byte_t* const data, const usys_t size)
	{
		usys_t offset = 0;
		while(offset < size)
		{
			const usys_t written = stream.Write(data + offset, size - offset);
			if(written != 0)
			{
				offset += written;
				continue;
			}
			const auto* const waitable = stream.OnOutputReady();
			ASSERT_NE(waitable, nullptr);
			waitable->WaitFor();
		}
	}

	static void ReadAll(TStream& stream, byte_t* const data, const usys_t size)
	{
		usys_t offset = 0;
		while(offset < size)
		{
			const usys_t n = stream.Read(data + offset, size - offset);
			if(n == 0)
			{
				const auto* const waitable = stream.OnInputReady();
				ASSERT_NE(waitable, nullptr);
				waitable->WaitFor();
				continue;
			}
			offset += n;
		}
	}

	TEST(io_net_quic, Supported)
	{
		if(!IsSupported())
			GTEST_SKIP() << "QUIC requires OpenSSL 3.5 or newer";
		EXPECT_TRUE(IsSupported());
	}

	TEST(io_net_quic, BidirectionalStreamLoopback)
	{
		if(!IsSupported())
			GTEST_SKIP() << "QUIC requires OpenSSL 3.5 or newer";

		TUdpSocket udp(ipaddr_t(U"127.0.0.1"));
		server_config_t server_config;
		server_config.certificate_chain = tls::TPemSource(TPath(U"support/tls-test-cert.pem"));
		server_config.private_key = tls::TPemSource(TPath(U"support/tls-test-key.pem"));
		server_config.application_protocol = U"el1-test";
		TServer server(&udp, std::move(server_config));

		TThread server_thread(U"quic-test-server", [&]()
		{
				auto connection = server.AcceptConnection();
				EXPECT_EQ(connection->ApplicationProtocol(), U"el1-test");
				auto stream = connection->AcceptStream();
				EXPECT_TRUE(stream->CanRead());
				EXPECT_TRUE(stream->CanWrite());
				byte_t request[5] = {};
				ReadAll(*stream, request, sizeof(request));
				EXPECT_EQ(memcmp(request, "hello", sizeof(request)), 0);
				const byte_t response[] = { 'w', 'o', 'r', 'l', 'd' };
				WriteAll(*stream, response, sizeof(response));
				EXPECT_TRUE(stream->CloseOutput());
		});

		client_config_t client_config;
		client_config.server_name = U"localhost";
		client_config.ca_certificates = tls::TPemSource(TPath(U"support/tls-test-cert.pem"));
		client_config.application_protocol = U"el1-test";
		TClient client({ ipaddr_t(U"127.0.0.1"), server.LocalAddress().port }, std::move(client_config));
		EXPECT_EQ(client.ApplicationProtocol(), U"el1-test");
		auto stream = client.OpenStream();
		const byte_t request[] = { 'h', 'e', 'l', 'l', 'o' };
		WriteAll(*stream, request, sizeof(request));
		EXPECT_TRUE(stream->CloseOutput());
		byte_t response[5] = {};
		ReadAll(*stream, response, sizeof(response));
		EXPECT_EQ(memcmp(response, "world", sizeof(response)), 0);

		EXPECT_EQ(server_thread.Join(), nullptr);
	}

	TEST(io_net_quic, MultipleStreamsAndUnidirectionalStream)
	{
		if(!IsSupported())
			GTEST_SKIP() << "QUIC requires OpenSSL 3.5 or newer";

		TUdpSocket udp(ipaddr_t(U"127.0.0.1"));
		server_config_t server_config;
		server_config.certificate_chain = tls::TPemSource(TPath(U"support/tls-test-cert.pem"));
		server_config.private_key = tls::TPemSource(TPath(U"support/tls-test-key.pem"));
		server_config.application_protocol = U"el1-test";
		TServer server(&udp, std::move(server_config));

		TThread server_thread(U"quic-test-server", [&]()
		{
				auto connection = server.AcceptConnection();
				auto first = connection->AcceptStream();
				auto second = connection->AcceptStream();
				EXPECT_NE(first->Id(), second->Id());
				byte_t a = 0;
				byte_t b = 0;
				ReadAll(*first, &a, 1);
				ReadAll(*second, &b, 1);
				EXPECT_EQ(a, 1U);
				EXPECT_EQ(b, 2U);

				auto uni = connection->AcceptStream();
				EXPECT_TRUE(uni->CanRead());
				EXPECT_FALSE(uni->CanWrite());
				byte_t c = 0;
				ReadAll(*uni, &c, 1);
				EXPECT_EQ(c, 3U);
		});

		client_config_t client_config;
		client_config.server_name = U"localhost";
		client_config.ca_certificates = tls::TPemSource(TPath(U"support/tls-test-cert.pem"));
		client_config.application_protocol = U"el1-test";
		TClient client({ ipaddr_t(U"127.0.0.1"), server.LocalAddress().port }, std::move(client_config));
		auto first = client.OpenStream();
		auto second = client.OpenStream();
		const byte_t a = 1;
		const byte_t b = 2;
		WriteAll(*first, &a, 1);
		WriteAll(*second, &b, 1);
		first->CloseOutput();
		second->CloseOutput();

		auto uni = client.OpenStream(true);
		EXPECT_FALSE(uni->CanRead());
		EXPECT_TRUE(uni->CanWrite());
		const byte_t c = 3;
		WriteAll(*uni, &c, 1);
		uni->CloseOutput();

		EXPECT_EQ(server_thread.Join(), nullptr);
	}
}
