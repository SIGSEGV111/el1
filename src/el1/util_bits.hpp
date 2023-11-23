#pragma once

#include "error.hpp"
#include "io_types.hpp"
#include "system_time.hpp"

namespace el1::util::bits
{
	using namespace io::types;

	// returns a mask with all bits set to 0 except the bit specified by index
	template<typename T> constexpr T MakeBitMask(const unsigned idx);
	inline static constexpr u8_t  MakeBitMask8 (const unsigned idx) { return MakeBitMask<u8_t>(idx); }
	inline static constexpr u16_t MakeBitMask16(const unsigned idx) { return MakeBitMask<u16_t>(idx); }
	inline static constexpr u32_t MakeBitMask32(const unsigned idx) { return MakeBitMask<u32_t>(idx); }
	inline static constexpr u64_t MakeBitMask64(const unsigned idx) { return MakeBitMask<u64_t>(idx); }

	void SetBit(byte_t* const buffer, const usys_t idx, const bool value);
	bool GetBit(const byte_t* const buffer, const usys_t idx);

	// like memset() just with bits accuracy
	void FillBits(byte_t* const buffer, const usys_t n_bits, const byte_t pattern);

	// like a shift, but the bit that wa shifted out instead moves to the other side
	template<typename T> constexpr T RotateLeft (const T value, unsigned n);
	template<typename T> constexpr T RotateRight(const T value, unsigned n);
	inline static constexpr u8_t  RotateLeft8  (const u8_t  value, unsigned n) { return RotateLeft(value, n); }
	inline static constexpr u16_t RotateLeft16 (const u16_t value, unsigned n) { return RotateLeft(value, n); }
	inline static constexpr u32_t RotateLeft32 (const u32_t value, unsigned n) { return RotateLeft(value, n); }
	inline static constexpr u64_t RotateLeft64 (const u64_t value, unsigned n) { return RotateLeft(value, n); }
	inline static constexpr u8_t  RotateRight8 (const u8_t  value, unsigned n) { return RotateRight(value, n); }
	inline static constexpr u16_t RotateRight16(const u16_t value, unsigned n) { return RotateRight(value, n); }
	inline static constexpr u32_t RotateRight32(const u32_t value, unsigned n) { return RotateRight(value, n); }
	inline static constexpr u64_t RotateRight64(const u64_t value, unsigned n) { return RotateRight(value, n); }

	// calculates how many bits we need to bridge a specific amount time at a specific clock rate (rounded up)
	usys_t TimeToBits(const double hz_clock, const system::time::TTime time);

	// returns the amount of bytes required to store a given number of bits
	usys_t BitsToFullBytes(const usys_t n_bits);

	// Picks and packs bits based on a pattern from a input value.
	// If at any given positioon the pattern contains a 1 the matching bit in the value will be copied.
	// If at any given positioon the pattern contains a 0 the matching bit will be skipped and not copied to the result.
	// The skipped bits will not leave holes in the result, instead they are packed together.
	// This is like an AND operation, except that the bits that were maked out do not consume any space in the result.
	template<typename T> constexpr T CollateBits(T in, T pattern);
	inline static u8_t  constexpr CollateBits8 (u8_t  in, u8_t  pattern) { return CollateBits(in, pattern); }
	inline static u16_t constexpr CollateBits16(u16_t in, u16_t pattern) { return CollateBits(in, pattern); }
	inline static u32_t constexpr CollateBits32(u32_t in, u32_t pattern) { return CollateBits(in, pattern); }
	inline static u64_t constexpr CollateBits64(u64_t in, u64_t pattern) { return CollateBits(in, pattern); }

	unsigned CountOneBits(u8_t  in);
	unsigned CountOneBits(u16_t in);
	unsigned CountOneBits(u32_t in);
	unsigned CountOneBits(u64_t in);

	template<typename T, typename ... A>
	unsigned CountOneBits(T a0, A ... a)
	{
		return CountOneBits(a0) + CountOneBits(a...);
	}

	/*****************************************/

	template<typename T>
	constexpr T MakeBitMask(const unsigned idx)
	{
		EL_ERROR(idx >= sizeof(T) * 8, TIndexOutOfBoundsException, 0, sizeof(T) * 8 - 1, idx);
		return (idx == 0) ? (T)1 : ( ((T)1) << idx );
	}

	template<typename T>
	constexpr T RotateLeft(const T value, unsigned n)
	{
		n %= (sizeof(T) * 8);
		if(n == 0) return value;
		return (value << n) | (value >> (sizeof(T) * 8 - n));
	}

	template<typename T>
	constexpr T RotateRight(const T value, unsigned n)
	{
		n %= (sizeof(T) * 8);
		if(n == 0) return value;
		return (value >> n) | (value << (sizeof(T) * 8 - n));
	}

	template<typename T>
	constexpr T CollateBits(T in, T pattern)
	{
		T out = 0;
		unsigned n = 0;
		for(unsigned i = 0; i < sizeof(T) * 8; i++)
		{
			if(pattern & 1)
			{
				out |= ((in & 1) << n);
				n++;
			}
			pattern >>= 1;
			in >>= 1;
		}
		return out;
	}
}
