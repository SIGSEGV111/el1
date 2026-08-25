#include "io_net_rtp.hpp"
#include <cstring>

namespace el1::io::net::rtp
{
	static u16_t ReadU16(const byte_t* const data)
	{
		return (u16_t)(((u16_t)data[0] << 8U) | data[1]);
	}

	static u32_t ReadU32(const byte_t* const data)
	{
		return ((u32_t)data[0] << 24U) |
			((u32_t)data[1] << 16U) |
			((u32_t)data[2] << 8U) |
			(u32_t)data[3];
	}

	static void WriteU16(byte_t* const data, const u16_t value)
	{
		data[0] = (byte_t)(value >> 8U);
		data[1] = (byte_t)value;
	}

	static void WriteU32(byte_t* const data, const u32_t value)
	{
		data[0] = (byte_t)(value >> 24U);
		data[1] = (byte_t)(value >> 16U);
		data[2] = (byte_t)(value >> 8U);
		data[3] = (byte_t)value;
	}

	bool ParsePacket(const array_t<const byte_t> packet, packet_view_t& view)
	{
		if(packet.Count() < 12U || (packet[0] >> 6U) != 2U)
			return false;

		const bool has_padding = (packet[0] & 0x20U) != 0;
		const bool has_extension = (packet[0] & 0x10U) != 0;
		const usys_t n_csrc = packet[0] & 0x0fU;
		usys_t offset = 12U + n_csrc * 4U;
		if(offset > packet.Count())
			return false;

		if(has_extension)
		{
			if(offset + 4U > packet.Count())
				return false;
			const usys_t n_extension_words = ReadU16(packet.ItemPtr(offset + 2U));
			offset += 4U + n_extension_words * 4U;
			if(offset > packet.Count())
				return false;
		}

		usys_t payload_end = packet.Count();
		if(has_padding)
		{
			if(payload_end == offset)
				return false;
			const usys_t n_padding = packet[-1];
			if(n_padding == 0U || n_padding > payload_end - offset)
				return false;
			payload_end -= n_padding;
		}

		view.marker = (packet[1] & 0x80U) != 0;
		view.payload_type = packet[1] & 0x7fU;
		view.sequence = ReadU16(packet.ItemPtr(2));
		view.timestamp = ReadU32(packet.ItemPtr(4));
		view.ssrc = ReadU32(packet.ItemPtr(8));
		view.payload = packet.Slice((ssys_t)offset, payload_end - offset);
		return true;
	}

	usys_t BuildPacket(
		array_t<byte_t> output,
		const u8_t payload_type,
		const u16_t sequence,
		const u32_t timestamp,
		const u32_t ssrc,
		const array_t<const byte_t> payload,
		const bool marker)
	{
		const usys_t n_required = 12U + payload.Count();
		if(output.Count() < n_required || payload_type > 127U)
			return 0U;

		output[0] = 0x80U;
		output[1] = payload_type | (marker ? 0x80U : 0U);
		WriteU16(output.ItemPtr(2), sequence);
		WriteU32(output.ItemPtr(4), timestamp);
		WriteU32(output.ItemPtr(8), ssrc);
		if(!payload.IsEmpty())
			memmove(output.ItemPtr(12), payload.ItemPtr(0), payload.Count());
		return n_required;
	}

	u16_t SequenceDistance(const u16_t newer, const u16_t older)
	{
		return (u16_t)(newer - older);
	}
}
