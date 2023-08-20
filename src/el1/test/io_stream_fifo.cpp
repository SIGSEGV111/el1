#include <gtest/gtest.h>
#include <el1/io_stream_fifo.hpp>
#include <el1/io_text.hpp>
#include <el1/util.hpp>
#include <string.h>

using namespace ::testing;

namespace
{
	using namespace el1::io::stream;
	using namespace el1::io::stream::fifo;
	using namespace el1::io::text;
	using namespace el1::system::task;

	TEST(io_stream_fifo, TFifo_construct)
	{
		{
			TFifo<byte_t> fifo(TFiber::Self(), TFiber::Self());
		}

		{
			TFifo<TUTF32> fifo(TFiber::Self(), TFiber::Self());
		}
	}

	TEST(io_stream_fifo, TFifo_loopback_simple)
	{
		TFifo<char, 16U> fifo(TFiber::Self(), TFiber::Self());
		EXPECT_EQ(fifo.Space(), 16U);
		EXPECT_EQ(fifo.Remaining(), 0U);

		EXPECT_EQ(fifo.Write("hello world!\n", 13U), 13U);
		EXPECT_EQ(fifo.Space(), 3U);
		EXPECT_EQ(fifo.Remaining(), 13U);

		char rx[16] = {};
		EXPECT_EQ(fifo.Read(rx, sizeof(rx)), 13U);
		EXPECT_TRUE(strncmp(rx, "hello world!\n", 13U) == 0);
	}

	TEST(io_stream_fifo, TFifo_loopback_multi)
	{
		TFifo<char, 16U> fifo(TFiber::Self(), TFiber::Self());
		EXPECT_EQ(fifo.Space(), 16U);
		EXPECT_EQ(fifo.Remaining(), 0U);

		EXPECT_EQ(fifo.Write("hello world!\n", 13U), 13U);
		EXPECT_EQ(fifo.Space(), 3U);
		EXPECT_EQ(fifo.Remaining(), 13U);

		EXPECT_EQ(fifo.Write("foobar!\n", 8U), 3U);
		EXPECT_EQ(fifo.Space(), 0U);
		EXPECT_EQ(fifo.Remaining(), 16U);

		EXPECT_EQ(fifo.Write("test!\n", 7U), 0U);
		EXPECT_EQ(fifo.Space(), 0U);
		EXPECT_EQ(fifo.Remaining(), 16U);

		char rx[16] = {};
		EXPECT_EQ(fifo.Read(rx, 13U), 13U);
		EXPECT_TRUE(strncmp(rx, "hello world!\n", 13U) == 0);
		EXPECT_EQ(fifo.Space(), 13U);
		EXPECT_EQ(fifo.Remaining(), 3U);

		EXPECT_EQ(fifo.Write("bar!\n", 5U), 5U);
		EXPECT_EQ(fifo.Space(), 8U);
		EXPECT_EQ(fifo.Remaining(), 8U);

		EXPECT_EQ(fifo.Read(rx, 8U), 8U);
		EXPECT_TRUE(strncmp(rx, "foobar!\n", 8U) == 0);
		EXPECT_EQ(fifo.Space(), 16U);
		EXPECT_EQ(fifo.Remaining(), 0U);
	}

	TEST(io_stream_fifo, TFifo_producer_consumer)
	{
		TFiber fib_producer;
		TFifo<byte_t, 173+1> fifo(&fib_producer, TFiber::Self());
		const u32_t n_tx = 9973;

		fib_producer.Start([&](){
			for(u32_t i = 0; i < n_tx;)
			{
				byte_t buffer[17];
				const u32_t n = el1::util::Min<u32_t>(sizeof(buffer), n_tx - i);
				for(u32_t j = 0; j < n; j++)
					buffer[j] = (byte_t)((i+j) % 256);

				fifo.WriteAll(buffer, n);
				i += n;
			}
		});

		byte_t buffer[n_tx + 1];
		EXPECT_EQ(fifo.BlockingRead(buffer, n_tx + 1), n_tx);

		for(u32_t i = 0; i < n_tx; i++)
			EXPECT_EQ(buffer[i], ((byte_t)i % 256));
	}
}
