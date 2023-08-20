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
		auto integer_str = (CharRange('1','9') + Optional(digits_str)) || '0'_P;
		auto sign_str = CharList('+','-');
		auto decimal_str = Optional(sign_str) + integer_str + Optional('.'_P + digits_str);
		auto to_double = Translate([](TString str){return str.ToDouble();}, decimal_str);
		auto double_foobar = to_double + Discard(" foobar!"_P);
		EXPECT_EQ(to_double.Parse("-17.54"), -17.54);
		EXPECT_THROW(to_double.Parse("-17.54 foobar!"), TException);
		EXPECT_EQ(double_foobar.Parse("-17.54 foobar!"), -17.54);
	}
}
