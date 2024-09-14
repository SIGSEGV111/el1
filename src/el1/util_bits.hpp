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
	constexpr usys_t BitsToFullBytes(const usys_t n_bits) { return (n_bits + 7) / 8; }

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

	/*****************************************/

	template<typename T>
	constexpr T FillBitMask(const unsigned n_bits)
	{
		return ((T)1 << n_bits) - (T)1;
	}

	template<unsigned n_bytes>
	static inline void FastCopyBytes(void* const _dst, const void* const _src);

	template<>
	inline void FastCopyBytes<1>(void* const dst, const void* const src)
	{
		*reinterpret_cast<u8_t*>(dst) = *reinterpret_cast<const u8_t*>(src);
	}

	template<>
	inline void FastCopyBytes<2>(void* const dst, const void* const src)
	{
		*reinterpret_cast<u16_t*>(dst) = *reinterpret_cast<const u16_t*>(src);
	}

	template<>
	inline void FastCopyBytes<3>(void* const dst, const void* const src)
	{
		*reinterpret_cast<u16_t*>(dst) = *reinterpret_cast<const u16_t*>(src);
		*(reinterpret_cast<u8_t*>(dst) + 2) = *(reinterpret_cast<const u8_t*>(src) + 2);
	}

	template<>
	inline void FastCopyBytes<4>(void* const dst, const void* const src)
	{
		*reinterpret_cast<u32_t*>(dst) = *reinterpret_cast<const u32_t*>(src);
	}

	/**
	* @brief Interprets a buffer as a packed array of integers of the specified bit size.
	*
	* @tparam bits_per_integer Number of bits used for each integer in the array.
	* @tparam T Type of the integers being extracted, defaults to u32_t.
	* @param array Pointer to the byte array interpreted as packed integers.
	* @param sz_array_bytes Size of the array in bytes.
	* @param idx Index of the integer to retrieve from the array.
	* @return The integer extracted from the specified index in the array.
	* @throws TIndexOutOfBoundsException If the calculated byte index exceeds the array size.
	*
	* @note This function assumes that the machine is little-endian.
	*/
	template<unsigned bits_per_integer, typename T = u32_t>
	T GetBitField(const byte_t* const array, const usys_t sz_array_bytes, const usys_t idx)
	{
		constexpr unsigned n_bytes = BitsToFullBytes(bits_per_integer);
		constexpr T mask = FillBitMask<T>(bits_per_integer);
		const usys_t idx_bit = idx * bits_per_integer;
		const usys_t idx_byte = idx_bit / 8;
		const unsigned n_shift = idx_bit - (idx_byte * 8);
		EL_ERROR(idx_byte + n_bytes + ((n_shift + bits_per_integer > 8) ? 1 : 0) > sz_array_bytes, TIndexOutOfBoundsException, 0, sz_array_bytes - 1, idx_byte + n_bytes);

		T integer = 0;
		if(n_shift + bits_per_integer > 8)
			FastCopyBytes<n_bytes + 1>(&integer, array + idx_byte);
		else
			FastCopyBytes<n_bytes>(&integer, array + idx_byte);
		integer >>= n_shift;
		integer &= mask;
		return integer;
	}
}
