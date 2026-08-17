#include <gtest/gtest.h>
#include <el1/io_collection_ringbuffer.hpp>
#include <type_traits>
#include <atomic>
#include <thread>

using namespace ::testing;

namespace
{
	using namespace el1::io::collection::ringbuffer;

	struct TRecord
	{
		u32_t sequence;
		u32_t inverse;
	};

	static_assert(std::is_trivially_copyable_v<TRecord>);

	TEST(io_collection_ringbuffer, reader_receives_data)
	{
		TRingBuffer<int> buffer(8);
		auto reader = buffer.Reader();
		buffer.Write(42);

		int value = 0;
		EXPECT_TRUE(reader.Read(value));
		EXPECT_EQ(value, 42);
		EXPECT_FALSE(reader.Read(value));
	}

	TEST(io_collection_ringbuffer, reader_is_an_isource)
	{
		TRingBuffer<int> buffer(8);
		auto reader = buffer.Reader();
		el1::io::stream::ISource<int>& source = reader;

		buffer.Write(11);
		buffer.Write(12);
		int values[4] = {};
		EXPECT_EQ(source.Read(values, 4), 2U);
		EXPECT_EQ(values[0], 11);
		EXPECT_EQ(values[1], 12);
	}

	TEST(io_collection_ringbuffer, slow_reader_skips_overwritten_samples)
	{
		TRingBuffer<int> buffer(4);
		auto reader = buffer.Reader();

		for(int i = 0; i < 10; i++)
			buffer.Write(i);

		int value = 0;
		for(int expected = 6; expected < 10; expected++)
		{
			ASSERT_TRUE(reader.Read(value));
			EXPECT_EQ(value, expected);
		}

		EXPECT_FALSE(reader.Read(value));
		EXPECT_EQ(reader.DroppedSamples(), 6U);
	}

	TEST(io_collection_ringbuffer, readers_keep_independent_positions_without_registration)
	{
		TRingBuffer<TRecord> buffer(8);
		auto first = buffer.Reader();
		auto second = buffer.Reader();

		buffer.Write({ 7, ~7U });

		TRecord value {};
		ASSERT_TRUE(first.Read(value));
		EXPECT_EQ(value.sequence, 7U);
		EXPECT_EQ(value.inverse, ~7U);

		ASSERT_TRUE(second.Read(value));
		EXPECT_EQ(value.sequence, 7U);
		EXPECT_EQ(value.inverse, ~7U);
	}


	TEST(io_collection_ringbuffer, concurrent_reader_never_accepts_overwritten_record)
	{
		TRingBuffer<TRecord> buffer(64);
		auto reader = buffer.Reader();
		std::atomic<bool> writer_done(false);

		std::thread writer(
			[&]()
			{
				for(u32_t i = 1; i <= 250000; i++)
					buffer.Write({ i, ~i });
				writer_done.store(true, std::memory_order_release);
			}
		);

		TRecord value {};
		usys_t n_read = 0;
		while(!writer_done.load(std::memory_order_acquire) || reader.Available() != 0)
		{
			if(!reader.Read(value))
			{
				std::this_thread::yield();
				continue;
			}

			EXPECT_EQ(value.inverse, ~value.sequence);
			n_read++;
		}

		writer.join();
		EXPECT_GT(n_read, 0U);
	}

	TEST(io_collection_ringbuffer, new_reader_starts_at_current_write_position)
	{
		TRingBuffer<int> buffer(4);
		buffer.Write(1);
		buffer.Write(2);
		auto reader = buffer.Reader();

		int value = 0;
		EXPECT_FALSE(reader.Read(value));
		buffer.Write(3);
		ASSERT_TRUE(reader.Read(value));
		EXPECT_EQ(value, 3);
	}
}
