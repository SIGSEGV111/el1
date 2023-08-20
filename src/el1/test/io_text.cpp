#include <gtest/gtest.h>
#include <el1/io_text.hpp>
#include <el1/io_text_encoding.hpp>
#include <el1/io_text_string.hpp>
#include <el1/io_collection_list.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::text;
	using namespace el1::io::text::string;
	using namespace el1::io::text::encoding;
	using namespace el1::io::collection::list;

	TEST(io_text, TLineReader)
	{
		// LF only
		{
			const TString input_str = "hello world\nfoobar\ntest\n\n";
			const TList< TString> lines = input_str.chars.Pipe().Transform(TLineReader()).Collect();
			EXPECT_EQ(lines.Count(), 4U);
			EXPECT_EQ(lines[0], "hello world");
			EXPECT_EQ(lines[1], "foobar");
			EXPECT_EQ(lines[2], "test");
			EXPECT_EQ(lines[3], "");
		}

		// CR LF
		{
			const TString input_str = "hello world\r\nfoobar\r\ntest\r\n\r\n";
			const TList< TString> lines = input_str.chars.Pipe().Transform(TLineReader()).Collect();
			EXPECT_EQ(lines.Count(), 4U);
			EXPECT_EQ(lines[0], "hello world");
			EXPECT_EQ(lines[1], "foobar");
			EXPECT_EQ(lines[2], "test");
			EXPECT_EQ(lines[3], "");
		}

		// mixing CR LF and LF only
		{
			const TString input_str = "hello world\r\nfoobar\ntest\n\r\n";
			const TList< TString> lines = input_str.chars.Pipe().Transform(TLineReader()).Collect();
			EXPECT_EQ(lines.Count(), 4U);
			EXPECT_EQ(lines[0], "hello world");
			EXPECT_EQ(lines[1], "foobar");
			EXPECT_EQ(lines[2], "test");
			EXPECT_EQ(lines[3], "");
		}

		// preserving CR when alone
		{
			const TString input_str = "hello\rworld\r\nfoobar\ntest\n\r\n";
			const TList< TString> lines = input_str.chars.Pipe().Transform(TLineReader()).Collect();
			EXPECT_EQ(lines.Count(), 4U);
			EXPECT_EQ(lines[0], "hello\rworld");
			EXPECT_EQ(lines[1], "foobar");
			EXPECT_EQ(lines[2], "test");
			EXPECT_EQ(lines[3], "");
		}

		// start with LF
		{
			const TString input_str = "\nhello world\r\nfoobar\ntest\n\r\n";
			const TList< TString> lines = input_str.chars.Pipe().Transform(TLineReader()).Collect();
			EXPECT_EQ(lines.Count(), 5U);
			EXPECT_EQ(lines[0], "");
			EXPECT_EQ(lines[1], "hello world");
			EXPECT_EQ(lines[2], "foobar");
			EXPECT_EQ(lines[3], "test");
			EXPECT_EQ(lines[4], "");
		}

		// no LF at end
		{
			const TString input_str = "hello world\nfoobar\ntest\nlast line";
			TLineReader lr;
			lr.buffer = "should not be visible";	// this is for testing only - your are not supposed to use it this way!
			const TList< TString> lines = input_str.chars.Pipe().Transform(lr).Collect();
			EXPECT_EQ(lines.Count(), 4U);
			EXPECT_EQ(lines[0], "hello world");
			EXPECT_EQ(lines[1], "foobar");
			EXPECT_EQ(lines[2], "test");
			EXPECT_EQ(lines[3], "last line");
			EXPECT_EQ(lr.buffer.chars.Count(), 0U);	// ensure that the TString is moved into the collection in the Collect() step and that lr was taken by reference and not copied in the Transform(lr) call - again this is only for testing and not supposed to be used this way
		}
	}

	// TEST(io_text, TTextWriter_basics)
	// {
	// 	TList<TUTF32> list;
	// 	TListSink<TUTF32> sink(&list);
	// 	TTextWriter::std.Format(sink, L"hello world %b %o %d %x %p %f %s %q", 112, 112, 112, 112, &list, 112.27, L"foobar", L"foobar");
	// }
}
