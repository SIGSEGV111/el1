#include <gtest/gtest.h>
#include <el1/io_text.hpp>
#include <el1/io_text_string.hpp>
#include "util.hpp"

using namespace ::testing;

namespace
{
	using namespace el1::io::text;
	using namespace el1::io::text::string;
	using namespace el1::error;

	TEST(io_text_string, TString_Construct)
	{
		{
			TString s;
			EXPECT_EQ(s.Length(), 0U);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Length(), 11U);
			EXPECT_EQ(s[4], 'o');
		}

		{
			TString s = L"hello world äöü";
			EXPECT_EQ(s.Length(), 15U);
			EXPECT_EQ(s[4], 'o');
		}

		{
			const TUTF32 arr[] = { 'h', 'e', 'l', 'l', 'o', TUTF32::TERMINATOR, 'w', 'o', 'r', 'l', 'd', TUTF32::TERMINATOR };
			TString s = arr;
			EXPECT_EQ(s.Length(), 5U);
			EXPECT_EQ(s[4], 'o');
		}

		{
			const TUTF32 arr[] = { 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', TUTF32::TERMINATOR };
			TString s(arr, 6);
			EXPECT_EQ(s.Length(), 6U);
			EXPECT_EQ(s[5], ' ');
		}
	}

	TEST(io_text_string, TString_Compare)
	{
		const TString s1 = "hello world";
		const TString s2 = "foobar";
		const TString s3 = "hello world ";
		const TString s4 = "ihello world";
		const TString s5 = "hello world";

		EXPECT_EQ(s1, s5);
		EXPECT_NE(s1, TString("hello worl_"));
		EXPECT_NE(s1, TString("foobar"));
		EXPECT_EQ(s2, TString("foobar"));

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

		EXPECT_TRUE(s1.BeginsWith("hello"));
		EXPECT_FALSE(s1.BeginsWith("hello!"));

		EXPECT_TRUE(s1.EndsWith("world"));
		EXPECT_FALSE(s1.EndsWith("world!"));
		EXPECT_FALSE(s1.EndsWith("_world"));
	}

	TEST(io_text_string, TString_Concat)
	{
		TString a = "hello";
		TString b = "world";
		TString r = a + " " + b + ' ' + '1';
		EXPECT_EQ(r, TString("hello world 1"));
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
			TString s = " \thello world \t ";
			s.Trim();
			EXPECT_EQ(s, "hello world");
		}

		{
			TString s = "hello world \t ";
			s.Trim();
			EXPECT_EQ(s, "hello world");
		}

		{
			TString s = " hello world";
			s.Trim();
			EXPECT_EQ(s, "hello world");
		}

		{
			TString s = " \thello world \t ";
			s.Trim(true, false);
			EXPECT_EQ(s, "hello world \t ");
		}

		{
			TString s = " \thello world \t ";
			s.Trim(false, true);
			EXPECT_EQ(s, " \thello world");
		}

		{
			TString s = "hello world";
			s.Trim();
			EXPECT_EQ(s, "hello world");
		}

		{
			TString s = "h ello worl\td";
			s.Trim();
			EXPECT_EQ(s, "h ello worl\td");
		}

		{
			TString s = " \t ";
			s.Trim();
			EXPECT_EQ(s, "");
		}
	}

	TEST(io_text_string, TString_ReplaceAt)
	{
		{
			TString s = "hello world";
			s.ReplaceAt(0, 5, "foobar");
			EXPECT_EQ(s, "foobar world");
		}

		{
			TString s = "hello world";
			s.ReplaceAt(6, 5, "foobar");
			EXPECT_EQ(s, "hello foobar");
		}

		{
			TString s = "hello world";
			s.ReplaceAt(5, 1, " brave new ");
			EXPECT_EQ(s, "hello brave new world");
		}

		{
			TString s = "hello world";
			s.ReplaceAt(6, 0, "brave new ");
			EXPECT_EQ(s, "hello brave new world");
		}

		{
			TString s = "hello world";
			s.ReplaceAt(0, 11, "");
			EXPECT_EQ(s, "");
		}

		{
			TString s = "hello world";
			s.ReplaceAt(1, 9, "");
			EXPECT_EQ(s, "hd");
		}

		{
			TString s = "hello world";
			s.ReplaceAt(0, 5, "test1");
			EXPECT_EQ(s, "test1 world");
		}

		{
			TString s = "hello world";
			s.ReplaceAt(-5, 5, "test1");
			EXPECT_EQ(s, "hello test1");
		}

		{
			TString s = "hello world";
			s.ReplaceAt(11, 0, " test1");
			EXPECT_EQ(s, "hello world test1");
		}

		{
			TString s = "hello world";
			EXPECT_THROW(s.ReplaceAt(12, 0, " test1"), el1::error::TIndexOutOfBoundsException);
		}
	}

	TEST(io_text_string, TString_Replace)
	{
		{
			TString s = "hello world";
			s.Replace("hello", "foobar");
			EXPECT_EQ(s, "foobar world");
		}

		{
			TString s = "hello world";
			s.Replace("l", "_", 0, false, 2);
			EXPECT_EQ(s, "he__o world");
// 			printf("s = '%s'\n", s.MakeCStr());
		}

		{
			TString s = "hello world";
			s.Replace("l", "_", -1, true, 2);
			EXPECT_EQ(s, "hel_o wor_d");
		}

		{
			TString s = "";
			s.Replace("a", "b");
			EXPECT_EQ(s, "");
		}

		{
			TString s = "test";
			s.Replace("test", "foobar");
			EXPECT_EQ(s, "foobar");
		}

		{
			TString s = "test";
			s.Replace("test", "");
			EXPECT_EQ(s, "");
		}

		{
			TString s = "test";
			s.Replace("t", "ttt");
			EXPECT_EQ(s, "tttesttt");
		}

		{
			TString s = "test";
			s.Replace("t", "t");
			EXPECT_EQ(s, "test");
		}

		{
			TString s = "test";
			EXPECT_THROW(s.Replace("", "test"), TInvalidArgumentException);
		}
	}

	TEST(io_text_string, TString_Find)
	{
		{
			TString s = "hello world";
			EXPECT_THROW(s.Find(""), TInvalidArgumentException);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find("world"), 6U);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find("test"), -1UL);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find(" "), 5UL);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find(s), 0UL);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find(s, -1, true), 0UL);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find(s, -2, true), -1UL);
		}

		{
			TString s = "hello world";
			EXPECT_THROW(s.Find(s, s.Length(), true), el1::error::TIndexOutOfBoundsException);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find(s, 1, false), -1UL);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find("hello worl_"), -1UL);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find("o", 6), 7UL);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find("o", 7), 7UL);
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.Find("w", 7), -1UL);
		}
	}

	TEST(io_text_string, TString_Pad)
	{
		{
			TString s = "hello world";
			s.Pad('_', 13, EPlacement::END);
			EXPECT_EQ(s, "hello world__");
		}

		{
			TString s = "hello world";
			s.Pad('_', 11, EPlacement::END);
			EXPECT_EQ(s, "hello world");
		}

		{
			TString s = "hello world";
			s.Pad('_', 13, EPlacement::START);
			EXPECT_EQ(s, "__hello world");
		}

		{
			TString s = "hello world";
			s.Pad('_', 13, EPlacement::MID);
			EXPECT_EQ(s, "hello__ world");
		}
	}

	TEST(io_text_string, TString_Reverse)
	{
		{
			TString s = "hello world";
			s.Reverse();
			EXPECT_EQ(s, "dlrow olleh");
		}
	}

	TEST(io_text_string, TString_Format)
	{
		EXPECT_EQ(TString::Format("hello %s", 'a'), "hello a");
		EXPECT_EQ(TString::Format("hello %s", L'a'), "hello a");
		EXPECT_EQ(TString::Format("hello %s", TUTF32('a')), "hello a");
		EXPECT_EQ(TString::Format("hello %s", "foobar"), "hello foobar");
		EXPECT_EQ(TString::Format("hello %s", ""), "hello ");
		EXPECT_EQ(TString::Format("test %d", 17), "test 17");
		EXPECT_EQ(TString::Format("test %d %d", 17572, 13), "test 17572 13");
		EXPECT_EQ(TString::Format("test %d", -17), "test -17");
		EXPECT_EQ(TString::Format("hello %s%d", "foobar", 3), "hello foobar3");
		EXPECT_EQ(TString::Format("progress = %d%%", 17), "progress = 17%");
		EXPECT_EQ(TString::Format("path = %q", "/opt/el1/src"), "path = '/opt/el1/src'");
		EXPECT_EQ(TString::Format("text = %q", "'some quoted text'"), "text = \"'some quoted text'\"");
		EXPECT_EQ(TString::Format("text = %q", L"'some quoted text'"), "text = \"'some quoted text'\"");
		EXPECT_THROW(TString::Format("wrong %g", 17), TException);
		EXPECT_THROW(TString::Format("wrong %%", 17), TException);
		EXPECT_THROW(TString::Format("wrong %s", 17), TException);

		EXPECT_EQ(TString::Format("test %d", 0), "test 0");

		EXPECT_EQ(TString::Format("test %d", (s8_t)17), "test 17");
		EXPECT_EQ(TString::Format("test %d", (u8_t)17), "test 17");
		EXPECT_EQ(TString::Format("test %d", (s16_t)17), "test 17");
		EXPECT_EQ(TString::Format("test %d", (u16_t)17), "test 17");
		EXPECT_EQ(TString::Format("test %d", (s32_t)17), "test 17");
		EXPECT_EQ(TString::Format("test %d", (u32_t)17), "test 17");
		EXPECT_EQ(TString::Format("test %d", (s64_t)17), "test 17");
		EXPECT_EQ(TString::Format("test %d", (u64_t)17), "test 17");

		// this is not exception-/thread-safe
		TNumberFormatter nf = TNumberFormatter::PLAIN_DECIMAL_US_EN;
		nf.config.n_decimal_places = 3;
		TNumberFormatter::DEFAULT_DECIMAL = &nf;
		EXPECT_EQ(TString::Format("test %d", 17.5725), "test 17.573");
		EXPECT_EQ(TString::Format("test %d",  17.9995), "test 18.000");
		EXPECT_EQ(TString::Format("test %d", -17.9995), "test -18.000");
		EXPECT_EQ(TString::Format("test %d", -17.9994), "test -17.999");
		TNumberFormatter::DEFAULT_DECIMAL = &TNumberFormatter::PLAIN_DECIMAL_US_EN;

		EXPECT_EQ(TString::Format("test %02x", 10), "test 0a");
		EXPECT_EQ(TString::Format("test %_2.7x", 10), "test _a.0000000");
		EXPECT_EQ(TString::Format("test %x", 17), "test 11");
		EXPECT_EQ(TString::Format("test %x", 167), "test a7");
		EXPECT_EQ(TString::Format("test %x", -167), "test -a7");
		EXPECT_EQ(TString::Format("test %x", -167ULL), "test ffffffffffffff59");
		EXPECT_EQ(TString::Format("test %x", (u8_t)-167U), "test 59");
		EXPECT_EQ(TString::Format("test %x", 0x59U), "test 59");
		EXPECT_EQ(TString::Format("test %x", 0x59), "test 59");

		EXPECT_EQ(TString::Format("test %b", 157), "test 10011101");
		EXPECT_EQ(TString::Format("test %b", -157), "test -10011101");

		EXPECT_EQ(TString::Format("test %s", "foobar %d"), "test foobar %d");
		EXPECT_EQ(TString::Format("test %%s %d", 10), "test %s 10");
	}

	TEST(io_text_string, TString_Escape)
	{
		{
			TString s = "she said 'hello\\world!'";
			s.Escape(TList<TUTF32>({'\'', '\"'}), '\\');
			EXPECT_EQ(s, "she said \\'hello\\\\world!\\'");
		}
	}

	TEST(io_text_string, TString_Truncate)
	{
		{
			TString s = "hello world";
			s.Truncate(5);
			EXPECT_EQ(s, "hello");
		}

		{
			TString s = "hello world";
			s.Truncate(10);
			EXPECT_EQ(s, "hello worl");
		}

		{
			TString s = "hello world";
			s.Truncate(11);
			EXPECT_EQ(s, "hello world");
		}

		{
			TString s = "hello world";
			s.Truncate(0);
			EXPECT_EQ(s, "");
		}

		{
			TString s = "";
			s.Truncate(0);
			EXPECT_EQ(s, "");
		}
	}

	TEST(io_text_string, TString_ToUpperLower)
	{
		{
			TString s = "hello world 123";
			s.ToUpper();
			EXPECT_EQ(s, "HELLO WORLD 123");
		}

		{
			TString s = "hello world 123";
			s.ToLower();
			EXPECT_EQ(s, "hello world 123");
		}

		{
			TString s = "HELLO WORLD 123";
			s.ToLower();
			EXPECT_EQ(s, "hello world 123");
		}

		{
			TString s = L"hellö wörld 123";
			s.ToUpper();
			EXPECT_EQ(s, L"HELLÖ WÖRLD 123");
		}
	}

	TEST(io_text_string, TString_Slice)
	{
		{
			TString s = "hello world";
			EXPECT_EQ(s.SliceSL(0, 5), "hello");
			EXPECT_EQ(s.SliceSL(6, 5), "world");
			EXPECT_EQ(s.SliceSL(5, 1), " ");
			EXPECT_EQ(s.SliceSL(11, 0), "");
			EXPECT_EQ(s.SliceSL(1, 0), "");
		}

		{
			TString s = "hello world";
			EXPECT_EQ(s.SliceBE(0, 5), "hello");
			EXPECT_EQ(s.SliceBE(5, 6), " ");
			EXPECT_EQ(s.SliceBE(6, 11), "world");
			EXPECT_EQ(s.SliceBE(6, 10), "worl");
		}

		{
			TString s = "hello world";

			EXPECT_THROW(s.SliceSL(10, 2), TInvalidArgumentException);
			EXPECT_THROW(s.SliceBE(-1, 5), TInvalidArgumentException);
			EXPECT_THROW(s.SliceBE(0, 12), TIndexOutOfBoundsException);
		}
	}

	TEST(io_text_string, TString_Split)
	{
		{
			TString s = "/home/user/dev/el1";
			TList<TString> list = s.Split("/");
			EXPECT_EQ(list.Count(), 5U);
			EXPECT_EQ(list[0], "");
			EXPECT_EQ(list[1], "home");
			EXPECT_EQ(list[2], "user");
			EXPECT_EQ(list[3], "dev");
			EXPECT_EQ(list[4], "el1");
		}

		{
			TString s = "/home/user/dev/el1";
			TList<TString> list = s.Split("/", 2);
			EXPECT_EQ(list.Count(), 2U);
			EXPECT_EQ(list[0], "");
			EXPECT_EQ(list[1], "home/user/dev/el1");
		}

		{
			TString s = "/home//user/dev/el1/";
			TList<TString> list = s.Split("/", -1U, true);
			EXPECT_EQ(list.Count(), 4U);
			EXPECT_EQ(list[0], "home");
			EXPECT_EQ(list[1], "user");
			EXPECT_EQ(list[2], "dev");
			EXPECT_EQ(list[3], "el1");
		}

		{
			TString s = "/home/user/dev/el1";
			TList<TString> list = s.Split('/');
			EXPECT_EQ(list.Count(), 5U);
			EXPECT_EQ(list[0], "");
			EXPECT_EQ(list[1], "home");
			EXPECT_EQ(list[2], "user");
			EXPECT_EQ(list[3], "dev");
			EXPECT_EQ(list[4], "el1");
		}

		{
			TString s = "/home/user/dev/el1";
			TList<TString> list = s.Split('/', 2);
			EXPECT_EQ(list.Count(), 2U);
			EXPECT_EQ(list[0], "");
			EXPECT_EQ(list[1], "home/user/dev/el1");
		}

		{
			TString s = "/home//user/dev/el1/";
			TList<TString> list = s.Split('/', -1U, true);
			EXPECT_EQ(list.Count(), 4U);
			EXPECT_EQ(list[0], "home");
			EXPECT_EQ(list[1], "user");
			EXPECT_EQ(list[2], "dev");
			EXPECT_EQ(list[3], "el1");
		}

		{
			TString s = "test | blub | foobar";
			TList<TString> list = s.Split(" | ");
			EXPECT_EQ(list.Count(), 3U);
			EXPECT_EQ(list[0], "test");
			EXPECT_EQ(list[1], "blub");
			EXPECT_EQ(list[2], "foobar");
		}

		{
			TString s = "test||foobar";
			TList<TString> list = s.Split("|");
			EXPECT_EQ(list.Count(), 3U);
			EXPECT_EQ(list[0], "test");
			EXPECT_EQ(list[1], "");
			EXPECT_EQ(list[2], "foobar");
		}

		{
			TString s = "";
			TList<TString> list = s.Split(" | ");
			EXPECT_EQ(list.Count(), 1U);
			EXPECT_EQ(list[0], "");
		}

		{
			TString s = "";
			TList<TString> list = s.Split(" | ", -1U, true);
			EXPECT_EQ(list.Count(), 0U);
		}
	}

	TEST(io_text_string, TString_Unescape)
	{
		{
			TString s = "hello\\ world";
			TList<TUTF32> special_chars = { ' ' };
			s.Unescape(special_chars, '\\');
			EXPECT_EQ(s, "hello world");
		}

		{
			TString s = "hello world";
			TList<TUTF32> special_chars = { ' ' };
			EXPECT_THROW(s.Unescape(special_chars, '\\'), TException);
		}
	}

	TEST(io_text_string, TString_Contains)
	{
		{
			TString str = "hello world";
			EXPECT_TRUE(str.Contains("hello"));
			EXPECT_TRUE(str.Contains("world"));
			EXPECT_TRUE(str.Contains(" "));
			EXPECT_FALSE(str.Contains("-"));
			EXPECT_FALSE(str.Contains("hello world1"));
			EXPECT_FALSE(str.Contains("hello_world"));
		}
	}

	TEST(io_text_string, TString_Quote)
	{
		{
			TString s = "hello world";
			s.Quote('\'', '\\');
			EXPECT_EQ(s, "'hello world'");
		}

		{
			TString s = "hello world 'foobar!'";
			s.Quote('\'', '\\');
			EXPECT_EQ(s, "'hello world \\'foobar!\\''");
		}
	}

	TEST(io_text_string, TString_Unquote)
	{
		{
			TString s = "'hello world'";
			s.Unquote('\'', '\\');
			EXPECT_EQ(s, "hello world");
		}

		{
			TString s = "'hello world \\'foobar!\\''";
			s.Unquote('\'', '\\');
			EXPECT_EQ(s, "hello world 'foobar!'");
		}

		{
			TString s = "hello world \\'foobar!\\''";
			EXPECT_THROW(s.Unquote('\'', '\\'), TInvalidArgumentException);
		}
	}

	TEST(io_text_string, TString_ToDouble)
	{
		{
			const TString str = "-12.5";
			EXPECT_EQ(str.ToDouble(), -12.5);
		}

		{
			const TString str = "12.5";
			EXPECT_EQ(str.ToDouble(), 12.5);
		}

		{
			const TString str = "12";
			EXPECT_EQ(str.ToDouble(), 12);
		}
	}

	TEST(io_text_string, TString_ToInteger)
	{
		{
			const TString str = "-12";
			EXPECT_EQ(str.ToInteger(), -12);
		}

		{
			const TString str = "12";
			EXPECT_EQ(str.ToInteger(), 12);
		}
	}

	TEST(io_text_string, TString_BeginsWith)
	{
		{
			TString a = "hello world";
			EXPECT_TRUE(a.BeginsWith("hello"));
			EXPECT_FALSE(a.BeginsWith("hello."));
			EXPECT_FALSE(a.BeginsWith("hello world "));
		}
	}

	// TEST(io_text_string, TString_Parse)
	// {
	// 	const TString str = "%test17.8$";
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
	// 		EXPECT_EQ(s, "test");
	// 		EXPECT_EQ(d, 17.8);
	// 		EXPECT_EQ(l, str.Length());
	// 	}
 //
	// 	{
	// 		double d;
	// 		TString s;
	// 		auto l = str.Parse("%%%s%d", s, d);
	// 		EXPECT_EQ(s, "test");
	// 		EXPECT_EQ(d, 17.8);
	// 		EXPECT_EQ(l, str.Length() - 1);
	// 	}
 //
	// 	{
	// 		s32_t x,y;
	// 		TString s;
	// 		auto l = str.Parse("%%%s%d.%d$", s, x, y);
	// 		EXPECT_EQ(s, "test");
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
