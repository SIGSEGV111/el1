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

	struct TCountingBinarySource : el1::io::stream::IBinarySource
	{
		TList<byte_t> bytes;
		usys_t pos = 0;
		usys_t n_reads = 0;
		usys_t max_request = 0;

		explicit TCountingBinarySource(TList<byte_t> bytes) : bytes(std::move(bytes)) {}

		usys_t Read(byte_t* const output, const usys_t n_max) final
		{
			n_reads++;
			max_request = el1::util::Max(max_request, n_max);
			const usys_t n = el1::util::Min(n_max, bytes.Count() - pos);
			if(n != 0)
				memcpy(output, bytes.ItemPtr(pos), n);
			pos += n;
			return n;
		}
	};

	struct TCountingBinarySink : el1::io::stream::IBinarySink
	{
		TList<byte_t> bytes;
		usys_t n_writes = 0;
		usys_t max_write = 0;
		usys_t n_flushes = 0;

		usys_t Write(const byte_t* const input, const usys_t count) final
		{
			n_writes++;
			max_write = el1::util::Max(max_write, count);
			bytes.Append(input, count);
			return count;
		}

		void Flush() final { n_flushes++; }
	};

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

	TEST(io_text, TStreamTextReaderBuffersBinaryInputAndDecodesAhead)
	{
		TString input;
		for(usys_t i = 0; i < 64; i++)
			input += U'a';

		TCountingBinarySource source(Encode(input.View()));
		TStreamTextReader reader(&source, 16);
		EXPECT_EQ(reader.BufferSize(), 16U);
		ASSERT_TRUE(reader.Ensure(1));
		EXPECT_EQ(source.n_reads, 1U);
		EXPECT_EQ(source.max_request, 16U);
		EXPECT_GE(reader.Head().Count(), 4U);

		reader.Shift(4);
		ASSERT_TRUE(reader.Ensure(1));
		// Four more ASCII characters are decoded from the already buffered 16 bytes.
		EXPECT_EQ(source.n_reads, 1U);
		EXPECT_EQ(reader[0], U'a');

		TCountingBinarySource tiny_source(Encode(U"😀ä"));
		TStreamTextReader tiny_reader(&tiny_source, 1);
		ASSERT_TRUE(tiny_reader.Ensure(2));
		EXPECT_EQ(tiny_reader[0], U'😀');
		EXPECT_EQ(tiny_reader[1], U'ä');

		EXPECT_THROW(TStreamTextReader(&tiny_source, 0), el1::error::TInvalidArgumentException);
	}

	TEST(io_text, TStreamTextWriterBuffersAcrossAppends)
	{
		TCountingBinarySink sink;
		{
			TStreamTextWriter writer(&sink, 8);
			EXPECT_EQ(writer.BufferSize(), 8U);
			writer.Write(U"ab");
			EXPECT_EQ(sink.n_writes, 0U);
			writer.Write(U"cdefgh");
			EXPECT_EQ(sink.n_writes, 1U);
			EXPECT_EQ(sink.max_write, 8U);
			writer.Write(U"ij");
			EXPECT_EQ(sink.n_writes, 1U);
			writer.Flush();
			EXPECT_EQ(sink.n_writes, 2U);
			EXPECT_EQ(sink.n_flushes, 1U);
		}
		EXPECT_EQ(Decode(sink.bytes), U"abcdefghij");

		TCountingBinarySink destructor_sink;
		{
			TStreamTextWriter writer(&destructor_sink, 16);
			writer.Write(U"tail");
			EXPECT_EQ(destructor_sink.n_writes, 0U);
		}
		EXPECT_EQ(destructor_sink.n_writes, 1U);
		EXPECT_EQ(Decode(destructor_sink.bytes), U"tail");

		TCountingBinarySink unicode_sink;
		TStreamTextWriter unicode_writer(&unicode_sink, 3);
		unicode_writer.Write(U"😀ä");
		unicode_writer.Flush();
		EXPECT_EQ(Decode(unicode_sink.bytes), U"😀ä");

		EXPECT_THROW(TStreamTextWriter(&unicode_sink, 0), el1::error::TInvalidArgumentException);
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

	TEST(io_text, BufferedTextSources)
	{
		static_assert(std::derived_from<ITextReader, el1::io::stream::IBufferedSource<char32_t>>);
		static_assert(std::derived_from<TStringSource, el1::io::stream::IBufferedSource<char32_t>>);

		TStringSource string_source(TString(U"abcd"));
		EXPECT_EQ(string_source.Count(), 4U);
		EXPECT_TRUE(string_source.Ensure(4));
		EXPECT_EQ(string_source[0], U'a');
		EXPECT_EQ(string_source[2], U'c');
		EXPECT_EQ(string_source.Head().Count(), 4U);
		string_source.Shift(1);
		char32_t read[2] = {};
		EXPECT_EQ(string_source.Read(read, 2), 2U);
		EXPECT_EQ(read[0], U'b');
		EXPECT_EQ(read[1], U'c');
		EXPECT_EQ(string_source.First(), U'd');

		TListSource<byte_t> bytes(Encode(U"ä😀x"));
		TStreamTextReader reader(&bytes);
		ASSERT_TRUE(reader.Ensure(2));
		EXPECT_EQ(reader[1], U'😀');
		const auto head = reader.Head();
		ASSERT_GE(head.Count(), 2U);
		EXPECT_EQ(head[0], U'ä');
		reader.Shift(2);
		ASSERT_TRUE(reader.Ensure(1));
		EXPECT_EQ(reader[0], U'x');
	}


	TEST(io_text, TStringViewTextReaderBorrowsWithoutCopying)
	{
		TString text(U"1234");
		const TStringView view = text.View();
		TStringViewTextReader reader(view);
		ASSERT_TRUE(reader.Ensure(4));
		EXPECT_EQ(reader.ItemPtr(0), view.Data());
		u16_t value = 0;
		EXPECT_TRUE(reader.TryScan(U"%d", value));
		EXPECT_EQ(value, 1234U);
		EXPECT_EQ(reader.Count(), 0U);
	}

	TEST(io_text, TextReaderTracksCharacterAndLineIndices)
	{
		TStringTextReader string_reader(TString(U"a\nbc\n"));
		EXPECT_EQ(string_reader.CharacterIndex(), (iosize_t)0);
		EXPECT_EQ(string_reader.LineIndex(), (iosize_t)0);
		EXPECT_EQ(string_reader.Position(4).character_index, (iosize_t)4);
		EXPECT_EQ(string_reader.Position(4).line_index, (iosize_t)1);

		string_reader.Shift(3);
		EXPECT_EQ(string_reader.CharacterIndex(), (iosize_t)3);
		EXPECT_EQ(string_reader.LineIndex(), (iosize_t)1);
		EXPECT_EQ(string_reader.Position(2).character_index, (iosize_t)5);
		EXPECT_EQ(string_reader.Position(2).line_index, (iosize_t)2);

		TListTextReader list_reader(TList<char32_t>{U'x', U'\n', U'y'});
		list_reader.Shift(2);
		EXPECT_EQ(list_reader.CharacterIndex(), (iosize_t)2);
		EXPECT_EQ(list_reader.LineIndex(), (iosize_t)1);

		TListSource<byte_t> bytes(Encode(U"ä\n😀x"));
		TStreamTextReader stream_reader(&bytes);
		ASSERT_TRUE(stream_reader.Ensure(4));
		EXPECT_EQ(stream_reader[3], U'x');
		stream_reader.Shift(3);
		EXPECT_EQ(stream_reader.CharacterIndex(), (iosize_t)3);
		EXPECT_EQ(stream_reader.LineIndex(), (iosize_t)1);
		ASSERT_TRUE(stream_reader.Ensure(1));
		EXPECT_EQ(stream_reader[0], U'x');
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

	TEST(io_text, TTextReaderScanIntegerRangeAndHex)
	{
		TStringTextReader reader(TString(U"9223372036854775807 fFfF"));
		s64_t signed_value = 0;
		u16_t hex_value = 0;
		EXPECT_TRUE(reader.TryScan(U"%d %x", signed_value, hex_value));
		EXPECT_EQ(signed_value, std::numeric_limits<s64_t>::max());
		EXPECT_EQ(hex_value, 0xffffu);

		TStringTextReader signed_overflow(TString(U"9223372036854775808"));
		signed_value = 123;
		EXPECT_FALSE(signed_overflow.TryScan(U"%d", signed_value));
		EXPECT_EQ(signed_value, 123);
		EXPECT_EQ(signed_overflow.CharacterIndex(), (iosize_t)0);

		TStringTextReader hex_overflow(TString(U"10000"));
		hex_value = 123;
		EXPECT_FALSE(hex_overflow.TryScan(U"%x", hex_value));
		EXPECT_EQ(hex_value, 123);
		EXPECT_EQ(hex_overflow.CharacterIndex(), (iosize_t)0);
	}

	TEST(io_text, ParseNumberUsesScannerConversionBackend)
	{
		const auto decimal = scan::ParseNumber<s64_t>(U"-9223372036854775808", 10);
		ASSERT_TRUE(decimal);
		EXPECT_EQ(*decimal, std::numeric_limits<s64_t>::min());
		EXPECT_FALSE(scan::ParseNumber<s64_t>(U"9223372036854775808", 10));

		const auto hexadecimal = scan::ParseNumber<u16_t>(U"fFfF", 16);
		ASSERT_TRUE(hexadecimal);
		EXPECT_EQ(*hexadecimal, 0xffffu);

		const auto floating = scan::ParseNumber<double>(U"-2.5E-2", 10);
		ASSERT_TRUE(floating);
		EXPECT_DOUBLE_EQ(*floating, -0.025);
	}

	TEST(io_text, TTextReaderScanFloatingExponent)
	{
		TStringTextReader reader(TString(U"1e3 -2.5E-2 +.5 1e"));

		double first = 0;
		double second = 0;
		double third = 0;
		EXPECT_TRUE(reader.TryScan(U"%f %f %f ", first, second, third));
		EXPECT_DOUBLE_EQ(first, 1000.0);
		EXPECT_DOUBLE_EQ(second, -0.025);
		EXPECT_DOUBLE_EQ(third, 0.5);

		const iosize_t before = reader.CharacterIndex();
		double malformed = 123.0;
		EXPECT_FALSE(reader.TryScan(U"%f", malformed));
		EXPECT_EQ(malformed, 123.0);
		EXPECT_EQ(reader.CharacterIndex(), before);
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
