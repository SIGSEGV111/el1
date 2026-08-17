#pragma once

#include "def.hpp"
#include "error.hpp"
#include "io_collection_list.hpp"
#include "io_stream.hpp"
#include "util.hpp"
#include <atomic>
#include <type_traits>

namespace el1::io::collection::ringbuffer
{
	using namespace io::types;
	using namespace io::collection::list;
	using namespace io::stream;

	// Single-writer/multi-reader overwriting ring buffer.
	//
	// The buffer deliberately does not synchronize individual slots. The writer
	// owns all writes and publishes completed writes by advancing write_position.
	// Readers own their read position independently. If a reader falls more than
	// Capacity() entries behind, the overwritten entries are skipped. A reader
	// checks write_position before and after copying a slot; if the producer wrapped
	// over that slot while it was being copied, the copy is discarded and retried.
	//
	// T must therefore be a cheap, trivially-copyable record type. This class is
	// intended for telemetry/sample streams where dropping old data is preferable
	// to blocking the producer.
	template<typename T>
	class TRingBuffer
	{
		static_assert(std::is_trivially_copyable_v<T>, "ring buffer records must be trivially copyable");
		static_assert(std::is_default_constructible_v<T>, "ring buffer records must be default constructible");
		static_assert(std::is_copy_constructible_v<T>, "ring buffer records must be copy constructible");
		static_assert(std::is_copy_assignable_v<T>, "ring buffer records must be copy assignable");

		public:
			using position_t = u32_t;
			static_assert(std::atomic<position_t>::is_always_lock_free, "ring buffer write position must be lock-free");

		private:
			static constexpr position_t WRITE_BUSY = 0x80000000U;
			static constexpr position_t POSITION_MASK = 0x7fffffffU;

			static position_t Distance(const position_t newer, const position_t older)
			{
				return (newer - older) & POSITION_MASK;
			}

		public:

			class TReader final : public ISource<T>
			{
				private:
					const TRingBuffer* buffer;
					position_t read_position;
					usys_t dropped_samples;

					void HandleOverrun(const position_t write_position)
					{
						const position_t available = TRingBuffer::Distance(write_position, read_position);
						if(available <= buffer->capacity)
							return;

						const position_t dropped = available - buffer->capacity;
						read_position = (read_position + dropped) & TRingBuffer::POSITION_MASK;
						dropped_samples += dropped;
					}

				public:
					TReader(const TReader&) = default;
					TReader(TReader&&) = default;
					TReader& operator=(const TReader&) = default;
					TReader& operator=(TReader&&) = default;

					usys_t Read(T* const arr_items, const usys_t n_items_max) final override
					{
						usys_t n_read = 0;
						while(n_read < n_items_max)
						{
							const position_t counter_before = buffer->write_position.load(std::memory_order_acquire);
							if((counter_before & TRingBuffer::WRITE_BUSY) != 0)
								break;

							const position_t write_before = counter_before & TRingBuffer::POSITION_MASK;
							HandleOverrun(write_before);
							if(read_position == write_before)
								break;

							const position_t position = read_position;
							arr_items[n_read] = buffer->items[position % buffer->capacity];
							std::atomic_thread_fence(std::memory_order_acquire);

							const position_t counter_after = buffer->write_position.load(std::memory_order_acquire);
							if(counter_after != counter_before)
							{
								if((counter_after & TRingBuffer::WRITE_BUSY) == 0)
									HandleOverrun(counter_after & TRingBuffer::POSITION_MASK);
								continue;
							}

							read_position = (position + 1) & TRingBuffer::POSITION_MASK;
							n_read++;
						}
						return n_read;
					}

					bool Read(T& value)
					{
						return Read(&value, 1) == 1;
					}

					usys_t Available() const EL_GETTER
					{
						const position_t counter = buffer->write_position.load(std::memory_order_acquire);
						if((counter & TRingBuffer::WRITE_BUSY) != 0)
							return 0;
						const position_t available = TRingBuffer::Distance(counter & TRingBuffer::POSITION_MASK, read_position);
						return util::Min<usys_t>(available, buffer->capacity);
					}

					usys_t DroppedSamples() const EL_GETTER
					{
						return dropped_samples;
					}

					explicit TReader(const TRingBuffer& buffer) :
						buffer(&buffer),
						read_position(buffer.write_position.load(std::memory_order_acquire) & TRingBuffer::POSITION_MASK),
						dropped_samples(0)
					{
					}
			};

		private:
			const position_t capacity;
			TList<T> items;
			std::atomic<position_t> write_position;

		public:
			TRingBuffer(const TRingBuffer&) = delete;
			TRingBuffer(TRingBuffer&&) = delete;
			TRingBuffer& operator=(const TRingBuffer&) = delete;
			TRingBuffer& operator=(TRingBuffer&&) = delete;

			void Write(const T& value)
			{
				const position_t position = write_position.load(std::memory_order_relaxed) & POSITION_MASK;
				write_position.store(position | WRITE_BUSY, std::memory_order_release);
				items[position % capacity] = value;
				write_position.store((position + 1) & POSITION_MASK, std::memory_order_release);
			}

			usys_t Capacity() const EL_GETTER
			{
				return capacity;
			}

			position_t WritePosition() const EL_GETTER
			{
				return write_position.load(std::memory_order_acquire) & POSITION_MASK;
			}

			TReader Reader() const
			{
				return TReader(*this);
			}

			explicit TRingBuffer(const usys_t requested_capacity) :
				capacity(static_cast<position_t>(requested_capacity)),
				items(requested_capacity),
				write_position(0)
			{
				EL_ERROR(requested_capacity == 0, error::TInvalidArgumentException, "capacity", "ring buffer capacity must be greater than zero");
				EL_ERROR(requested_capacity > POSITION_MASK / 2U, error::TInvalidArgumentException, "capacity", "ring buffer capacity is too large for write-position arithmetic");
				items.SetCount(requested_capacity);
			}
	};

	// Compatibility names for existing users. Consumers no longer register with
	// the producer; constructing a reader simply snapshots the current write position.
	template<typename T>
	using TRingBufferProducer = TRingBuffer<T>;

	template<typename T>
	using TRingBufferConsumer = typename TRingBuffer<T>::TReader;
}
