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
		EXPECT_TRUE(addrs.Contains(ipaddr_t("127.0.0.1")));
		EXPECT_TRUE(addrs.Contains(ipaddr_t("::1")));
	}

	TEST(io_net_ip_ipaddr_t, IsLoopback)
	{
		ipaddr_t a1("127.0.0.1");
		ipaddr_t a2("127.0.0.2");
		ipaddr_t a3("10.42.13.1");
		ipaddr_t a4("10.42.12.2");
		ipaddr_t a5("::1");
		ipaddr_t a6("::2");

		EXPECT_TRUE(a1.IsLoopback());
		EXPECT_TRUE(a2.IsLoopback());
		EXPECT_FALSE(a3.IsLoopback());
		EXPECT_FALSE(a4.IsLoopback());
		EXPECT_TRUE(a5.IsLoopback());
		EXPECT_FALSE(a6.IsLoopback());
	}

		TEST(io_net_ip_ipaddr_t, MatchingPrefixLength)
	{
		ipaddr_t a1("127.0.0.1");
		ipaddr_t a2("127.0.0.2");
		ipaddr_t a3("10.42.13.1");
		ipaddr_t a4("10.42.12.2");

		EXPECT_EQ(a1.MatchingPrefixLength(a1), 128U);
		EXPECT_EQ(a1.MatchingPrefixLength(a2), 126U);
		EXPECT_EQ(a3.MatchingPrefixLength(a4), 128U - 8U - 1U);
	}

	TEST(io_net_ip_ipaddr_t, parse)
	{
		ipaddr_t("127.0.0.1");
		ipaddr_t("10.42.13.114");
		ipaddr_t("192.168.168.2");
		ipaddr_t("::1");
		ipaddr_t("fe80::aaa1:59ff:fe18:a945");
		ipaddr_t("0001:0002:0003:0004:0005:0006:0007:0008");

		EXPECT_THROW(ipaddr_t("0001:0002:0003:0004:0005:0006:0007:0008", EIP::V4), TException);
		EXPECT_THROW(ipaddr_t("127.0.0.1", EIP::V6), TException);

		EXPECT_THROW(ipaddr_t("127.0.0.256"), TException);
		EXPECT_THROW(ipaddr_t("fÜÄÖ::aaa1:59ff:fe18:a945"), TException);
		EXPECT_THROW(ipaddr_t("fe80::aaa1:59ff:fe18:-a945"), TException);
		EXPECT_THROW(ipaddr_t("0001:0002:0003:0004:0005:0006:0007:0008:0009"), TException);

		EXPECT_THROW(ipaddr_t("hello world"), TException);
	}

	TEST(io_net_ip, ResolveHostname)
	{
		{
			const TList<ipaddr_t> addrs = ResolveHostname("localhost");
			EXPECT_TRUE(addrs.Contains(ipaddr_t("127.0.0.1")));
			EXPECT_TRUE(addrs.Contains(ipaddr_t("::1")));
		}

		{
			// this obviously will need an update from time to time ...
			const TList<ipaddr_t> addrs = ResolveHostname("heise.de");
			EXPECT_TRUE(addrs.Contains(ipaddr_t("193.99.144.80")));
			EXPECT_TRUE(addrs.Contains(ipaddr_t("2a02:2e0:3fe:1001:302::")));
		}
	}

	TEST(io_net_ip, TUdpNode_construct)
	{
		{
			TUdpNode udp(12121U, EIP::ANY);
		}

		{
			TUdpNode udp(12121U, EIP::V4);
			EXPECT_TRUE(udp.LocalAddress().ip.IsV4());
			EXPECT_FALSE(udp.LocalAddress().ip.IsV6());
		}

		{
			TUdpNode udp(12121U, EIP::V6);
			EXPECT_TRUE(udp.LocalAddress().ip.IsV6());
			EXPECT_FALSE(udp.LocalAddress().ip.IsV4());
		}

		{
			TUdpNode udp(ipaddr_t("127.0.0.1"), 12121U);
			EXPECT_TRUE(udp.LocalAddress().ip.IsV4());
			EXPECT_FALSE(udp.LocalAddress().ip.IsV6());
		}

		{
			TUdpNode udp(ipaddr_t("::1"), 12121U);
			EXPECT_TRUE(udp.LocalAddress().ip.IsV6());
			EXPECT_FALSE(udp.LocalAddress().ip.IsV4());
		}
	}

	TEST(io_net_ip, TUdpNode_loopback_simple)
	{
		{
			TUdpNode node1(ipaddr_t("::1"), 12121U);
			TUdpNode node2(ipaddr_t("::1"), 12122U);

			byte_t tx[] = { 0,1,2,3,4,5,6,7,8,9 };
			TList<byte_t> rx;

			EXPECT_TRUE(node1.Send(node2.LocalAddress().ip, node2.LocalAddress().port, tx, sizeof(tx)));

			EXPECT_TRUE(node2.OnReceiveMsg().WaitFor(1));
			ipaddr_t rx_ip;
			port_t rx_port;
			EXPECT_TRUE(node2.Receive(rx, &rx_ip, &rx_port));
			EXPECT_EQ(rx_ip, ipaddr_t("::1"));
			EXPECT_EQ(rx_port, 12121U);

			EXPECT_TRUE(rx.Count() == sizeof(tx));
			EXPECT_TRUE(memcmp(rx.ItemPtr(0), tx, sizeof(tx)) == 0);

			EXPECT_FALSE(node2.Receive(rx));
		}

		{
			TUdpNode node1(ipaddr_t("127.0.0.1"), 12121U);
			TUdpNode node2(ipaddr_t("127.0.0.1"), 12122U);

			byte_t tx[] = { 0,1,2,3,4,5,6,7,8,9 };
			TList<byte_t> rx;

			EXPECT_TRUE(node1.Send(node2.LocalAddress().ip, node2.LocalAddress().port, tx, sizeof(tx)));

			EXPECT_TRUE(node2.OnReceiveMsg().WaitFor(1));
			ipaddr_t rx_ip;
			port_t rx_port;
			EXPECT_TRUE(node2.Receive(rx, &rx_ip, &rx_port));
			EXPECT_EQ(rx_ip, ipaddr_t("127.0.0.1"));
			EXPECT_EQ(rx_port, 12121U);

			EXPECT_TRUE(rx.Count() == sizeof(tx));
			EXPECT_TRUE(memcmp(rx.ItemPtr(0), tx, sizeof(tx)) == 0);

			EXPECT_FALSE(node2.Receive(rx));
		}
	}

	TEST(io_net_ip, TUdpNode_loopback_dualstack)
	{
		{
			TUdpNode node1(ipaddr_t("127.0.0.1"), 12121U);
			TUdpNode node2(12122U);

			byte_t tx[] = { 0,1,2,3,4,5,6,7,8,9 };
			TList<byte_t> rx;

			EXPECT_TRUE(node1.Send(ipaddr_t("127.0.0.1"), 12122U, tx, sizeof(tx)));

			if(node2.OnReceiveMsg().WaitFor(1))
			{
				ipaddr_t rx_ip;
				port_t rx_port;
				EXPECT_TRUE(node2.Receive(rx, &rx_ip, &rx_port));
				EXPECT_EQ(rx_ip, ipaddr_t("127.0.0.1"));
				EXPECT_EQ(rx_port, 12121U);

				EXPECT_TRUE(rx.Count() == sizeof(tx));
				EXPECT_TRUE(memcmp(rx.ItemPtr(0), tx, sizeof(tx)) == 0);

				EXPECT_FALSE(node2.Receive(rx));
			}
			else
			{
				::system("netstat -lupn");
				EXPECT_TRUE(false);
			}
		}

		{
			// dual-stack with one socket

			TUdpNode node1(12121U);
			TUdpNode node2(ipaddr_t("127.0.0.1"), 12122U);

			byte_t tx[] = { 0,1,2,3,4,5,6,7,8,9 };
			TList<byte_t> rx;

			EXPECT_TRUE(node1.Send(ipaddr_t("127.0.0.1"), 12122U, tx, sizeof(tx)));

			if(node2.OnReceiveMsg().WaitFor(1))
			{
				ipaddr_t rx_ip;
				port_t rx_port;
				EXPECT_TRUE(node2.Receive(rx, &rx_ip, &rx_port));
				EXPECT_EQ(rx_ip, ipaddr_t("127.0.0.1"));
				EXPECT_EQ(rx_port, 12121U);

				EXPECT_TRUE(rx.Count() == sizeof(tx));
				EXPECT_TRUE(memcmp(rx.ItemPtr(0), tx, sizeof(tx)) == 0);

				EXPECT_FALSE(node2.Receive(rx));
			}
			else
			{
				::system("netstat -lupn");
				EXPECT_TRUE(false);
			}
		}
	}

	TEST(io_net_ip, TUdpNode_loopback_varysize)
	{
		TUdpNode node1(ipaddr_t("127.0.0.1"), 12121U);
		TUdpNode node2(ipaddr_t("127.0.0.1"), 12122U);

		TList<byte_t> tx;
		TList<byte_t> rx;

		// 1024 bytes

		for(usys_t i = 0; i < 1024; i++)
			tx.Append((byte_t)i % 256);

		EXPECT_TRUE(node1.Send(ipaddr_t("127.0.0.1"), 12122U, tx));
		EXPECT_TRUE(node2.OnReceiveMsg().WaitFor(1));
		EXPECT_TRUE(node2.Receive(rx));

		EXPECT_EQ(tx.Count(), rx.Count());
		for(usys_t i = 0; i < 1024; i++)
			EXPECT_EQ(rx[i], ((byte_t)i % 256));

		// 1152 bytes

		for(usys_t i = 1024; i < 1152; i++)
			tx.Append((byte_t)i % 256);

		EXPECT_TRUE(node1.Send(ipaddr_t("127.0.0.1"), 12122U, tx));
		EXPECT_TRUE(node2.OnReceiveMsg().WaitFor(1));
		EXPECT_TRUE(node2.Receive(rx));

		EXPECT_EQ(tx.Count(), rx.Count());
		for(usys_t i = 0; i < 1152; i++)
			EXPECT_EQ(rx[i], ((byte_t)i % 256));

		// 8 bytes

		tx = { 0,1,2,3,4,5,6,7 };

		EXPECT_TRUE(node1.Send(ipaddr_t("127.0.0.1"), 12122U, tx));
		EXPECT_TRUE(node2.OnReceiveMsg().WaitFor(1));
		EXPECT_TRUE(node2.Receive(rx));

		EXPECT_EQ(tx.Count(), rx.Count());
		for(usys_t i = 0; i < 8; i++)
			EXPECT_EQ(rx[i], i);
	}

	TEST(io_net_ip, TTcpServer_construct)
	{
		{
			TTcpServer server(12121U);
			EXPECT_TRUE(server.LocalAddress().ip.IsV6());
			EXPECT_FALSE(server.LocalAddress().ip.IsV4());
		}

		{
			TTcpServer server(ipaddr_t("127.0.0.1"), 12121U);
			EXPECT_TRUE(server.LocalAddress().ip.IsV4());
			EXPECT_FALSE(server.LocalAddress().ip.IsV6());
		}

		{
			TTcpServer server(ipaddr_t("::1"), 12121U);
			EXPECT_TRUE(server.LocalAddress().ip.IsV6());
			EXPECT_FALSE(server.LocalAddress().ip.IsV4());
		}
	}

	TEST(io_net_ip, TTcpServer_loopback_simple)
	{
		{
			TFiber fib_server([](){
				TTcpServer server(ipaddr_t("127.0.0.1"), 12121U);

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

			TTcpClient client(ipaddr_t("127.0.0.1"), 12121U);
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
