#include <gtest/gtest.h>
#include <el1/io_net_rtp.hpp>
#include <cstring>

using namespace ::testing;

namespace
{
	using namespace el1::io::net::rtp;
	using namespace el1::io::collection::array;
	using namespace el1::io::types;

	TEST(io_net_rtp, BuildAndParsePacket)
	{
		byte_t payload[] = { 1, 2, 3, 4, 5 };
		byte_t packet[64] = {};
		const usys_t n_packet = BuildPacket(packet, 96, 0xfffeU, 0x12345678U, 0xaabbccddU, payload, true);
		ASSERT_EQ(n_packet, 17U);

		packet_view_t view;
		ASSERT_TRUE(ParsePacket(array_t<const byte_t>::FromUnsafePointer(packet, n_packet), view));
		EXPECT_TRUE(view.marker);
		EXPECT_EQ(view.payload_type, 96U);
		EXPECT_EQ(view.sequence, 0xfffeU);
		EXPECT_EQ(view.timestamp, 0x12345678U);
		EXPECT_EQ(view.ssrc, 0xaabbccddU);
		ASSERT_EQ(view.payload.Count(), sizeof(payload));
		EXPECT_EQ(memcmp(view.payload.ItemPtr(0), payload, sizeof(payload)), 0);
	}

	TEST(io_net_rtp, ParseExtensionAndPadding)
	{
		const byte_t packet[] = {
			0xb1, 0x61, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
			0xde, 0xad, 0xbe, 0xef,
			0x10, 0x00, 0x00, 0x01,
			0xaa, 0xbb, 0xcc, 0xdd,
			0x11, 0x22, 0x00, 0x02
		};
		packet_view_t view;
		ASSERT_TRUE(ParsePacket(packet, view));
		ASSERT_EQ(view.payload.Count(), 2U);
		EXPECT_EQ(view.payload[0], 0x11U);
		EXPECT_EQ(view.payload[1], 0x22U);
	}

	TEST(io_net_rtp, RejectMalformedPackets)
	{
		const byte_t short_packet[] = { 0x80, 0x60 };
		packet_view_t view;
		EXPECT_FALSE(ParsePacket(short_packet, view));

		byte_t wrong_version[12] = {};
		wrong_version[0] = 0x40;
		EXPECT_FALSE(ParsePacket(wrong_version, view));

		byte_t output[12] = {};
		EXPECT_EQ(BuildPacket(output, 128U, 0, 0, 0, array_t<const byte_t>{}), 0U);
	}

	TEST(io_net_rtp, SequenceDistanceWraps)
	{
		EXPECT_EQ(SequenceDistance(10U, 7U), 3U);
		EXPECT_EQ(SequenceDistance(1U, 0xffffU), 2U);
	}
}
