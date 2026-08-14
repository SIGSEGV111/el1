#include <gtest/gtest.h>
#include <el1/io_stream.hpp>
#include <el1/io_stream_buffer.hpp>
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
		EXPECT_EQ(3U, source.Count());

		TList<u32_t> output;
		TListSink<u32_t> sink(&output);
		EXPECT_EQ((iosize_t)3, source.WriteOut(sink, (iosize_t)-1, true));
		EXPECT_EQ(0U, source.Count());
		ASSERT_EQ(3U, output.Count());
		EXPECT_EQ(20U, output[0]);
		EXPECT_EQ(30U, output[1]);
		EXPECT_EQ(40U, output[2]);
	}
	TEST(io_stream, BufferedCollectionSources)
	{
		static_assert(std::derived_from<TListSource<u32_t>, IBufferedSource<u32_t>>);
		static_assert(std::derived_from<TArraySource<u32_t>, IBufferedSource<u32_t>>);

		const u32_t values[] = { 10, 20, 30, 40 };
		TArraySource<u32_t> array_source{array_t<const u32_t>(values)};
		EXPECT_EQ(array_source.Count(), 4U);
		EXPECT_TRUE(array_source.Ensure(4));
		EXPECT_FALSE(array_source.Ensure(5));
		EXPECT_EQ(array_source[0], 10U);
		EXPECT_EQ(array_source[2], 30U);
		EXPECT_EQ(array_source.Head().Count(), 4U);
		array_source.Shift(1);
		EXPECT_EQ(array_source.Count(), 3U);
		EXPECT_EQ(array_source.First(), 20U);

		TListSource<u32_t> list_source(TList<u32_t>{ 1, 2, 3 });
		EXPECT_EQ(list_source.Count(), 3U);
		EXPECT_EQ(list_source[2], 3U);
		list_source.Shift(2);
		EXPECT_EQ(list_source.Count(), 1U);
		EXPECT_EQ(list_source[0], 3U);
		EXPECT_FALSE(list_source.Ensure(2));
	}

	TEST(io_stream, TPullBufferImplementsBufferedCollection)
	{
		TListSource<u32_t> input(TList<u32_t>{ 5, 6, 7, 8 });
		buffer::TPullBuffer<u32_t> buffered(&input);
		EXPECT_EQ(buffered.Count(), 0U);
		ASSERT_TRUE(buffered.Ensure(3));
		EXPECT_GE(buffered.Count(), 3U);
		EXPECT_EQ(buffered[0], 5U);
		EXPECT_EQ(buffered[2], 7U);
		EXPECT_EQ(buffered.Head().Count(), buffered.Count());
		buffered.Shift(2);
		EXPECT_EQ(buffered[0], 7U);
		ASSERT_TRUE(buffered.Ensure(2));
		EXPECT_EQ(buffered[1], 8U);
	}


}
