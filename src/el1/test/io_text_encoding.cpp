#include <gtest/gtest.h>
#include <el1/io_text.hpp>
#include <el1/io_text_encoding.hpp>
#include <el1/io_text_encoding_utf8.hpp>
#include <el1/io_collection_list.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::text;
	using namespace el1::io::text::encoding;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::io::collection::list;

	TEST(io_text_encoding_utf8, TUTF8Decoder)
	{
		// examples taken from wikipedia. Thank you!

		// single byte ASCII (y)
		{
			const TList< byte_t> utf8_data = { 0x79U };
			TList<TUTF32> utf32_data = utf8_data.Pipe().Transform(TUTF8Decoder()).Collect();
			EXPECT_EQ(utf32_data.Count(), 1U);
			EXPECT_EQ(utf32_data[0].code, 0x79U);
		}

		// two-byte sequence (ä)
		{
			const TList< byte_t> utf8_data = { 0xC3U, 0xA4U };
			TList<TUTF32> utf32_data = utf8_data.Pipe().Transform(TUTF8Decoder()).Collect();
			EXPECT_EQ(utf32_data.Count(), 1U);
			EXPECT_EQ(utf32_data[0].code, 0x00E4U);
		}

		// three-byte sequence (€)
		{
			const TList< byte_t> utf8_data = { 0xE2U, 0x82U, 0xACU };
			TList<TUTF32> utf32_data = utf8_data.Pipe().Transform(TUTF8Decoder()).Collect();
			EXPECT_EQ(utf32_data.Count(), 1U);
			EXPECT_EQ(utf32_data[0].code, 0x20ACU);
		}

		// four-byte sequence (𝄞)
		{
			const TList< byte_t> utf8_data = { 0xF0U, 0x9DU, 0x84U, 0x9EU };
			TList<TUTF32> utf32_data = utf8_data.Pipe().Transform(TUTF8Decoder()).Collect();
			EXPECT_EQ(utf32_data.Count(), 1U);
			EXPECT_EQ(utf32_data[0].code, 0x1D11EU);
		}

		// mixed sequence (yä€𝄞y)
		{
			const TList< byte_t> utf8_data = { 0x79U, 0xC3U, 0xA4U, 0xE2U, 0x82U, 0xACU, 0xF0U, 0x9DU, 0x84U, 0x9EU, 0x79U };
			TList<TUTF32> utf32_data = utf8_data.Pipe().Transform(TUTF8Decoder()).Collect();
			EXPECT_EQ(utf32_data.Count(), 5U);
			EXPECT_EQ(utf32_data[0].code, 0x79U);
			EXPECT_EQ(utf32_data[1].code, 0x00E4U);
			EXPECT_EQ(utf32_data[2].code, 0x20ACU);
			EXPECT_EQ(utf32_data[3].code, 0x1D11EU);
			EXPECT_EQ(utf32_data[4].code, 0x79U);
		}

		// invalid sequence (start with follow byte / missing start byte)
		{
			const TList< byte_t> utf8_data = { 0xA4U, 0x79U };
			EXPECT_THROW(utf8_data.Pipe().Transform(TUTF8Decoder()).Collect(), TInvalidUtf8SequenceException);
		}

		// invalid sequence (missing follow byte mid stream)
		{
			const TList< byte_t> utf8_data = { 0xF0U, 0x9DU, 0x84U, 0x79U };
			EXPECT_THROW(utf8_data.Pipe().Transform(TUTF8Decoder()).Collect(), TInvalidUtf8SequenceException);
		}

		// incomplete sequence (EOF / missing follow byte)
		{
			const TList< byte_t> utf8_data = { 0xF0U, 0x9DU, 0x84U };
			EXPECT_THROW(utf8_data.Pipe().Transform(TUTF8Decoder()).Collect(), el1::io::stream::TStreamDryException);
		}
	}

	TEST(io_text_encoding_utf8, TUTF8Encoder)
	{
		// single byte ASCII (y)
		{
			const TList< TUTF32> utf32_data = { 0x79U };
			TList<byte_t> utf8_data = utf32_data.Pipe().Transform(TUTF8Encoder()).Collect();
			EXPECT_EQ(utf8_data.Count(), 1U);
			EXPECT_EQ(utf8_data[0], 0x79U);
		}

		// two-byte sequence (ä)
		{
			const TList< TUTF32> utf32_data = { 0x00E4U };
			TList<byte_t> utf8_data = utf32_data.Pipe().Transform(TUTF8Encoder()).Collect();
			EXPECT_EQ(utf8_data.Count(), 2U);
			EXPECT_EQ(utf8_data[0], 0xC3U);
			EXPECT_EQ(utf8_data[1], 0xA4U);
		}

		// three-byte sequence (€)
		{
			const TList< TUTF32> utf32_data = { 0x20ACU };
			TList<byte_t> utf8_data = utf32_data.Pipe().Transform(TUTF8Encoder()).Collect();
			EXPECT_EQ(utf8_data.Count(), 3U);
			EXPECT_EQ(utf8_data[0], 0xE2U);
			EXPECT_EQ(utf8_data[1], 0x82U);
			EXPECT_EQ(utf8_data[2], 0xACU);
		}

		// four-byte sequence (𝄞)
		{
			const TList< TUTF32> utf32_data = { 0x1D11EU };
			TList<byte_t> utf8_data = utf32_data.Pipe().Transform(TUTF8Encoder()).Collect();
			EXPECT_EQ(utf8_data.Count(), 4U);
			EXPECT_EQ(utf8_data[0], 0xF0U);
			EXPECT_EQ(utf8_data[1], 0x9DU);
			EXPECT_EQ(utf8_data[2], 0x84U);
			EXPECT_EQ(utf8_data[3], 0x9EU);
		}

		// mixed sequence (yä€𝄞y)
		{
			const TList< TUTF32> utf32_data = { 0x79U, 0x00E4U, 0x20ACU, 0x1D11EU, 0x79U };
			TList<byte_t> utf8_data = utf32_data.Pipe().Transform(TUTF8Encoder()).Collect();
			EXPECT_EQ(utf8_data.Count(), 11U);
			EXPECT_EQ(utf8_data[0], 0x79U);

			EXPECT_EQ(utf8_data[1], 0xC3U);
			EXPECT_EQ(utf8_data[2], 0xA4U);

			EXPECT_EQ(utf8_data[3], 0xE2U);
			EXPECT_EQ(utf8_data[4], 0x82U);
			EXPECT_EQ(utf8_data[5], 0xACU);

			EXPECT_EQ(utf8_data[6], 0xF0U);
			EXPECT_EQ(utf8_data[7], 0x9DU);
			EXPECT_EQ(utf8_data[8], 0x84U);
			EXPECT_EQ(utf8_data[9], 0x9EU);

			EXPECT_EQ(utf8_data[10], 0x79U);
		}

		// lowest encodeable character code
		{
			const TList< TUTF32> utf32_data = { 0U };
			TList<byte_t> utf8_data = utf32_data.Pipe().Transform(TUTF8Encoder()).Collect();
			EXPECT_EQ(utf8_data.Count(), 1U);
			EXPECT_EQ(utf8_data[0], 0U);
		}

		// highest encodeable character code
		{
			const TList< TUTF32> utf32_data = { 0x3FFFFFU };
			TList<byte_t> utf8_data = utf32_data.Pipe().Transform(TUTF8Encoder()).Collect();
			EXPECT_EQ(utf8_data.Count(), 4U);
			EXPECT_EQ(utf8_data[0], 0b11111111);
			EXPECT_EQ(utf8_data[1], 0b10111111);
			EXPECT_EQ(utf8_data[2], 0b10111111);
			EXPECT_EQ(utf8_data[3], 0b10111111);
		}

		// invalid input (unicode out encodeable of range)
		{
			const TList< TUTF32> utf32_data = { 0x400000U };
			EXPECT_THROW(utf32_data.Pipe().Transform(TUTF8Encoder()).Collect(), TInvalidUtf8SequenceException);
		}
		{
			const TList< TUTF32> utf32_data = { 0x500000U };
			EXPECT_THROW(utf32_data.Pipe().Transform(TUTF8Encoder()).Collect(), TInvalidUtf8SequenceException);
		}
		{
			const TList< TUTF32> utf32_data = { 0xFFFFFFFFU };
			EXPECT_THROW(utf32_data.Pipe().Transform(TUTF8Encoder()).Collect(), TInvalidUtf8SequenceException);
		}
	}
}
