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

	TEST(io_text_parser, DispatchSelectsOneCharacterBranch)
	{
		auto parser = Dispatch(
			Case(U'a'_P, Translate([](TString) { return 1; }, U"alpha"_P)),
			Case(CharList(U'b', U'c'), Translate([](TString) { return 2; }, U"beta"_P))
		);

		EXPECT_EQ(parser.Parse(U"alpha"), 1);
		EXPECT_EQ(parser.Parse(U"beta"), 2);
		TStringSource input(U"delta");
		usys_t pos = 0;
		EXPECT_FALSE(parser.TryParse(input, pos));
		EXPECT_EQ(pos, 0U);
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

	TEST(io_text_parser, StaticCompletionFollowsGrammar)
	{
		auto command = U"git "_P + (U"status"_P || U"stash"_P || U"show"_P);

		auto sta = command.Complete(U"git sta");
		ASSERT_EQ(sta.Count(), 2U);
		EXPECT_EQ(sta[0].replace_begin, 4U);
		EXPECT_EQ(sta[0].replace_end, 7U);
		EXPECT_EQ(sta[0].replacement, U"status");
		EXPECT_EQ(sta[1].replace_begin, 4U);
		EXPECT_EQ(sta[1].replace_end, 7U);
		EXPECT_EQ(sta[1].replacement, U"stash");

		auto sho = command.Complete(U"git sho");
		ASSERT_EQ(sho.Count(), 1U);
		EXPECT_EQ(sho[0].replacement, U"show");

		auto repetition = Repeat(U'a'_P, 0, NEG1) + U'b'_P;
		auto repeated = repetition.Complete(U"aaa");
		ASSERT_EQ(repeated.Count(), 2U);
		EXPECT_EQ(repeated[0].replacement, U"a");
		EXPECT_EQ(repeated[1].replacement, U"b");
	}

	TEST(io_text_parser, CompletionRespectsExpectSeparatedByAndRecursion)
	{
		auto expected = U"foo"_P + Expect(U"bar"_P);
		auto partial = expected.Complete(U"foob");
		ASSERT_EQ(partial.Count(), 1U);
		EXPECT_EQ(partial[0].replace_begin, 3U);
		EXPECT_EQ(partial[0].replace_end, 4U);
		EXPECT_EQ(partial[0].replacement, U"bar");
		EXPECT_THROW((void)expected.Complete(U"foox"), TParseException);

		auto separated = SeparatedBy(U"foo"_P, U','_P, 1);
		auto after_value = separated.Complete(U"foo");
		ASSERT_EQ(after_value.Count(), 1U);
		EXPECT_EQ(after_value[0].replacement, U",");
		auto after_separator = separated.Complete(U"foo,");
		ASSERT_EQ(after_separator.Count(), 1U);
		EXPECT_EQ(after_separator[0].replace_begin, 4U);
		EXPECT_EQ(after_separator[0].replacement, U"foo");

		auto nested = Recursive<TString>([](auto self)
		{
			return U"x"_P || Between(U'('_P, self, U')'_P);
		});
		auto recursive = nested.Complete(U"((x");
		ASSERT_EQ(recursive.Count(), 1U);
		EXPECT_EQ(recursive[0].replacement, U")");

		auto minimum_two = Validate(
			[](const TStringView value) { return value.Length() >= 2; },
			Capture(Repeat(U'a'_P, 1, 2))
		);
		auto extend_invalid_but_incomplete = minimum_two.Complete(U"a");
		ASSERT_EQ(extend_invalid_but_incomplete.Count(), 1U);
		EXPECT_EQ(extend_invalid_but_incomplete[0].replacement, U"a");
		EXPECT_THROW((void)Validate([](const TString&) { return false; }, U"a"_P).Complete(U"a"), TParseException);
	}

	TEST(io_text_parser, DynamicCompletionUsesSourcePrefixAndCursor)
	{
		TString seen_prefix;
		auto token = WithCompletion(
			Capture(OneOrMore(~CharList(U' ', U'\t'))),
			[&](const TStringView prefix, TCompletionSink& sink)
			{
				seen_prefix = TString(prefix);
				if(prefix == U"sta")
				{
					sink.Add(U"status");
					sink.Add(U"stash");
					sink.Add(U"status"); // duplicate candidates are suppressed
				}
			}
		);

		const TString line(U"sta --ignored-suffix");
		auto completions = token.Complete(line, 3);
		EXPECT_EQ(seen_prefix, U"sta");
		ASSERT_EQ(completions.Count(), 2U);
		EXPECT_EQ(completions[0].replace_begin, 0U);
		EXPECT_EQ(completions[0].replace_end, 3U);
		EXPECT_EQ(completions[0].replacement, U"status");
		EXPECT_EQ(completions[1].replacement, U"stash");
	}

	TEST(io_text_parser, CompletionTraversesSemanticAndDispatchNodes)
	{
		auto translated = Translate([](TString value) { return value.Length(); }, U"alpha"_P);
		auto translated_completion = translated.Complete(U"al");
		ASSERT_EQ(translated_completion.Count(), 1U);
		EXPECT_EQ(translated_completion[0].replacement, U"alpha");

		auto checked = TryTranslate(
			[](TString value) -> std::optional<TString>
			{
				return value == U"yes" ? std::optional<TString>(std::move(value)) : std::nullopt;
			},
			U"yes"_P || U"no"_P
		);
		auto checked_completion = checked.Complete(U"y");
		ASSERT_EQ(checked_completion.Count(), 1U);
		EXPECT_EQ(checked_completion[0].replacement, U"yes");
		EXPECT_EQ(checked.Complete(U"no").Count(), 0U);

		auto dispatched = Dispatch(
			Case(U'a'_P, Translate([](TString value) { return value.Length(); }, U"alpha"_P)),
			Case(U'b'_P, Translate([](TString value) { return value.Length(); }, U"beta"_P))
		);
		auto at_start = dispatched.Complete(U"");
		ASSERT_EQ(at_start.Count(), 2U);
		EXPECT_EQ(at_start[0].replacement, U"alpha");
		EXPECT_EQ(at_start[1].replacement, U"beta");
		auto in_branch = dispatched.Complete(U"be");
		ASSERT_EQ(in_branch.Count(), 1U);
		EXPECT_EQ(in_branch[0].replacement, U"beta");

		auto looked_ahead = LookAhead(U"foo"_P) + U"foo"_P;
		auto lookahead_completion = looked_ahead.Complete(U"fo");
		ASSERT_EQ(lookahead_completion.Count(), 1U);
		EXPECT_EQ(lookahead_completion[0].replacement, U"foo");

		EXPECT_EQ(If(false, U"hidden"_P).Complete(U"").Count(), 0U);
	}

}
