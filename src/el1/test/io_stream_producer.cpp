#include <gtest/gtest.h>
#include <el1/io_stream_producer.hpp>
#include <el1/io_text_encoding.hpp>
#include <el1/util.hpp>
#include <string.h>

using namespace ::testing;

namespace
{
	using namespace el1::io::stream;
	using namespace el1::io::stream::producer;
	using namespace el1::io::text::encoding;
	using namespace el1::system::task;

	TEST(io_stream_producer, Produce_basics)
	{
		EXPECT_EQ(Produce<char>([](ISink<char>& sink){}).Count(), 0U);
		EXPECT_EQ(Produce<char>([](ISink<char>& sink){ sink.WriteAll("hello world", 11U); }).Count(), 11U);
		EXPECT_EQ(Produce<char>([](ISink<char>& sink){
			for(unsigned i = 0; i < 512U; i++)
				sink.WriteAll("hello world", 11U);
		}).Count(), 11U * 512U);
	}
}
