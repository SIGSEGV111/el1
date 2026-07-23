#include <gtest/gtest.h>
#include <el1/io_collection_ringbuffer.hpp>
#include <el1/system_task.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::collection::ringbuffer;
	using namespace el1::system::task;

	TEST(io_collection_ringbuffer, consumer_receives_data_and_waitable_wakes)
	{
		TRingBufferProducer<int> producer(8);
		TRingBufferConsumer<int> consumer(producer);

		TThread writer(
			"ring-writer",
			[&]()
			{
				TFiber::Sleep(0.01);
				producer.Write(42);
			}
		);

		consumer.OnDataAvailable().WaitFor();
		int value = 0;
		EXPECT_TRUE(consumer.Read(value));
		EXPECT_EQ(value, 42);
		EXPECT_FALSE(consumer.Read(value));

		auto exception = writer.Join();
		EXPECT_EQ(exception, nullptr);
	}

	TEST(io_collection_ringbuffer, slow_consumer_skips_overwritten_samples)
	{
		TRingBufferProducer<int> producer(4);
		TRingBufferConsumer<int> consumer(producer);

		for(int i = 0; i < 10; i++)
			producer.Write(i);

		int value = 0;
		for(int expected = 6; expected < 10; expected++)
		{
			ASSERT_TRUE(consumer.Read(value));
			EXPECT_EQ(value, expected);
		}

		EXPECT_FALSE(consumer.Read(value));
		EXPECT_EQ(consumer.DroppedSamples(), 6U);
	}
}
