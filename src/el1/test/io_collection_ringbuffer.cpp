#include <gtest/gtest.h>
#include <el1/io_collection_ringbuffer.hpp>
#include <el1/system_task.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::collection::ringbuffer;
	using namespace el1::system::task;

	struct TRecord
	{
		u32_t sequence;
		u32_t inverse;
	};

	static_assert(std::is_trivially_copyable_v<TRecord>);
	static_assert(std::is_trivially_move_constructible_v<TRecord>);
	static_assert(std::is_trivially_move_assignable_v<TRecord>);

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

	TEST(io_collection_ringbuffer, consumer_can_throw_on_overrun_and_continue)
	{
		TRingBufferProducer<int> producer(4);
		TRingBufferConsumer<int> consumer(producer);

		for(int i = 0; i < 10; i++)
			producer.Write(i);

		int value = 0;
		try
		{
			consumer.Read(value, true);
			FAIL() << "expected TRingBufferOverrunException";
		}
		catch(const TRingBufferOverrunException& exception)
		{
			EXPECT_EQ(exception.dropped_samples, 6U);
		}

		EXPECT_EQ(consumer.DroppedSamples(), 6U);
		ASSERT_TRUE(consumer.Read(value, true));
		EXPECT_EQ(value, 6);
	}

	TEST(io_collection_ringbuffer, multiple_consumers_keep_independent_read_positions)
	{
		TRingBufferProducer<TRecord> producer(8);
		TRingBufferConsumer<TRecord> first(producer);
		TRingBufferConsumer<TRecord> second(producer);

		producer.Write({ 7, ~7U });

		EXPECT_TRUE(first.OnDataAvailable().IsReady());
		EXPECT_TRUE(second.OnDataAvailable().IsReady());

		TRecord value {};
		ASSERT_TRUE(first.Read(value));
		EXPECT_EQ(value.sequence, 7U);
		EXPECT_EQ(value.inverse, ~7U);

		ASSERT_TRUE(second.Read(value));
		EXPECT_EQ(value.sequence, 7U);
		EXPECT_EQ(value.inverse, ~7U);
	}

	TEST(io_collection_ringbuffer, one_write_wakes_multiple_consumer_fibers)
	{
		TRingBufferProducer<int> producer(8);
		TRingBufferConsumer<int> first(producer);
		TRingBufferConsumer<int> second(producer);
		int first_value = 0;
		int second_value = 0;

		TFiber first_reader([&]()
		{
			first.OnDataAvailable().WaitFor();
			EXPECT_TRUE(first.Read(first_value));
		});

		TFiber second_reader([&]()
		{
			second.OnDataAvailable().WaitFor();
			EXPECT_TRUE(second.Read(second_value));
		});

		producer.Write(23);

		EXPECT_EQ(first_reader.Join(), nullptr);
		EXPECT_EQ(second_reader.Join(), nullptr);
		EXPECT_EQ(first_value, 23);
		EXPECT_EQ(second_value, 23);
	}
	TEST(io_collection_ringbuffer, one_write_wakes_consumers_on_different_threads)
	{
		TRingBufferProducer<int> producer(8);
		TRingBufferConsumer<int> first(producer);
		TRingBufferConsumer<int> second(producer);
		std::atomic<int> first_value(0);
		std::atomic<int> second_value(0);

		TThread first_reader(
			"ring-reader-1",
			[&]()
			{
				first.OnDataAvailable().WaitFor();
				int value = 0;
				if(first.Read(value))
					first_value.store(value, std::memory_order_release);
			}
		);

		TThread second_reader(
			"ring-reader-2",
			[&]()
			{
				second.OnDataAvailable().WaitFor();
				int value = 0;
				if(second.Read(value))
					second_value.store(value, std::memory_order_release);
			}
		);

		producer.Write(31);

		EXPECT_EQ(first_reader.Join(), nullptr);
		EXPECT_EQ(second_reader.Join(), nullptr);
		EXPECT_EQ(first_value.load(std::memory_order_acquire), 31);
		EXPECT_EQ(second_value.load(std::memory_order_acquire), 31);
	}

}
