#include <gtest/gtest.h>
#include <el1/io_text.hpp>
#include <el1/io_text_parser.hpp>
#include <el1/io_text_terminal.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::error;
	using namespace el1::io::text::parser;
	using namespace el1::io::text::string;
	using namespace el1::io::text::terminal;
	using namespace el1::io::collection::list;

	TEST(io_text_parser, Basics)
	{
		auto digits_str = Repeat(CharRange('0','9'), 1, 16);
		auto integer_str = (CharRange('1','9') + Optional(digits_str)) || U'0'_P;
		auto sign_str = CharList('+','-');
		auto decimal_str = Optional(sign_str) + integer_str + Optional(U'.'_P + digits_str);
		auto to_double = Translate([](TString str){return str.ToDouble();}, decimal_str);
		auto double_foobar = to_double + Discard(U" foobar!"_P);
		EXPECT_EQ(to_double.Parse("-17.54"), -17.54);
		EXPECT_THROW(to_double.Parse("-17.54 foobar!"), TException);
		EXPECT_EQ(double_foobar.Parse("-17.54 foobar!"), -17.54);
	}

	TEST(io_text_parser, AlternativesBacktrackAndOffsetParsing)
	{
		EXPECT_EQ((U"foo"_P || U"bar"_P).Parse(U"bar"), U"bar");

		TStringInput failed_input(U"abx");
		usys_t failed_pos = 0;
		auto failed = (U"a"_P + U"bc"_P).TryParse(failed_input, failed_pos);
		EXPECT_FALSE(failed);
		EXPECT_EQ(failed_pos, 0U);

		TStringInput offset_input(U"xxbar!");
		usys_t offset = 2;
		auto value = U"bar"_P.TryParse(offset_input, offset);
		ASSERT_TRUE(value);
		EXPECT_EQ(*value, U"bar");
		EXPECT_EQ(offset, 5U);
	}

}
