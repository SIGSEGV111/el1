#include <gtest/gtest.h>
#include <el1/io_text.hpp>
#include <el1/io_text_encoding.hpp>
#include <el1/io_text_string.hpp>
#include <el1/io_collection_list.hpp>

using namespace ::testing;

struct TYesNo
{
	bool value;
};

namespace el1::io::text::scan
{
	template<>
	struct TScanner<::TYesNo>
	{
		static constexpr bool Supports(const char32_t code) noexcept { return code == U'y'; }
		static constexpr bool Validate(const char32_t, const format::TDefaultFormatSpec& spec) noexcept
		{
			return !spec.has_pad && !spec.has_width && !spec.has_precision;
		}

		static auto Parser(const char32_t, const format::TDefaultFormatSpec&)
		{
			using namespace parser;
			return Translate([](string::TString text) { return TYesNo{text == U"yes"}; }, U"yes"_P || U"no"_P);
		}
	};
}

namespace
{
	using namespace el1::io::text;
	using namespace el1::io::text::string;
	using namespace el1::io::text::encoding;
	using namespace el1::io::text::parser;
	using namespace el1::io::collection::list;

	TList<byte_t> Encode(const TStringView text)
	{
		TList<char32_t> chars(text);
		return chars.Pipe().Transform(TCharEncoder()).Collect();
	}

	TString Decode(TList<byte_t>& bytes)
	{
		return bytes.Pipe().Transform(TCharDecoder()).Collect();
	}

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
			auto line_pipe = input_str.chars.Pipe().Transform(lr);
			// The temporary array pipe is now owned by the transform adapter. The
			// stateful lvalue transformator remains borrowed and therefore observable.
			const TList< TString> lines = line_pipe.Collect();
			EXPECT_EQ(lines.Count(), 4U);
			EXPECT_EQ(lines[0], "hello world");
			EXPECT_EQ(lines[1], "foobar");
			EXPECT_EQ(lines[2], "test");
			EXPECT_EQ(lines[3], "last line");
			EXPECT_EQ(lr.buffer.chars.Count(), 0U);	// ensure that the TString is moved into the collection in the Collect() step and that lr was taken by reference and not copied in the Transform(lr) call - again this is only for testing and not supposed to be used this way
		}
	}

	TEST(io_text, TStreamTextWriterPrintUsesFormatRegistry)
	{
		TList<byte_t> bytes;
		TListSink<byte_t> sink(&bytes);
		TStreamTextWriter writer(&sink);

		writer.Print(U"ä😀 [%04d] %q %x", 42, U"Hällö", 255);
		writer.Print(U" / 100%%");
		writer.Write(U" raw%%");
		writer.Flush();

		EXPECT_EQ(Decode(bytes), U"ä😀 [0042] 'Hällö' ff / 100% raw%%");
	}

	TEST(io_text, TStreamTextReaderScanUnicodeAndTypes)
	{
		static_assert(scan::IsValidScan<s32_t, u32_t, TString, char32_t>(U"ä😀 %d %x %q %c"));
		static_assert(!scan::IsValidScan<s32_t>(U"%s"));
		static_assert(!scan::IsValidScan<TString>(U"%d"));
		static_assert(!scan::IsValidScan<s32_t>(U"%05d"));
		TListSource<byte_t> source(Encode(U"ä😀 42 ff 'hello world' Z tail"));
		TStreamTextReader reader(&source);

		s32_t decimal = 0;
		u32_t hexadecimal = 0;
		TString quoted;
		char32_t character = U'\0';
		EXPECT_TRUE(reader.TryScan(U"ä😀 %d %x %q %c ", decimal, hexadecimal, quoted, character));
		EXPECT_EQ(decimal, 42);
		EXPECT_EQ(hexadecimal, 255U);
		EXPECT_EQ(quoted, U"hello world");
		EXPECT_EQ(character, U'Z');

		TString tail;
		EXPECT_GT(reader.Scan(U"%s", tail), 0U);
		EXPECT_EQ(tail, U"tail");

		using TFixed = el1::io::bcd::TFixedBCD<8, 4, 4, 10>;
		TListSource<byte_t> fixed_source(Encode(U"12.3456"));
		TStreamTextReader fixed_reader(&fixed_source);
		TFixed fixed;
		EXPECT_TRUE(fixed_reader.TryScan(U"%d", fixed));
		EXPECT_EQ(TString::Format(U"%.4d", fixed), U"12.3456");
	}

	TEST(io_text, TStreamTextReaderScanRollbackAndWidth)
	{
		TListSource<byte_t> source(Encode(U"12345 xyz"));
		TStreamTextReader reader(&source);

		s32_t value = 77;
		EXPECT_FALSE(reader.TryScan(U"%3d-nope", value));
		EXPECT_EQ(value, 77);

		EXPECT_TRUE(reader.TryScan(U"%3d", value));
		EXPECT_EQ(value, 123);

		s32_t rest = 0;
		TString word;
		EXPECT_TRUE(reader.TryScan(U"%d %s", rest, word));
		EXPECT_EQ(rest, 45);
		EXPECT_EQ(word, U"xyz");
	}

	TEST(io_text, TStreamTextReaderScannerCanUseParser)
	{
		static_assert(scan::IsValidScan<TYesNo>(U"%y"));
		TListSource<byte_t> source(Encode(U"yes no"));
		TStreamTextReader reader(&source);

		TYesNo first{false};
		TYesNo second{true};
		EXPECT_TRUE(reader.TryScan(U"%y ", first));
		EXPECT_TRUE(reader.TryScan(U"%y", second));
		EXPECT_TRUE(first.value);
		EXPECT_FALSE(second.value);
	}

	TEST(io_text, TStreamTextReaderParserIntegration)
	{
		TListSource<byte_t> source(Encode(U"foobar!remaining"));
		TStreamTextReader reader(&source);

		const auto parser = Discard(U"foo"_P) + U"bar"_P + Discard(U'!'_P);
		auto value = reader.TryParse(parser);
		ASSERT_TRUE(value);
		EXPECT_EQ(*value, U"bar");

		TString remaining;
		EXPECT_TRUE(reader.TryScan(U"%s", remaining));
		EXPECT_EQ(remaining, U"remaining");
	}

}
