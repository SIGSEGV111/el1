#include "util_bits.hpp"
#include <math.h>

namespace el1::util::bits
{
	using namespace error;

	void SetBit(byte_t* const buffer, const usys_t idx_bit, const bool value)
	{
		const usys_t idx_byte = idx_bit / 8U;
		const byte_t mask = MakeBitMask8(idx_bit - idx_byte * 8U);

		if(value)
			buffer[idx_byte] |= mask;
		else
			buffer[idx_byte] &= (~mask);
	}

	bool GetBit(const byte_t* const buffer, const usys_t idx_bit)
	{
		const usys_t idx_byte = idx_bit / 8U;
		const byte_t mask = MakeBitMask8(idx_bit - idx_byte * 8U);
		return (buffer[idx_byte] & mask) != 0;
	}

	void FillBits(byte_t* const buffer, const usys_t n_bits, const byte_t pattern)
	{
		byte_t rot = pattern;
		const byte_t first_bit = 0b00000001;

		for(usys_t i = 0; i < n_bits; i++)
		{
			SetBit(buffer, i, (rot & first_bit) != 0);
			rot = RotateRight8(rot, 1);
		}
	}

	usys_t TimeToBits(const double hz_clock, const system::time::TTime time)
	{
		return (usys_t)lrint(ceil(hz_clock * time.ConvertToF(system::time::EUnit::SECONDS)));
	}

	usys_t BitsToFullBytes(const usys_t n_bits)
	{
		usys_t n_bytes = n_bits / 8U;
		if(n_bytes * 8 < n_bits)
			n_bytes++;
		return n_bytes;
	}

	unsigned CountOneBits(u8_t  in)
	{
		return __builtin_popcount(in);
	}

	unsigned CountOneBits(u16_t in)
	{
		return __builtin_popcount(in);
	}

	unsigned CountOneBits(u32_t in)
	{
		return __builtin_popcountl(in);
	}

	unsigned CountOneBits(u64_t in)
	{
		return __builtin_popcountll(in);
	}
}
