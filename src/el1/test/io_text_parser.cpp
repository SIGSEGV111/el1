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

		TStringSource failed_input(U"abx");
		usys_t failed_pos = 0;
		auto failed = (U"a"_P + U"bc"_P).TryParse(failed_input, failed_pos);
		EXPECT_FALSE(failed);
		EXPECT_EQ(failed_pos, 0U);

		TStringSource offset_input(U"xxbar!");
		usys_t offset = 2;
		auto value = U"bar"_P.TryParse(offset_input, offset);
		ASSERT_TRUE(value);
		EXPECT_EQ(*value, U"bar");
		EXPECT_EQ(offset, 5U);
	}

	TEST(io_text_parser, IfTryTranslateLookAheadAndEnd)
	{
		auto even_digit = TryTranslate(
			[](const char32_t chr) -> std::optional<unsigned>
			{
				const unsigned value = (unsigned)(chr - U'0');
				return value % 2 == 0 ? std::optional<unsigned>(value) : std::nullopt;
			},
			CharRange(U'0', U'9')
		);
		EXPECT_EQ(even_digit.Parse(U"8"), 8U);
		EXPECT_THROW(even_digit.Parse(U"7"), TException);

		EXPECT_EQ(If(true, U"foo"_P).Parse(U"foo"), U"foo");
		EXPECT_THROW((void)If(false, U"foo"_P).Parse(U"foo"), TException);
		TStringSource conditional_input(U"foo");
		usys_t conditional_pos = 0;
		EXPECT_FALSE(If(false, U"foo"_P).TryParse(conditional_input, conditional_pos));
		EXPECT_EQ(conditional_pos, 0U);

		auto word = U"foo"_P + LookAhead(Discard(U','_P));
		TStringSource input(U"foo,bar");
		usys_t pos = 0;
		auto parsed = word.TryParse(input, pos);
		ASSERT_TRUE(parsed);
		EXPECT_EQ(*parsed, U"foo");
		EXPECT_EQ(pos, 3U);

		EXPECT_NO_THROW((void)(U"foo"_P + End()).Parse(U"foo"));
		EXPECT_THROW((void)(U"foo"_P + End()).Parse(U"foobar"), TException);
	}

	TEST(io_text_parser, CaptureViewsSourceBufferAndTryTranslateIsVariadic)
	{
		TStringSource input(TString(U"xx1234yy"));
		usys_t pos = 2;
		auto digits = Capture(Repeat(CharRange(U'0', U'9'), 1, 4));
		auto captured = digits.TryParse(input, pos);
		ASSERT_TRUE(captured);
		EXPECT_EQ(*captured, U"1234");
		EXPECT_EQ(captured->Data(), input.ItemPtr(2));
		EXPECT_EQ(pos, 6U);

		auto pair = TryTranslate(
			[](const char32_t a, const char32_t b) -> std::optional<unsigned>
			{
				return (unsigned)(a - U'0') * 10U + (unsigned)(b - U'0');
			},
			CharRange(U'0', U'9'), CharRange(U'0', U'9')
		);
		EXPECT_EQ(pair.Parse(U"42"), 42U);
	}

	TEST(io_text_parser, NegatedCharacterMatcher)
	{
		auto not_quote_or_backslash = ~CharList(U'"', U'\\');
		EXPECT_EQ(not_quote_or_backslash.Parse(U"x"), U'x');

		TStringSource quote_input(U"\"");
		usys_t quote_pos = 0;
		EXPECT_FALSE(not_quote_or_backslash.TryParse(quote_input, quote_pos));
		EXPECT_EQ(quote_pos, 0U);

		auto strict = ~(
			CharList(U'"', U'\\') ||
			If(true, CharRange((char32_t)0, (char32_t)0x1f))
		);
		auto tolerant = ~(
			CharList(U'"', U'\\') ||
			If(false, CharRange((char32_t)0, (char32_t)0x1f))
		);

		EXPECT_THROW((void)strict.Parse(U"\n"), TException);
		EXPECT_EQ(tolerant.Parse(U"\n"), U'\n');
		EXPECT_THROW((void)tolerant.Parse(U"\\"), TException);
	}

	TEST(io_text_parser, WhereValidateAndExpect)
	{
		auto even = Where(
			[](const char32_t chr) { return ((chr - U'0') % 2) == 0; },
			CharRange(U'0', U'9')
		);
		EXPECT_EQ(even.Parse(U"8"), U'8');
		TStringSource odd_input(U"7");
		usys_t odd_pos = 0;
		EXPECT_FALSE(even.TryParse(odd_input, odd_pos));
		EXPECT_EQ(odd_pos, 0U);

		auto validated = Validate(
			[](const char32_t chr) { return chr != U'7'; },
			CharRange(U'0', U'9')
		);
		EXPECT_EQ(validated.Parse(U"8"), U'8');
		TStringSource invalid_input(U"7");
		usys_t invalid_pos = 0;
		EXPECT_THROW((void)validated.TryParse(invalid_input, invalid_pos), TParseException);
		EXPECT_EQ(invalid_pos, 0U);

		auto committed = U'a'_P + Expect(U'b'_P);
		EXPECT_NO_THROW((void)committed.Parse(U"ab"));
		EXPECT_THROW((void)(committed || U"ac"_P).Parse(U"ac"), TParseException);
	}

	TEST(io_text_parser, Recursive)
	{
		auto nested = Recursive<TString>([](auto self)
		{
			return U"x"_P || Between(U'('_P, self, U')'_P);
		});

		EXPECT_EQ(nested.Parse(U"x"), U"x");
		EXPECT_EQ(nested.Parse(U"(x)"), U"x");
		EXPECT_EQ(nested.Parse(U"(((x)))"), U"x");

		auto left_recursive = Recursive<TString>([](auto self)
		{
			return self || U"x"_P;
		});
		EXPECT_THROW(left_recursive.Parse(U"x"), TLogicException);

		EXPECT_THROW(nested.Parse(U"(((x)))", TParseLimits{3}), TException);
	}

	TEST(io_text_parser, BetweenAndSeparatedBy)
	{
		auto integer = Translate([](TString value) { return (s32_t)value.ToInteger(); }, OneOrMore(CharRange(U'0', U'9')));
		auto list = Between(U'['_P, SeparatedBy(integer, U','_P), U']'_P);

		EXPECT_EQ(list.Parse(U"[]").Count(), 0U);
		auto values = list.Parse(U"[1,22,333]");
		ASSERT_EQ(values.Count(), 3U);
		EXPECT_EQ(values[0], 1);
		EXPECT_EQ(values[1], 22);
		EXPECT_EQ(values[2], 333);
		EXPECT_THROW(list.Parse(U"[1,]"), TException);

		auto discarded = Discard(U'('_P) + Discard(U')'_P);
		EXPECT_NO_THROW((void)discarded.Parse(U"()"));
	}

}
