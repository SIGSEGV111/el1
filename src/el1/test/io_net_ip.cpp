#include <gtest/gtest.h>
#include <el1/error.hpp>
#include <el1/io_net_ip.hpp>
#include <el1/io_collection_list.hpp>
#include <el1/system_task.hpp>
#include <stdlib.h>

using namespace ::testing;

namespace
{
	using namespace el1::io::net::ip;
	using namespace el1::io::collection::list;
	using namespace el1::error;
	using namespace el1::system::task;

	TEST(io_net_ip, EnumMyIpAddresses)
	{
		auto addrs = EnumMyIpAddresses();
		EXPECT_GE(addrs.Count(), 3U);
		EXPECT_TRUE(addrs.Contains(ipaddr_t(U"127.0.0.1")));
		EXPECT_TRUE(addrs.Contains(ipaddr_t(U"::1")));
	}

	TEST(io_net_ip_ipaddr_t, IsLoopback)
	{
		ipaddr_t a1(U"127.0.0.1");
		ipaddr_t a2(U"127.0.0.2");
		ipaddr_t a3(U"10.42.13.1");
		ipaddr_t a4(U"10.42.12.2");
		ipaddr_t a5(U"::1");
		ipaddr_t a6(U"::2");

		EXPECT_TRUE(a1.IsLoopback());
		EXPECT_TRUE(a2.IsLoopback());
		EXPECT_FALSE(a3.IsLoopback());
		EXPECT_FALSE(a4.IsLoopback());
		EXPECT_TRUE(a5.IsLoopback());
		EXPECT_FALSE(a6.IsLoopback());
	}

		TEST(io_net_ip_ipaddr_t, MatchingPrefixLength)
	{
		ipaddr_t a1(U"127.0.0.1");
		ipaddr_t a2(U"127.0.0.2");
		ipaddr_t a3(U"10.42.13.1");
		ipaddr_t a4(U"10.42.12.2");

		EXPECT_EQ(a1.MatchingPrefixLength(a1), 128U);
		EXPECT_EQ(a1.MatchingPrefixLength(a2), 126U);
		EXPECT_EQ(a3.MatchingPrefixLength(a4), 128U - 8U - 1U);
	}

	TEST(io_net_ip_ipaddr_t, parse)
	{
		ipaddr_t(U"127.0.0.1");
		ipaddr_t(U"10.42.13.114");
		ipaddr_t(U"192.168.168.2");
		ipaddr_t(U"::1");
		ipaddr_t(U"fe80::aaa1:59ff:fe18:a945");
		ipaddr_t(U"0001:0002:0003:0004:0005:0006:0007:0008");

		EXPECT_THROW(ipaddr_t(U"0001:0002:0003:0004:0005:0006:0007:0008", EIP::V4), TException);
		EXPECT_THROW(ipaddr_t(U"127.0.0.1", EIP::V6), TException);

		EXPECT_THROW(ipaddr_t(U"127.0.0.256"), TException);
		EXPECT_THROW(ipaddr_t(U"fÜÄÖ::aaa1:59ff:fe18:a945"), TException);
		EXPECT_THROW(ipaddr_t(U"fe80::aaa1:59ff:fe18:-a945"), TException);
		EXPECT_THROW(ipaddr_t(U"0001:0002:0003:0004:0005:0006:0007:0008:0009"), TException);

		EXPECT_THROW(ipaddr_t(U"hello world"), TException);
	}

	TEST(io_net_ip, ResolveHostname)
	{
		{
			const TList<ipaddr_t> addrs = ResolveHostname(U"localhost");
			EXPECT_TRUE(addrs.Contains(ipaddr_t(U"127.0.0.1")));
			EXPECT_TRUE(addrs.Contains(ipaddr_t(U"::1")));
		}

		{
			// this obviously will need an update from time to time ...
			const TList<ipaddr_t> addrs = ResolveHostname(U"heise.de");
			EXPECT_TRUE(addrs.Contains(ipaddr_t(U"193.99.144.80")));
			EXPECT_TRUE(addrs.Contains(ipaddr_t(U"2a02:2e0:3fe:1001:302::")));
		}
	}

	TEST(io_net_ip, TUdpNode_construct)
	{
		{
			TUdpSocket udp;
			EXPECT_NE(udp.LocalAddress().port, 0U);
		}

		{
			TUdpNode udp(0U, EIP::V4);
			EXPECT_TRUE(udp.LocalAddress().ip.IsV4());
			EXPECT_FALSE(udp.LocalAddress().ip.IsV6());
			EXPECT_NE(udp.LocalAddress().port, 0U);
		}

		{
			TUdpNode udp(0U, EIP::V6);
			EXPECT_TRUE(udp.LocalAddress().ip.IsV6());
			EXPECT_FALSE(udp.LocalAddress().ip.IsV4());
			EXPECT_NE(udp.LocalAddress().port, 0U);
		}

		{
			TUdpNode udp(ipaddr_t(U"127.0.0.1"));
			EXPECT_TRUE(udp.LocalAddress().ip.IsV4());
			EXPECT_FALSE(udp.LocalAddress().ip.IsV6());
		}

		{
			TUdpNode udp(ipaddr_t(U"::1"));
			EXPECT_TRUE(udp.LocalAddress().ip.IsV6());
			EXPECT_FALSE(udp.LocalAddress().ip.IsV4());
		}
	}

	TEST(io_net_ip, TUdpNode_loopback_ipv4)
	{
		TUdpNode sender(ipaddr_t(U"127.0.0.1"));
		TUdpNode receiver(ipaddr_t(U"127.0.0.1"));
		const byte_t tx[] = { 0,1,2,3,4,5,6,7,8,9 };

		EXPECT_TRUE(sender.Send(receiver.LocalAddress(), tx, sizeof(tx)));
		EXPECT_TRUE(receiver.OnReceiveMsg().WaitFor(1));

		auto datagram = receiver.Receive();
		ASSERT_TRUE(datagram.has_value());
		EXPECT_EQ(datagram->source, sender.LocalAddress());
		ASSERT_EQ(datagram->data.Count(), sizeof(tx));
		EXPECT_EQ(memcmp(datagram->data.ItemPtr(0), tx, sizeof(tx)), 0);
		EXPECT_FALSE(receiver.Receive().has_value());
	}

	TEST(io_net_ip, TUdpNode_loopback_ipv6)
	{
		TUdpNode sender(ipaddr_t(U"::1"));
		TUdpNode receiver(ipaddr_t(U"::1"));
		const byte_t tx[] = { 10,9,8,7,6,5,4,3,2,1 };

		EXPECT_TRUE(sender.Send(receiver.LocalAddress(), tx, sizeof(tx)));
		EXPECT_TRUE(receiver.OnReceiveMsg().WaitFor(1));

		udp_datagram_t datagram;
		ASSERT_TRUE(receiver.Receive(datagram));
		EXPECT_EQ(datagram.source, sender.LocalAddress());
		ASSERT_EQ(datagram.data.Count(), sizeof(tx));
		EXPECT_EQ(memcmp(datagram.data.ItemPtr(0), tx, sizeof(tx)), 0);
	}

	TEST(io_net_ip, TUdpNode_dualstack_ipv4_send)
	{
		TUdpNode sender;
		TUdpNode receiver(ipaddr_t(U"127.0.0.1"));
		const byte_t tx[] = { 1,2,3,4 };

		EXPECT_TRUE(sender.Send(ipport_t{ipaddr_t(U"127.0.0.1"), receiver.LocalAddress().port}, tx, sizeof(tx)));
		EXPECT_TRUE(receiver.OnReceiveMsg().WaitFor(1));

		auto datagram = receiver.Receive();
		ASSERT_TRUE(datagram.has_value());
		ASSERT_EQ(datagram->data.Count(), sizeof(tx));
		EXPECT_EQ(memcmp(datagram->data.ItemPtr(0), tx, sizeof(tx)), 0);
	}

	TEST(io_net_ip, TUdpNode_dualstack_ipv4_receive)
	{
		TUdpNode sender(ipaddr_t(U"127.0.0.1"));
		TUdpNode receiver;
		const byte_t tx[] = { 5,6,7,8 };

		EXPECT_TRUE(sender.Send(ipaddr_t(U"127.0.0.1"), receiver.LocalAddress().port, tx, sizeof(tx)));
		EXPECT_TRUE(receiver.OnReceiveMsg().WaitFor(1));

		auto datagram = receiver.Receive();
		ASSERT_TRUE(datagram.has_value());
		EXPECT_EQ(datagram->source, sender.LocalAddress());
		ASSERT_EQ(datagram->data.Count(), sizeof(tx));
		EXPECT_EQ(memcmp(datagram->data.ItemPtr(0), tx, sizeof(tx)), 0);
	}

	TEST(io_net_ip, TUdpNode_preserves_datagram_boundaries_and_empty_datagrams)
	{
		TUdpNode sender(ipaddr_t(U"127.0.0.1"));
		TUdpNode receiver(ipaddr_t(U"127.0.0.1"));
		const ipport_t target = receiver.LocalAddress();
		const byte_t first[] = { 1,2,3 };
		const byte_t second[] = { 4,5 };

		EXPECT_TRUE(sender.Send(target, first, sizeof(first)));
		EXPECT_TRUE(sender.Send(target, nullptr, 0));
		EXPECT_TRUE(sender.Send(target, second, sizeof(second)));

		auto a = receiver.Receive();
		auto b = receiver.Receive();
		auto c = receiver.Receive();
		ASSERT_TRUE(a.has_value());
		ASSERT_TRUE(b.has_value());
		ASSERT_TRUE(c.has_value());
		EXPECT_EQ(a->data.Count(), 3U);
		EXPECT_EQ(b->data.Count(), 0U);
		EXPECT_EQ(c->data.Count(), 2U);
		EXPECT_EQ(a->data[0], 1U);
		EXPECT_EQ(c->data[0], 4U);
	}

	TEST(io_net_ip, TUdpNode_resizes_receive_buffer)
	{
		TUdpNode sender(ipaddr_t(U"127.0.0.1"));
		TUdpNode receiver(ipaddr_t(U"127.0.0.1"));
		TList<byte_t> tx;
		for(usys_t i = 0; i < 4096; i++)
			tx.Append((byte_t)i);

		EXPECT_TRUE(sender.Send(receiver.LocalAddress(), tx));
		EXPECT_TRUE(receiver.OnReceiveMsg().WaitFor(1));
		auto datagram = receiver.Receive();
		ASSERT_TRUE(datagram.has_value());
		ASSERT_EQ(datagram->data.Count(), tx.Count());
		EXPECT_EQ(memcmp(datagram->data.ItemPtr(0), tx.ItemPtr(0), tx.Count()), 0);
	}

	TEST(io_net_ip, TUdpNode_hostname_send)
	{
		TUdpNode sender(ipaddr_t(U"127.0.0.1"));
		TUdpNode receiver(ipaddr_t(U"127.0.0.1"));
		const byte_t tx[] = { 42 };

		EXPECT_TRUE(sender.Send(U"localhost", receiver.LocalAddress().port, tx));
		EXPECT_TRUE(receiver.OnReceiveMsg().WaitFor(1));
		auto datagram = receiver.Receive();
		ASSERT_TRUE(datagram.has_value());
		ASSERT_EQ(datagram->data.Count(), 1U);
		EXPECT_EQ(datagram->data[0], 42U);
	}

	TEST(io_net_ip, TUdpNode_ipv4_socket_rejects_ipv6_target)
	{
		TUdpNode sender(ipaddr_t(U"127.0.0.1"));
		const byte_t tx[] = { 1 };
		EXPECT_THROW({ const bool sent = sender.Send(ipport_t{ipaddr_t(U"::1"), 9U}, tx, sizeof(tx)); (void)sent; }, TInvalidArgumentException);
	}

	TEST(io_net_ip, TTcpServer_construct)
	{
		{
			TTcpServer server(12121U);
			EXPECT_TRUE(server.LocalAddress().ip.IsV6());
			EXPECT_FALSE(server.LocalAddress().ip.IsV4());
		}

		{
			TTcpServer server(ipaddr_t(U"127.0.0.1"), 12121U);
			EXPECT_TRUE(server.LocalAddress().ip.IsV4());
			EXPECT_FALSE(server.LocalAddress().ip.IsV6());
		}

		{
			TTcpServer server(ipaddr_t(U"::1"), 12121U);
			EXPECT_TRUE(server.LocalAddress().ip.IsV6());
			EXPECT_FALSE(server.LocalAddress().ip.IsV4());
		}
	}

	TEST(io_net_ip, TTcpServer_loopback_simple)
	{
		{
			TFiber fib_server([](){
				TTcpServer server(ipaddr_t(U"127.0.0.1"), 12121U);

				for(;;)
				{
					// std::cerr<<"waiting for client to connect ...\n";
					EXPECT_TRUE(server.OnClientConnect().WaitFor(5));
					// std::cerr<<"client incomming!\n";
					auto client = server.AcceptClient();
					// std::cerr<<"client connected!\n";

					for(;;)
					{
						byte_t buffer[128];
						const usys_t r = client->Read(buffer, sizeof(buffer));
						if(r == 0)
						{
							if(client->OnInputReady() == nullptr)
								break;

							EXPECT_TRUE(client->OnInputReady()->WaitFor(5));
							continue;
						}

						client->WriteAll(buffer, r);
					}

					// std::cerr<<"client won't send data anymore\n";
					client->Close();
				}
			});

			fib_server.SwitchTo();

			TTcpClient client(ipaddr_t(U"127.0.0.1"), 12121U);
			char buffer[12];
			// std::cerr<<"waiting for server to be ready to accept data ...\n";
			EXPECT_TRUE(client.OnOutputReady()->WaitFor(5));
			// std::cerr<<"sending data ...\n";
			EXPECT_EQ(client.Write((const byte_t*)"hello world", 12U), 12U);
			// std::cerr<<"waiting for data from server ...\n";
			EXPECT_TRUE(client.OnInputReady()->WaitFor(5));
			// std::cerr<<"reading for data from server ...\n";
			EXPECT_EQ(client.Read((byte_t*)buffer, 12U), 12U);
			// std::cerr<<"comparing data ...\n";
			EXPECT_TRUE(strncmp(buffer, "hello world", 12U) == 0);
			// std::cerr<<"shutting down\n";
			client.Close();
		}
	}
}
