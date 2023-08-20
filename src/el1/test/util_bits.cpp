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
}
