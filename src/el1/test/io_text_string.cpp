#include <gtest/gtest.h>
#include <el1/io_text.hpp>
#include <el1/io_text_string.hpp>
#include "util.hpp"
#include <limits>

using namespace ::testing;

struct TCustomFormatValue
{
	el1::io::types::s64_t value;
};

struct TDefaultSpecFormatValue
{
	el1::io::types::s64_t value;
};

namespace el1::io::text::format
{
	struct TCustomValueSpec
	{
		bool hexadecimal = false;
		bool unicode = false;
	};

	template<>
	struct TFormatter<::TCustomFormatValue>
	{
		using TSpec = TCustomValueSpec;

		static constexpr bool Supports(const char32_t code) noexcept
		{
			return code == 'h' || code == 'm';
		}

		static constexpr bool ParseSpec(TSpec& out, const char32_t, const TFormatSpecView& text) noexcept
		{
			if(text.Length() == 0)
				return true;
			if(text.IsBraced() && text.Length() == 1 && text[0] == 'h')
			{
				out.hexadecimal = true;
				return true;
			}
			if(text.IsBraced() && text.Length() == 2 && text[0] == 0x00e4U && text[1] == 0x1f600U)
			{
				out.unicode = true;
				return true;
			}
			return false;
		}

		static void Format(IFormatSink& out, const ::TCustomFormatValue& value, const char32_t, const TSpec& spec)
		{
			const string::TString text = spec.unicode ? string::TString::Format(U"<U:%d>", value.value) :
				(spec.hexadecimal ? string::TString::Format(U"<%x>", value.value) : string::TString::Format(U"<%d>", value.value));
			out.Append(text.View());
		}
	};

	template<>
	struct TFormatter<::TDefaultSpecFormatValue>
	{
		static constexpr bool Supports(const char32_t code) noexcept
		{
			return code == 'm';
		}

		static void Format(IFormatSink& out, const ::TDefaultSpecFormatValue& value, const char32_t, const TDefaultFormatSpec& spec)
		{
			const string::TString text = string::TString::Format(U"<%d:%d:%d>", spec.has_width ? spec.width : 0, spec.has_precision ? spec.precision : 0, value.value);
			out.Append(text.View());
		}
	};
}

template<typename TFormat, typename... TArgs>
concept CCanFormat = requires(const TFormat& format, const TArgs&... args)
{
	el1::io::text::string::TString::Format(format, args...);
};

template<typename T>
concept CCanUseRuntimeFormatString = requires(T value)
{
	el1::io::text::string::TString::Format(value, 1);
};

namespace
{
	using namespace el1::io::text;
	using namespace el1::io::text::string;
	using namespace el1::error;


	TEST(io_text_string, TStringView_UnicodeLiteral)
	{
		static_assert(!std::is_constructible_v<TStringView, const char32_t*, usys_t>);
		static_assert(std::is_constructible_v<TStringView, const TString&>);
		constexpr TStringView ascii = U"hello";
		static_assert(ascii.Length() == 5);
		static constexpr char32_t raw_chars[] = U"unsafe";
		constexpr TStringView raw_view = TStringView::FromUnsafePointer(raw_chars + 1, 3);
		static_assert(raw_view.Length() == 3);
		EXPECT_EQ(raw_view[0], U'n');

		constexpr TStringView unicode = U"Hällö 😀";
		static_assert(unicode.Length() == 7);

		EXPECT_EQ(unicode[0], static_cast<u32_t>('H'));
		EXPECT_EQ(unicode[1], 0x00e4U);
		EXPECT_EQ(unicode[4], 0x00f6U);
		EXPECT_EQ(unicode[-1], 0x1f600U);
		EXPECT_EQ(unicode.Data()[unicode.Length()], 0U);


		const TString owned = unicode;
		EXPECT_EQ(owned, unicode);
		EXPECT_TRUE(owned.BeginsWith(TStringView(U"Häll")));
		EXPECT_TRUE(owned.EndsWith(TStringView(U"ö 😀")));
		EXPECT_TRUE(owned.Contains(TStringView(U"llö")));
		EXPECT_EQ(owned.Find(TStringView(U"😀")), 6U);

		const TStringView slice = unicode.SliceBE(1, 5);
		EXPECT_EQ(slice, U"ällö");

		EXPECT_EQ(TStringView(U"-123").ToInteger(), -123);
		EXPECT_DOUBLE_EQ(TStringView(U"12.5").ToDouble(), 12.5);
		EXPECT_EQ(TString::Format(U"%s/%s", TStringView(U"ä"), TStringView(U"😀")), TStringView(U"ä/😀"));
	}

	TEST(io_text_string, StaticSymbolViews)
	{
		static_assert(OCTAL_SYMBOLS.Length() == 8);
		static_assert(DECIMAL_SYMBOLS.Length() == 10);
		static_assert(HEXADECIMAL_SYMBOLS_UC.Length() == 16);
		static_assert(HEXADECIMAL_SYMBOLS_LC.Length() == 16);
		static_assert(BINARY_SYMBOLS.Length() == 2);
		static_assert(ASCII_QUOTE_SYMBOLS.Length() == 2);
		static_assert(CONTROL_CHARS.Length() == 32);
		static_assert(WHITESPACE_CHARS.Length() == 25);

		EXPECT_EQ(CONTROL_CHARS[0], U'\0');
		EXPECT_EQ(CONTROL_CHARS[31], (char32_t)0x1f);
		EXPECT_EQ(WHITESPACE_CHARS[0], U'\t');
		EXPECT_EQ(WHITESPACE_CHARS[-1], (char32_t)0x3000);
		EXPECT_EQ(OCTAL_SYMBOLS, TStringView(U"01234567"));
		EXPECT_EQ(HEXADECIMAL_SYMBOLS_LC, TStringView(U"0123456789abcdef"));
	}

	TEST(io_text_string, TString_Construct)
	{
		{
			TString s;
			EXPECT_EQ(s.Length(), 0U);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Length(), 11U);
			EXPECT_EQ(s[4], U'o');
		}

		{
			TString s = U"hello world äöü";
			EXPECT_EQ(s.Length(), 15U);
			EXPECT_EQ(s[4], U'o');
		}

		{
			const char32_t arr[] = { 'h', 'e', 'l', 'l', 'o', U'\0', 'w', 'o', 'r', 'l', 'd', U'\0' };
			TString s(arr);
			EXPECT_EQ(s.Length(), 5U);
			EXPECT_EQ(s[4], U'o');
		}

		{
			const char32_t arr[] = { 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', U'\0' };
			TString s(arr, 6);
			EXPECT_EQ(s.Length(), 6U);
			EXPECT_EQ(s[5], U' ');
		}
	}

	TEST(io_text_string, TString_Compare)
	{
		const TString s1 = U"hello world";
		const TString s2 = U"foobar";
		const TString s3 = U"hello world ";
		const TString s4 = U"ihello world";
		const TString s5 = U"hello world";

		EXPECT_EQ(s1, s5);
		EXPECT_NE(s1, TString(U"hello worl_"));
		EXPECT_NE(s1, TString(U"foobar"));
		EXPECT_EQ(s2, TString(U"foobar"));

		EXPECT_GT(s3, s1);
		EXPECT_LT(s1, s3);
		EXPECT_LE(s1, s5);
		EXPECT_GE(s1, s5);
		EXPECT_GE(s3, s1);
		EXPECT_LE(s1, s3);
		EXPECT_NE(s1, s3);
		EXPECT_FALSE(s3 >= s4);
		EXPECT_FALSE(s3 >  s4);
		EXPECT_FALSE(s4 <= s3);
		EXPECT_FALSE(s4 <  s3);
		EXPECT_FALSE(s1 >= s3);
		EXPECT_FALSE(s3 <= s1);
		EXPECT_FALSE(s1 >  s3);
		EXPECT_FALSE(s3 <  s1);

		EXPECT_GT(s4, s3);
		EXPECT_LT(s3, s4);
		EXPECT_GE(s4, s3);
		EXPECT_LE(s3, s4);
		EXPECT_NE(s3, s4);

		EXPECT_TRUE(s1.BeginsWith(TStringView(U"hello")));
		EXPECT_FALSE(s1.BeginsWith(TStringView(U"hello!")));

		EXPECT_TRUE(s1.EndsWith(TStringView(U"world")));
		EXPECT_FALSE(s1.EndsWith(TStringView(U"world!")));
		EXPECT_FALSE(s1.EndsWith(TStringView(U"_world")));
	}

	TEST(io_text_string, TString_Concat)
	{
		TString a = U"hello";
		TString b = U"world";
		TString r = a + TStringView(U" ") + b + ' ' + '1';
		EXPECT_EQ(r, TString(U"hello world 1"));
	}

	TEST(io_text_string, TString_MakeCStr)
	{
		const char* ref ="hello world";
		const TString s1 = ref;
		EXPECT_TRUE(strcmp(ref, s1.MakeCStr().get()) == 0);
	}

	TEST(io_text_string, TString_Trim)
	{
		{
			TString s = U" \thello world \t ";
			s.Trim();
			EXPECT_EQ(s, U"hello world");
		}

		{
			TString s = U"hello world \t ";
			s.Trim();
			EXPECT_EQ(s, U"hello world");
		}

		{
			TString s = U" hello world";
			s.Trim();
			EXPECT_EQ(s, U"hello world");
		}

		{
			TString s = U" \thello world \t ";
			s.Trim(true, false);
			EXPECT_EQ(s, U"hello world \t ");
		}

		{
			TString s = U" \thello world \t ";
			s.Trim(false, true);
			EXPECT_EQ(s, U" \thello world");
		}

		{
			TString s = U"hello world";
			s.Trim();
			EXPECT_EQ(s, U"hello world");
		}

		{
			TString s = U"h ello worl\td";
			s.Trim();
			EXPECT_EQ(s, U"h ello worl\td");
		}

		{
			TString s = U" \t ";
			s.Trim();
			EXPECT_EQ(s, U"");
		}
	}

	TEST(io_text_string, TString_ReplaceAt)
	{
		{
			TString s = U"hello world";
			s.ReplaceAt(0, 5, TStringView(U"foobar"));
			EXPECT_EQ(s, U"foobar world");
		}

		{
			TString s = U"hello world";
			s.ReplaceAt(6, 5, TStringView(U"foobar"));
			EXPECT_EQ(s, U"hello foobar");
		}

		{
			TString s = U"hello world";
			s.ReplaceAt(5, 1, TStringView(U" brave new "));
			EXPECT_EQ(s, U"hello brave new world");
		}

		{
			TString s = U"hello world";
			s.ReplaceAt(6, 0, TStringView(U"brave new "));
			EXPECT_EQ(s, U"hello brave new world");
		}

		{
			TString s = U"hello world";
			s.ReplaceAt(0, 11, TStringView(U""));
			EXPECT_EQ(s, U"");
		}

		{
			TString s = U"hello world";
			s.ReplaceAt(1, 9, TStringView(U""));
			EXPECT_EQ(s, U"hd");
		}

		{
			TString s = U"hello world";
			s.ReplaceAt(0, 5, TStringView(U"test1"));
			EXPECT_EQ(s, U"test1 world");
		}

		{
			TString s = U"hello world";
			s.ReplaceAt(-5, 5, TStringView(U"test1"));
			EXPECT_EQ(s, U"hello test1");
		}

		{
			TString s = U"hello world";
			s.ReplaceAt(11, 0, TStringView(U" test1"));
			EXPECT_EQ(s, U"hello world test1");
		}

		{
			TString s = U"hello world";
			EXPECT_THROW(s.ReplaceAt(12, 0, TStringView(U" test1")), el1::error::TIndexOutOfBoundsException);
		}
	}

	TEST(io_text_string, TString_Replace)
	{
		{
			TString s = U"hello world";
			s.Replace(TStringView(U"hello"), TStringView(U"foobar"));
			EXPECT_EQ(s, U"foobar world");
		}

		{
			TString s = U"hello world";
			s.Replace(TStringView(U"l"), TStringView(U"_"), 0, false, 2);
			EXPECT_EQ(s, U"he__o world");
// 			printf("s = '%s'\n", s.MakeCStr());
		}

		{
			TString s = U"hello world";
			s.Replace(TStringView(U"l"), TStringView(U"_"), -1, true, 2);
			EXPECT_EQ(s, U"hel_o wor_d");
		}

		{
			TString s = U"";
			s.Replace(TStringView(U"a"), TStringView(U"b"));
			EXPECT_EQ(s, U"");
		}

		{
			TString s = U"test";
			s.Replace(TStringView(U"test"), TStringView(U"foobar"));
			EXPECT_EQ(s, U"foobar");
		}

		{
			TString s = U"test";
			s.Replace(TStringView(U"test"), TStringView(U""));
			EXPECT_EQ(s, U"");
		}

		{
			TString s = U"test";
			s.Replace(TStringView(U"t"), TStringView(U"ttt"));
			EXPECT_EQ(s, U"tttesttt");
		}

		{
			TString s = U"test";
			s.Replace(TStringView(U"t"), TStringView(U"t"));
			EXPECT_EQ(s, U"test");
		}

		{
			TString s = U"test";
			EXPECT_THROW(s.Replace(TStringView(U""), TStringView(U"test")), TInvalidArgumentException);
		}
	}

	TEST(io_text_string, TString_Find)
	{
		{
			TString s = U"hello world";
			EXPECT_THROW(s.Find(TStringView(U"")), TInvalidArgumentException);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(TStringView(U"world")), 6U);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(TStringView(U"test")), -1UL);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(TStringView(U" ")), 5UL);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(s), 0UL);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(s, -1, true), 0UL);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(s, -2, true), -1UL);
		}

		{
			TString s = U"hello world";
			EXPECT_THROW(s.Find(s, s.Length(), true), el1::error::TIndexOutOfBoundsException);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(s, 1, false), -1UL);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(TStringView(U"hello worl_")), -1UL);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(TStringView(U"o"), 6), 7UL);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(TStringView(U"o"), 7), 7UL);
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.Find(TStringView(U"w"), 7), -1UL);
		}
	}

	TEST(io_text_string, TString_Pad)
	{
		{
			TString s = U"hello world";
			s.Pad('_', 13, EPlacement::END);
			EXPECT_EQ(s, U"hello world__");
		}

		{
			TString s = U"hello world";
			s.Pad('_', 11, EPlacement::END);
			EXPECT_EQ(s, U"hello world");
		}

		{
			TString s = U"hello world";
			s.Pad('_', 13, EPlacement::START);
			EXPECT_EQ(s, U"__hello world");
		}

		{
			TString s = U"hello world";
			s.Pad('_', 13, EPlacement::MID);
			EXPECT_EQ(s, U"hello__ world");
		}
	}

	TEST(io_text_string, TString_Reverse)
	{
		{
			TString s = U"hello world";
			s.Reverse();
			EXPECT_EQ(s, U"dlrow olleh");
		}
	}

	TEST(io_text_string, TString_Format)
	{
		EXPECT_EQ(TString::Format(U"hello %s", 'a'), U"hello a");
		EXPECT_EQ(TString::Format(U"hello %s", L'a'), U"hello a");
		EXPECT_EQ(TString::Format(U"hello %s", U'a'), U"hello a");
		EXPECT_EQ(TString::Format(U"hello %s", "foobar"), U"hello foobar");
		EXPECT_EQ(TString::Format(U"hello %s", ""), U"hello ");
		EXPECT_EQ(TString::Format(U"test %d", 17), U"test 17");
		EXPECT_EQ(TString::Format(U"test %d %d", 17572, 13), U"test 17572 13");
		EXPECT_EQ(TString::Format(U"test %d", -17), U"test -17");
		EXPECT_EQ(TString::Format(U"hello %s%d", "foobar", 3), U"hello foobar3");
		EXPECT_EQ(TString::Format(U"progress = %d%%", 17), U"progress = 17%");
		EXPECT_EQ(TString::Format(U"path = %q", "/opt/el1/src"), U"path = '/opt/el1/src'");
		EXPECT_EQ(TString::Format(U"text = %q", U"'some quoted text'"), U"text = \"'some quoted text'\"");
		EXPECT_EQ(TString::Format(U"text = %q", U"'some quoted text'"), U"text = \"'some quoted text'\"");
		static_assert(!format::IsValidFormat<int>(U"wrong %g"));
		static_assert(!format::IsValidFormat<int>(U"wrong %%"));
		static_assert(!format::IsValidFormat<int>(U"wrong %s"));
		static_assert(!format::IsValidFormat<int, int>(U"only %d"));
		static_assert(!format::IsValidFormat<int>(U"too many %d %d"));

		EXPECT_EQ(TString::Format(U"test %d", 0), U"test 0");

		EXPECT_EQ(TString::Format(U"test %d", (s8_t)17), U"test 17");
		EXPECT_EQ(TString::Format(U"test %d", (u8_t)17), U"test 17");
		EXPECT_EQ(TString::Format(U"test %d", (s16_t)17), U"test 17");
		EXPECT_EQ(TString::Format(U"test %d", (u16_t)17), U"test 17");
		EXPECT_EQ(TString::Format(U"test %d", (s32_t)17), U"test 17");
		EXPECT_EQ(TString::Format(U"test %d", (u32_t)17), U"test 17");
		EXPECT_EQ(TString::Format(U"test %d", (s64_t)17), U"test 17");
		EXPECT_EQ(TString::Format(U"test %d", (u64_t)17), U"test 17");

		EXPECT_EQ(TString::Format(U"test %.3d", 17.5725), U"test 17.573");
		EXPECT_EQ(TString::Format(U"test %.3d",  17.9995), U"test 18.000");
		EXPECT_EQ(TString::Format(U"test %.3d", -17.9995), U"test -18.000");
		EXPECT_EQ(TString::Format(U"test %.3d", -17.9994), U"test -17.999");

		EXPECT_EQ(TString::Format(U"test %02x", 10), U"test 0a");
		EXPECT_EQ(TString::Format(U"test %_2.7x", 10), U"test _a.0000000");
		EXPECT_EQ(TString::Format(U"test %x", 17), U"test 11");
		EXPECT_EQ(TString::Format(U"test %x", 167), U"test a7");
		EXPECT_EQ(TString::Format(U"test %x", -167), U"test -a7");
		EXPECT_EQ(TString::Format(U"test %x", -167ULL), U"test ffffffffffffff59");
		EXPECT_EQ(TString::Format(U"test %x", (u8_t)-167U), U"test 59");
		EXPECT_EQ(TString::Format(U"test %x", 0x59U), U"test 59");
		EXPECT_EQ(TString::Format(U"test %x", 0x59), U"test 59");

		EXPECT_EQ(TString::Format(U"test %b", 157), U"test 10011101");
		EXPECT_EQ(TString::Format(U"test %b", -157), U"test -10011101");

		EXPECT_EQ(TString::Format(U"test %s", "foobar %d"), U"test foobar %d");
		EXPECT_EQ(TString::Format(U"test %%s %d", 10), U"test %s 10");
	}

	TEST(io_text_string, TString_FormatCompileTimeRegistry)
	{
		using namespace el1::io::text::format;
		using el1::io::bcd::TBCD;

		static_assert(IsValidFormat<int>(U"%d"));
		static_assert(IsValidFormat<unsigned>(U"%u"));
		static_assert(!IsValidFormat<unsigned long long>(U"%llu"));
		static_assert(!IsValidFormat<unsigned long long>(U"%zu"));
		static_assert(!IsValidFormat<long long>(U"%lld"));
		static_assert(IsValidFormat<TCustomFormatValue>(U"%h"));
		static_assert(IsValidFormat<TCustomFormatValue>(U"%hm"));
		static_assert(IsValidFormat<TCustomFormatValue>(U"%{h}m"));
		static_assert(IsValidFormat<TDefaultSpecFormatValue>(U"%17.3m"));
		static_assert(IsValidFormat<int>(U"ä😀[%04d]漢"));
		static_assert(IsValidFormat<TCustomFormatValue>(U"%{ä😀}m"));
		static_assert(IsValidFormat<>(U"100%%"));
		static_assert(!IsValidFormat<int*>(U"%d"));
		static_assert(!IsValidFormat<int>(U"%p"));
		static_assert(!IsValidFormat<TCustomFormatValue>(U"%05h"));
		static_assert(!IsValidFormat<TCustomFormatValue>(U"%d"));
		static_assert(!IsValidFormat<TCustomFormatValue>(U"%{word}m"));
		static_assert(!IsValidFormat<TDefaultSpecFormatValue>(U"%{17.3}m"));
		static_assert(!IsValidFormat<int>(U"%{10}d"));
		static_assert(!CCanUseRuntimeFormatString<TString>);
		static_assert(CCanUseRuntimeFormatString<TStringView>);
		static_assert(!CCanUseRuntimeFormatString<const char*>);
		static_assert(!CCanUseRuntimeFormatString<const char32_t*>);
		static_assert(!std::is_constructible_v<TFormatString<int>, const char32_t*>);
		static_assert(!std::is_constructible_v<TFormatString<int>, const char (&)[3]>);
		static_assert(!std::is_constructible_v<TFormatString<int>, const wchar_t (&)[3]>);
		static_assert(std::is_constructible_v<TFormatString<int>, const char32_t (&)[3]>);

		constexpr TFormatString<int> format_left(U"left=%d");
		constexpr TFormatString<const char*> format_right(U", right=%s");
		constexpr auto format_combined = format_left + format_right;
		static_assert(format_combined.N_ARGUMENTS == 2U);
		static_assert(CCanFormat<decltype(format_combined), int, const char*>);
		static_assert(!CCanFormat<decltype(format_combined), unsigned, const char*>);
		EXPECT_EQ(TString::Format(format_combined, 17, "test"), U"left=17, right=test");

		constexpr TFormatString<> format_prefix(U"prefix: ");
		constexpr auto format_combined_three = format_prefix + format_left + format_right;
		static_assert(format_combined_three.N_ARGUMENTS == 2U);
		EXPECT_EQ(TString::Format(format_combined_three, 23, "ok"), U"prefix: left=23, right=ok");

		EXPECT_EQ(TString::Format(U"%10.9d", 12), U"        12.000000000");
		EXPECT_EQ(TString::Format(U"%010.9d", 12), U"0000000012.000000000");
		EXPECT_EQ(TString::Format(U"%10.9d", 12.25), U"        12.250000000");
		EXPECT_EQ(TString::Format(U"%5d", -12), U"   -12");
		EXPECT_EQ(TString::Format(U"%05d", -12), U"-00012");
		EXPECT_EQ(TString::Format(U"%_5d", -12), U"___-12");
		EXPECT_EQ(TString::Format(U"%d", std::numeric_limits<s64_t>::min()), U"-9223372036854775808");
		EXPECT_EQ(TString::Format(U"%u", 17U), U"17");
		EXPECT_EQ(TString::Format(U"%d", 1234567890123ULL), U"1234567890123");
		EXPECT_EQ(TString::Format(U"%o", 0755), U"755");
		EXPECT_EQ(TString::Format(U"%h", TCustomFormatValue{42}), U"<42>");
		EXPECT_EQ(TString::Format(U"%hm", TCustomFormatValue{42}), U"<42>m");
		EXPECT_EQ(TString::Format(U"%{h}m", TCustomFormatValue{42}), U"<2a>");
		EXPECT_EQ(TString::Format(U"%17.3m", TDefaultSpecFormatValue{42}), U"<17:3:42>");
		EXPECT_EQ(TString::Format(U"wide %04x", 42), U"wide 002a");
		EXPECT_EQ(TString::Format(U"unicode %s", "ok"), U"unicode ok");
		EXPECT_EQ(TString::Format(U"unicode %s", U"ä😀"), TStringView(U"unicode ä😀"));

		const TStringView runtime_format = U"runtime %04x %s %%";
		EXPECT_EQ(TString::Format(runtime_format, 42, U"ok"), TStringView(U"runtime 002a ok %"));

		const TStringView runtime_source = U"xx%04xyy";
		const TStringView runtime_slice = runtime_source.SliceSL(2, 4);
		EXPECT_EQ(TString::Format(runtime_slice, 42), U"002a");

		const TStringView invalid_runtime_format = U"wrong %s";
		EXPECT_THROW(TString::Format(invalid_runtime_format, 42), TException);

		static constexpr char32_t NAMED_FORMAT[] = U"named %04x";
		EXPECT_EQ(TString::Format(NAMED_FORMAT, 42), U"named 002a");

		// Native U literals are char32_t arrays before the compile-time parser sees
		// them. Formatter positions are therefore code-point indices, independent
		// of the source file's UTF-8 byte length.
		EXPECT_EQ(TString::Format(U"ä😀[%04d]漢字", 42), TStringView(U"ä😀[0042]漢字"));
		EXPECT_EQ(TString::Format(U"前%%😀%d後", 7), TStringView(U"前%😀7後"));
		EXPECT_EQ(TString::Format(U"α%dβ%sγ😀%xδ", 12, "ü", 255), TStringView(U"α12βüγ😀ffδ"));
		EXPECT_EQ(TString::Format(U"ä%{ä😀}mß", TCustomFormatValue{42}), TStringView(U"ä<U:42>ß"));

		TBCD decimal(0, 10, 2, 5);
		decimal.Digit(1, 1);
		decimal.Digit(0, 2);
		decimal.Digit(-1, 3);
		decimal.Digit(-2, 4);
		decimal.Digit(-3, 5);
		decimal.Digit(-4, 6);
		decimal.Digit(-5, 7);
		EXPECT_EQ(TString::Format(U"%2.3d", decimal), U"12.346");
		EXPECT_EQ(TString::Format(U"%5.7d", decimal), U"   12.3456700");

		int value = 0;
		const TString pointer = TString::Format(U"%p", &value);
		EXPECT_GT(pointer.Length(), 0U);
	}

	TEST(io_text_string, TString_Escape)
	{
		{
			TString s = U"she said 'hello\\world!'";
			s.Escape(TList<char32_t>({'\'', '\"'}), '\\');
			EXPECT_EQ(s, U"she said \\'hello\\\\world!\\'");
		}
	}

	TEST(io_text_string, TString_Truncate)
	{
		{
			TString s = U"hello world";
			s.Truncate(5);
			EXPECT_EQ(s, U"hello");
		}

		{
			TString s = U"hello world";
			s.Truncate(10);
			EXPECT_EQ(s, U"hello worl");
		}

		{
			TString s = U"hello world";
			s.Truncate(11);
			EXPECT_EQ(s, U"hello world");
		}

		{
			TString s = U"hello world";
			s.Truncate(0);
			EXPECT_EQ(s, U"");
		}

		{
			TString s = U"";
			s.Truncate(0);
			EXPECT_EQ(s, U"");
		}
	}

	TEST(io_text_string, TString_ToUpperLower)
	{
		{
			TString s = U"hello world 123";
			s.ToUpper();
			EXPECT_EQ(s, U"HELLO WORLD 123");
		}

		{
			TString s = U"hello world 123";
			s.ToLower();
			EXPECT_EQ(s, U"hello world 123");
		}

		{
			TString s = U"HELLO WORLD 123";
			s.ToLower();
			EXPECT_EQ(s, U"hello world 123");
		}

		{
			TString s = U"hellö wörld 123";
			s.ToUpper();
			EXPECT_EQ(s, U"HELLÖ WÖRLD 123");
		}
	}

	TEST(io_text_string, TString_Slice)
	{
		{
			TString s = U"hello world";
			EXPECT_EQ(s.SliceSL(0, 5), U"hello");
			EXPECT_EQ(s.SliceSL(6, 5), U"world");
			EXPECT_EQ(s.SliceSL(5, 1), U" ");
			EXPECT_EQ(s.SliceSL(11, 0), U"");
			EXPECT_EQ(s.SliceSL(1, 0), U"");
		}

		{
			TString s = U"hello world";
			EXPECT_EQ(s.SliceBE(0, 5), U"hello");
			EXPECT_EQ(s.SliceBE(5, 6), U" ");
			EXPECT_EQ(s.SliceBE(6, 11), U"world");
			EXPECT_EQ(s.SliceBE(6, 10), U"worl");
		}

		{
			TString s = U"hello world";

			EXPECT_THROW(s.SliceSL(10, 2), TInvalidArgumentException);
			EXPECT_THROW(s.SliceBE(-1, 5), TInvalidArgumentException);
			EXPECT_THROW(s.SliceBE(0, 12), TIndexOutOfBoundsException);
		}
	}

	TEST(io_text_string, TString_Split)
	{
		{
			TString s = U"/home/user/dev/el1";
			TList<TString> list = s.Split(TStringView(U"/"));
			EXPECT_EQ(list.Count(), 5U);
			EXPECT_EQ(list[0], U"");
			EXPECT_EQ(list[1], U"home");
			EXPECT_EQ(list[2], U"user");
			EXPECT_EQ(list[3], U"dev");
			EXPECT_EQ(list[4], U"el1");
		}

		{
			TString s = U"/home/user/dev/el1";
			TList<TString> list = s.Split(TStringView(U"/"), 2);
			EXPECT_EQ(list.Count(), 2U);
			EXPECT_EQ(list[0], U"");
			EXPECT_EQ(list[1], U"home/user/dev/el1");
		}

		{
			TString s = U"/home//user/dev/el1/";
			TList<TString> list = s.Split(TStringView(U"/"), -1U, true);
			EXPECT_EQ(list.Count(), 4U);
			EXPECT_EQ(list[0], U"home");
			EXPECT_EQ(list[1], U"user");
			EXPECT_EQ(list[2], U"dev");
			EXPECT_EQ(list[3], U"el1");
		}

		{
			TString s = U"/home/user/dev/el1";
			TList<TString> list = s.Split('/');
			EXPECT_EQ(list.Count(), 5U);
			EXPECT_EQ(list[0], U"");
			EXPECT_EQ(list[1], U"home");
			EXPECT_EQ(list[2], U"user");
			EXPECT_EQ(list[3], U"dev");
			EXPECT_EQ(list[4], U"el1");
		}

		{
			TString s = U"/home/user/dev/el1";
			TList<TString> list = s.Split('/', 2);
			EXPECT_EQ(list.Count(), 2U);
			EXPECT_EQ(list[0], U"");
			EXPECT_EQ(list[1], U"home/user/dev/el1");
		}

		{
			TString s = U"/home//user/dev/el1/";
			TList<TString> list = s.Split('/', -1U, true);
			EXPECT_EQ(list.Count(), 4U);
			EXPECT_EQ(list[0], U"home");
			EXPECT_EQ(list[1], U"user");
			EXPECT_EQ(list[2], U"dev");
			EXPECT_EQ(list[3], U"el1");
		}

		{
			TString s = U"test | blub | foobar";
			TList<TString> list = s.Split(TStringView(U" | "));
			EXPECT_EQ(list.Count(), 3U);
			EXPECT_EQ(list[0], U"test");
			EXPECT_EQ(list[1], U"blub");
			EXPECT_EQ(list[2], U"foobar");
		}

		{
			TString s = U"test||foobar";
			TList<TString> list = s.Split(TStringView(U"|"));
			EXPECT_EQ(list.Count(), 3U);
			EXPECT_EQ(list[0], U"test");
			EXPECT_EQ(list[1], U"");
			EXPECT_EQ(list[2], U"foobar");
		}

		{
			TString s = U"";
			TList<TString> list = s.Split(TStringView(U" | "));
			EXPECT_EQ(list.Count(), 1U);
			EXPECT_EQ(list[0], U"");
		}

		{
			TString s = U"";
			TList<TString> list = s.Split(TStringView(U" | "), -1U, true);
			EXPECT_EQ(list.Count(), 0U);
		}
	}

	TEST(io_text_string, TString_Unescape)
	{
		{
			TString s = U"hello\\ world";
			TList<char32_t> special_chars = { ' ' };
			s.Unescape(special_chars, '\\');
			EXPECT_EQ(s, U"hello world");
		}

		{
			TString s = U"hello world";
			TList<char32_t> special_chars = { ' ' };
			EXPECT_THROW(s.Unescape(special_chars, '\\'), TException);
		}
	}

	TEST(io_text_string, TString_Contains)
	{
		{
			TString str = U"hello world";
			EXPECT_TRUE(str.Contains(TStringView(U"hello")));
			EXPECT_TRUE(str.Contains(TStringView(U"world")));
			EXPECT_TRUE(str.Contains(TStringView(U" ")));
			EXPECT_FALSE(str.Contains(TStringView(U"-")));
			EXPECT_FALSE(str.Contains(TStringView(U"hello world1")));
			EXPECT_FALSE(str.Contains(TStringView(U"hello_world")));
		}
	}

	TEST(io_text_string, TString_Quote)
	{
		{
			TString s = U"hello world";
			s.Quote('\'', '\\');
			EXPECT_EQ(s, U"'hello world'");
		}

		{
			TString s = U"hello world 'foobar!'";
			s.Quote('\'', '\\');
			EXPECT_EQ(s, U"'hello world \\'foobar!\\''");
		}
	}

	TEST(io_text_string, TString_Unquote)
	{
		{
			TString s = U"'hello world'";
			s.Unquote('\'', '\\');
			EXPECT_EQ(s, U"hello world");
		}

		{
			TString s = U"'hello world \\'foobar!\\''";
			s.Unquote('\'', '\\');
			EXPECT_EQ(s, U"hello world 'foobar!'");
		}

		{
			TString s = U"hello world \\'foobar!\\''";
			EXPECT_THROW(s.Unquote('\'', '\\'), TInvalidArgumentException);
		}
	}

	TEST(io_text_string, TString_ToDouble)
	{
		{
			const TString str = U"-12.5";
			EXPECT_EQ(str.ToDouble(), -12.5);
		}

		{
			const TString str = U"12.5";
			EXPECT_EQ(str.ToDouble(), 12.5);
		}

		{
			const TString str = U"12";
			EXPECT_EQ(str.ToDouble(), 12);
		}
	}

	TEST(io_text_string, TString_ToInteger)
	{
		{
			const TString str = U"-12";
			EXPECT_EQ(str.ToInteger(), -12);
		}

		{
			const TString str = U"12";
			EXPECT_EQ(str.ToInteger(), 12);
		}
	}

	TEST(io_text_string, TString_BeginsWith)
	{
		{
			TString a = U"hello world";
			EXPECT_TRUE(a.BeginsWith(TStringView(U"hello")));
			EXPECT_FALSE(a.BeginsWith(TStringView(U"hello.")));
			EXPECT_FALSE(a.BeginsWith(TStringView(U"hello world ")));
		}
	}

	TEST(io_text_string, TString_ReplaceChars)
	{
		TList<char32_t> list = {' '};
		{
			TString a = U"hello world";
			a.ReplaceChars(list, '_', false);
			EXPECT_EQ(a, U"hello_world");
		}

		{
			TString a = U"hello world";
			a.ReplaceChars(list, '_', true);
			EXPECT_EQ(a, U"_____ _____");
		}
	}

	// TEST(io_text_string, TString_Parse)
	// {
	// 	const TString str = U"%test17.8$";
 //
	// 	{
	// 		double d;
	// 		auto l = str.Parse("%%test%d$", d);
	// 		EXPECT_EQ(d, 17.8);
	// 		EXPECT_EQ(l, str.Length());
	// 	}
 //
	// 	{
	// 		double d;
	// 		TString s;
	// 		auto l = str.Parse("%%%s%d$", s, d);
	// 		EXPECT_EQ(s, U"test");
	// 		EXPECT_EQ(d, 17.8);
	// 		EXPECT_EQ(l, str.Length());
	// 	}
 //
	// 	{
	// 		double d;
	// 		TString s;
	// 		auto l = str.Parse("%%%s%d", s, d);
	// 		EXPECT_EQ(s, U"test");
	// 		EXPECT_EQ(d, 17.8);
	// 		EXPECT_EQ(l, str.Length() - 1);
	// 	}
 //
	// 	{
	// 		s32_t x,y;
	// 		TString s;
	// 		auto l = str.Parse("%%%s%d.%d$", s, x, y);
	// 		EXPECT_EQ(s, U"test");
	// 		EXPECT_EQ(x, 17);
	// 		EXPECT_EQ(y, 8);
	// 		EXPECT_EQ(l, str.Length());
	// 	}
 //
	// 	{
	// 		int x,y;
 //
	// 		// unterminated '%'
	// 		EXPECT_THROW(str.Parse("%test%d$", x), TException);
 //
	// 		// format doesn't match string
	// 		EXPECT_THROW(str.Parse("%tesT%d$", x), TException);
 //
	// 		// format doesn't match string
	// 		EXPECT_THROW(str.Parse("%test%dX", x), TException);
 //
	// 		// too few arguments
	// 		EXPECT_THROW(str.Parse("%%test%d.%d$", x), TException);
 //
	// 		// too many arguments
	// 		EXPECT_THROW(str.Parse("%%test17.8$", x, y), TException);
	// 	}
	// }
}
