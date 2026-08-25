#pragma once

#include "io_types.hpp"
#include "io_collection_array.hpp"

namespace el1::io::net::rtp
{
	using namespace io::types;
	using io::collection::array::array_t;

	struct packet_view_t
	{
		bool marker = false;
		u8_t payload_type = 0;
		u16_t sequence = 0;
		u32_t timestamp = 0;
		u32_t ssrc = 0;
		array_t<const byte_t> payload;
	};

	// RTP version 2 packet parser. CSRC lists, header extensions and padding are
	// accepted and skipped. The payload view references the supplied packet.
	bool ParsePacket(array_t<const byte_t> packet, packet_view_t& view);

	// Builds the fixed RTP v2 header followed by payload. Returns the packet size,
	// or 0 when output is too small or payload_type is outside the 7-bit range.
	usys_t BuildPacket(
		array_t<byte_t> output,
		u8_t payload_type,
		u16_t sequence,
		u32_t timestamp,
		u32_t ssrc,
		array_t<const byte_t> payload,
		bool marker = false);

	// Wrap-safe unsigned sequence distance used for short-range RTP ordering.
	u16_t SequenceDistance(u16_t newer, u16_t older);
}
