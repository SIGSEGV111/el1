#include <gtest/gtest.h>
#include <el1/io_format_base64.hpp>
#include <el1/io_text_string.hpp>
#include <el1/io_text_encoding_utf8.hpp>
#include <el1/io_collection_list.hpp>
#include <el1/debug.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::format::base64;
	using namespace el1::io::collection::list;
	using namespace el1::io::text::string;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::debug;

	TEST(io_format_base64, TBase64Decoder)
	{
		const TString b64 = "UG9seWZvbiB6d2l0c2NoZXJuZCBhw59lbiBNw6R4Y2hlbnMgVsO2Z2VsIFLDvGJlbiwgSm9naHVydCB1bmQgUXVhcms=";
		const char* const expected = "Polyfon zwitschernd aßen Mäxchens Vögel Rüben, Joghurt und Quark";
		TList<byte_t> decoded = b64.chars.Pipe().Transform(TBase64Decoder()).Collect();
		EXPECT_TRUE(strncmp(expected, (char*)decoded.ItemPtr(0), decoded.Count()) == 0);
		if(strncmp(expected, (char*)decoded.ItemPtr(0), decoded.Count()) != 0)
			Hexdump(decoded.ItemPtr(0), decoded.Count());
	}

	TEST(io_format_base64, TBase64Encoder)
	{
		const TString str = "Polyfon zwitschernd aßen Mäxchens Vögel Rüben, Joghurt und Quark";
		const TString expected = "UG9seWZvbiB6d2l0c2NoZXJuZCBhw59lbiBNw6R4Y2hlbnMgVsO2Z2VsIFLDvGJlbiwgSm9naHVydCB1bmQgUXVhcms=";
		const TString encoded = str.chars.Pipe().Transform(TUTF8Encoder()).Transform(TBase64Encoder()).Collect();
		EXPECT_EQ(encoded, expected);
	}

	TEST(io_format_base64, TBase64Decoder_DecodeBlock)
	{
		{
			const byte_t arr_input[4] = { 65, 65, 65, 65 };
			byte_t arr_output[3] = { 0xff, 0xff, 0xff };
			EXPECT_EQ(TBase64Decoder::DecodeBlock(arr_input, arr_output), 0U);
		}

		{
			const byte_t arr_input[4] = { 12, 16, 64, 64 };
			byte_t arr_output[3] = { 0xff, 0xff, 0xff };
			EXPECT_EQ(TBase64Decoder::DecodeBlock(arr_input, arr_output), 1U);
			EXPECT_EQ(arr_output[0], '1');
		}

		{
			const byte_t arr_input[4] = { 12, 19, 8, 64 };
			byte_t arr_output[3] = { 0xff, 0xff, 0xff };
			EXPECT_EQ(TBase64Decoder::DecodeBlock(arr_input, arr_output), 2U);
			EXPECT_EQ(arr_output[0], '1');
			EXPECT_EQ(arr_output[1], '2');
		}

		{
			const byte_t arr_input[4] = { 12, 19, 8, 51 };
			byte_t arr_output[3] = { 0xff, 0xff, 0xff };
			EXPECT_EQ(TBase64Decoder::DecodeBlock(arr_input, arr_output), 3U);
			EXPECT_EQ(arr_output[0], '1');
			EXPECT_EQ(arr_output[1], '2');
			EXPECT_EQ(arr_output[2], '3');
		}

		{
			const byte_t arr_input[4] = { 20, 6, 61, 44 };
			byte_t arr_output[3] = { 0xff, 0xff, 0xff };
			EXPECT_EQ(TBase64Decoder::DecodeBlock(arr_input, arr_output), 3U);
			EXPECT_EQ(arr_output[0], 'P');
			EXPECT_EQ(arr_output[1], 'o');
			EXPECT_EQ(arr_output[2], 'l');
		}
	}

	TEST(io_format_base64, TBase64Decoder_EncodeBlock)
	{
		{
			byte_t buffer[4] = { '%', 0, 0, 0 };
			TBase64Encoder::EncodeBlock(buffer, 1);
			EXPECT_EQ(buffer[0],  9);
			EXPECT_EQ(buffer[1], 16);
			EXPECT_EQ(buffer[2], 64);
			EXPECT_EQ(buffer[3], 64);
		}

		{
			byte_t buffer[4] = { '%', 'm', 0, 0 };
			TBase64Encoder::EncodeBlock(buffer, 2);
			EXPECT_EQ(buffer[0],  9);
			EXPECT_EQ(buffer[1], 22);
			EXPECT_EQ(buffer[2], 52);
			EXPECT_EQ(buffer[3], 64);
		}

		{
			byte_t buffer[4] = { '%', 'm', 'z', 0 };
			TBase64Encoder::EncodeBlock(buffer, 3);
			EXPECT_EQ(buffer[0],  9);
			EXPECT_EQ(buffer[1], 22);
			EXPECT_EQ(buffer[2], 53);
			EXPECT_EQ(buffer[3], 58);
		}
	}
}
