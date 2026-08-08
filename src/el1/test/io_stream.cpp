#include <gtest/gtest.h>
#include <el1/io_stream.hpp>
#include <el1/io_collection_list.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::error;
	using namespace el1::io::collection::list;
	using namespace el1::io::stream;
	using namespace el1::io::types;

	TEST(io_stream, TLimitSink)
	{
		TList<u32_t> output;
		TListSink<u32_t> sink(&output);
		TLimitSink<u32_t> limit_sink(&sink, 3);
		const u32_t first[] = { 1, 2 };
		const u32_t second[] = { 3, 4 };

		EXPECT_EQ(2U, limit_sink.Write(first, 2));
		EXPECT_EQ(1U, limit_sink.Write(second, 1));
		EXPECT_THROW((void)limit_sink.Write(second + 1, 1), TLimitExceededException);
		ASSERT_EQ(3U, output.Count());
		EXPECT_EQ(1U, output[0]);
		EXPECT_EQ(2U, output[1]);
		EXPECT_EQ(3U, output[2]);

		TLimitSink<u32_t> unlimited_sink(&sink, NEG1);
		EXPECT_EQ(2U, unlimited_sink.Write(first, 2));
		EXPECT_EQ(5U, output.Count());
	}

	TEST(io_stream, TListSource)
	{
		TListSource<u32_t> source(TList<u32_t>{ 10, 20, 30, 40 });
		u32_t first = 0;
		EXPECT_EQ(1U, source.Read(&first, 1));
		EXPECT_EQ(10U, first);
		EXPECT_EQ(3U, source.Remaining());

		TList<u32_t> output;
		TListSink<u32_t> sink(&output);
		EXPECT_EQ((iosize_t)3, source.WriteOut(sink, (iosize_t)-1, true));
		EXPECT_EQ(0U, source.Remaining());
		ASSERT_EQ(3U, output.Count());
		EXPECT_EQ(20U, output[0]);
		EXPECT_EQ(30U, output[1]);
		EXPECT_EQ(40U, output[2]);
	}
}
