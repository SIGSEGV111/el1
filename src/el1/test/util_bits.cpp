#include <gtest/gtest.h>
#include <string.h>
#include <el1/io_types.hpp>
#include <el1/util_bits.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::types;
	using namespace el1::util::bits;

	TEST(system_bits, MakeBitMask)
	{
		EXPECT_EQ(MakeBitMask8(0), 0b00000001U);
		EXPECT_EQ(MakeBitMask8(1), 0b00000010U);
		EXPECT_EQ(MakeBitMask8(3), 0b00001000U);
		EXPECT_EQ(MakeBitMask8(7), 0b10000000U);

		EXPECT_EQ(MakeBitMask16(0),  0x01U);
		EXPECT_EQ(MakeBitMask16(1),  0x02U);
		EXPECT_EQ(MakeBitMask16(3),  0x08U);
		EXPECT_EQ(MakeBitMask16(15), 0x8000U);

		EXPECT_EQ(MakeBitMask32(0),  0x01U);
		EXPECT_EQ(MakeBitMask32(1),  0x02U);
		EXPECT_EQ(MakeBitMask32(3),  0x08U);
		EXPECT_EQ(MakeBitMask32(31), 0x80000000U);

		EXPECT_EQ(MakeBitMask64(0),  0x01U);
		EXPECT_EQ(MakeBitMask64(1),  0x02U);
		EXPECT_EQ(MakeBitMask64(3),  0x08U);
		EXPECT_EQ(MakeBitMask64(63), 0x8000000000000000U);
	}

	TEST(system_bits, GetBit)
	{
		// 7, 8, 10, 20, 30, 31
		const byte_t arr[] = { 0b10000000, 0b00000101, 0b00010000, 0b11000000 };
		EXPECT_EQ(GetBit(arr,  0), false);
		EXPECT_EQ(GetBit(arr,  1), false);
		EXPECT_EQ(GetBit(arr,  7), true);
		EXPECT_EQ(GetBit(arr,  8), true);
		EXPECT_EQ(GetBit(arr,  9), false);
		EXPECT_EQ(GetBit(arr, 10), true);
		EXPECT_EQ(GetBit(arr, 19), false);
		EXPECT_EQ(GetBit(arr, 20), true);
		EXPECT_EQ(GetBit(arr, 21), false);
		EXPECT_EQ(GetBit(arr, 29), false);
		EXPECT_EQ(GetBit(arr, 30), true);
		EXPECT_EQ(GetBit(arr, 31), true);
	}

	TEST(system_bits, SetBit)
	{
		const byte_t cmp[] = { 0b10000000, 0b00000101, 0b00010000, 0b11000000 };
		byte_t arr[] = { 0x00, 0x00, 0x00, 0x00 };
		SetBit(arr,  0, false);
		SetBit(arr,  1, false);
		SetBit(arr,  7, true);
		SetBit(arr,  8, true);
		SetBit(arr,  9, false);
		SetBit(arr, 10, true);
		SetBit(arr, 19, false);
		SetBit(arr, 20, true);
		SetBit(arr, 21, false);
		SetBit(arr, 29, false);
		SetBit(arr, 30, true);
		SetBit(arr, 31, true);
		EXPECT_TRUE(memcmp(cmp, arr, 4) == 0);
	}

	TEST(system_bits, FillBits)
	{
		{
			const byte_t cmp[] = { 0xAA, 0xAA, 0xAA, 0x0A };
			byte_t arr[] = { 0x00, 0x00, 0x00, 0x00 };
			FillBits(arr, 28, 0xAA);
			EXPECT_TRUE(memcmp(cmp, arr, 4) == 0);
		}

		{
			const byte_t cmp[] = { 0x00, 0x00, 0x00, 0x00 };
			byte_t arr[] = { 0x00, 0x00, 0x00, 0x00 };
			FillBits(arr, 0, 0xFF);
			EXPECT_TRUE(memcmp(cmp, arr, 4) == 0);
		}

		{
			const byte_t cmp[] = { 0xFF, 0x0F, 0x00, 0x00 };
			byte_t arr[] = { 0x00, 0x00, 0x00, 0x00 };
			FillBits(arr, 12, 0xFF);
			EXPECT_TRUE(memcmp(cmp, arr, 4) == 0);
		}
	}

	TEST(system_bits, Rotate)
	{
		EXPECT_EQ(RotateLeft8(0b00000000, 0), 0b00000000U);
		EXPECT_EQ(RotateLeft8(0b00000000, 1), 0b00000000U);
		EXPECT_EQ(RotateLeft8(0b00000001, 0), 0b00000001U);
		EXPECT_EQ(RotateLeft8(0b00000001, 2), 0b00000100U);
		EXPECT_EQ(RotateLeft8(0b00000001, 7), 0b10000000U);
		EXPECT_EQ(RotateLeft8(0b00000001, 8), 0b00000001U);
		EXPECT_EQ(RotateLeft8(0b10000000, 1), 0b00000001U);
		EXPECT_EQ(RotateLeft8(0b10000000, 2), 0b00000010U);

		EXPECT_EQ(RotateLeft64(0b00000000, 0),  0b00000000U);
		EXPECT_EQ(RotateLeft64(0b00000000, 1),  0b00000000U);
		EXPECT_EQ(RotateLeft64(0b00000001, 0),  0b00000001U);
		EXPECT_EQ(RotateLeft64(0b00000001, 2),  0b00000100U);
		EXPECT_EQ(RotateLeft64(0b00000001, 7),  0b10000000U);
		EXPECT_EQ(RotateLeft64(0b00000001, 8), 0b100000000U);
		EXPECT_EQ(RotateLeft64(0b00000001, 64), 0b00000001U);
		EXPECT_EQ(RotateLeft64(0b00000001, 65), 0b00000010U);
	}

	TEST(system_bits, CollateBits)
	{
		const u16_t in = 0b1101010100011110;
		const u16_t pa = 0b1010101010101010;
		const u16_t re = 0b10000011;
		EXPECT_EQ(CollateBits16(in, pa), re);
	}

	TEST(system_bits, FastCopyBytes)
	{
		const u32_t src = 0x12345678U;
		{
			u32_t dst = 0;
			FastCopyBytes<1>(&dst, &src);
			EXPECT_EQ(dst, 0x78U);
		}

		{
			u32_t dst = 0;
			FastCopyBytes<2>(&dst, &src);
			EXPECT_EQ(dst, 0x5678U);
		}

		{
			u32_t dst = 0;
			FastCopyBytes<3>(&dst, &src);
			EXPECT_EQ(dst, 0x345678U);
		}

		{
			u32_t dst = 0;
			FastCopyBytes<4>(&dst, &src);
			EXPECT_EQ(dst, 0x12345678U);
		}
	}

	TEST(system_bits, BitsToFullBytes)
	{
		EXPECT_EQ(BitsToFullBytes(1), 1U);
		EXPECT_EQ(BitsToFullBytes(3), 1U);
		EXPECT_EQ(BitsToFullBytes(7), 1U);
		EXPECT_EQ(BitsToFullBytes(8), 1U);
		EXPECT_EQ(BitsToFullBytes(9), 2U);
		EXPECT_EQ(BitsToFullBytes(15), 2U);
		EXPECT_EQ(BitsToFullBytes(16), 2U);
		EXPECT_EQ(BitsToFullBytes(17), 3U);
	}

	TEST(system_bits, FillBitMask)
	{
		EXPECT_EQ(FillBitMask<u32_t>(1), 0b00000001U);
		EXPECT_EQ(FillBitMask<u32_t>(2), 0b00000011U);
		EXPECT_EQ(FillBitMask<u32_t>(3), 0b00000111U);
	}

	TEST(system_bits, GetBitField)
	{
		const byte_t array[] = { 0xab, 0xff, 0x12 };
		EXPECT_EQ(GetBitField<1>(array, sizeof(array), 0), 1U);
		EXPECT_EQ(GetBitField<1>(array, sizeof(array), 1), 1U);
		EXPECT_EQ(GetBitField<1>(array, sizeof(array), 2), 0U);
		EXPECT_EQ(GetBitField<1>(array, sizeof(array), 3), 1U);
		EXPECT_EQ(GetBitField<1>(array, sizeof(array), 4), 0U);

		EXPECT_EQ(GetBitField<3>(array, sizeof(array), 0), 0xabU & 7);
		EXPECT_EQ(GetBitField<3>(array, sizeof(array), 1), (0xabU >> 3) & 7);
		EXPECT_EQ(GetBitField<3>(array, sizeof(array), 2), (0x12ffabU >> 6) & 7);
		EXPECT_EQ(GetBitField<3>(array, sizeof(array), 3), (0x12ffabU >> 9) & 7);

		EXPECT_EQ(GetBitField<4>(array, sizeof(array), 0), 0x0bU);
		EXPECT_EQ(GetBitField<4>(array, sizeof(array), 1), 0x0aU);
		EXPECT_EQ(GetBitField<4>(array, sizeof(array), 2), 0x0fU);
		EXPECT_EQ(GetBitField<4>(array, sizeof(array), 3), 0x0fU);
		EXPECT_EQ(GetBitField<4>(array, sizeof(array), 4), 0x02U);
		EXPECT_EQ(GetBitField<4>(array, sizeof(array), 5), 0x01U);

		EXPECT_EQ(GetBitField<8>(array, sizeof(array), 0), 0xabU);
		EXPECT_EQ(GetBitField<8>(array, sizeof(array), 1), 0xffU);
		EXPECT_EQ(GetBitField<8>(array, sizeof(array), 2), 0x12U);

		EXPECT_EQ(GetBitField<16>(array, sizeof(array), 0), 0xffabU);
	}
}
