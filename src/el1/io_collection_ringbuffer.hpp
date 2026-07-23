#pragma once

#include "def.hpp"
#include "error.hpp"
#include "io_collection_list.hpp"
#include "system_handle.hpp"
#include "system_task.hpp"
#include "system_waitable.hpp"
#include "util.hpp"
#include <atomic>
#include <memory>

#ifdef EL_OS_LINUX
#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif

namespace el1::io::collection::ringbuffer
{
	using namespace io::collection::list;
	using namespace io::types;
	using namespace system::handle;
	using namespace system::task;
	using namespace system::waitable;

	template<typename T>
	class TRingBufferConsumer;

	// Single-producer/multi-consumer ring buffer.
	//
	// The producer never waits for consumers. Slow consumers automatically skip samples
	// that have already been overwritten. Every consumer owns an independent read position
	// and a kernel waitable that can wake fibers when the producer publishes new data.
	template<typename T>
	class TRingBufferProducer
	{
		friend class TRingBufferConsumer<T>;

		private:
			using position_t = u32_t;

			struct TSlot
			{
				std::atomic_flag lock = ATOMIC_FLAG_INIT;
				position_t position = static_cast<position_t>(NEG1);
				T value {};

				void Lock()
				{
					while(lock.test_and_set(std::memory_order_acquire))
					{
					}
				}

				void Unlock()
				{
					lock.clear(std::memory_order_release);
				}
			};

			const usys_t capacity;
			std::unique_ptr<TSlot[]> slots;
			std::atomic<position_t> write_position;
			mutable TSimpleMutex consumers_mutex;
			TList<TRingBufferConsumer<T>*> consumers;

			void RegisterConsumer(TRingBufferConsumer<T>* const consumer)
			{
				const TMutexAutoLock lock(&consumers_mutex);
				consumers.Append(consumer);
			}

			void UnregisterConsumer(TRingBufferConsumer<T>* const consumer)
			{
				const TMutexAutoLock lock(&consumers_mutex);
				EL_ERROR(consumers.RemoveItem(consumer) != 1, TLogicException);
			}

			void NotifyConsumers()
			{
				const TMutexAutoLock lock(&consumers_mutex);
				for(TRingBufferConsumer<T>* const consumer : consumers)
					consumer->Notify();
			}

		public:
			TRingBufferProducer(const TRingBufferProducer&) = delete;
			TRingBufferProducer(TRingBufferProducer&&) = delete;
			TRingBufferProducer& operator=(const TRingBufferProducer&) = delete;
			TRingBufferProducer& operator=(TRingBufferProducer&&) = delete;

			void Write(const T& value)
			{
				const position_t position = write_position.load(std::memory_order_relaxed);
				TSlot& slot = slots[position % capacity];

				slot.Lock();
				try
				{
					slot.value = value;
					slot.position = position;
				}
				catch(...)
				{
					slot.Unlock();
					throw;
				}
				slot.Unlock();

				// Publish the fully written slot. On cache-coherent SMP systems this release
				// fence/store pair is the required cache visibility barrier; an explicit CPU
				// cache flush is neither necessary nor desirable.
				std::atomic_thread_fence(std::memory_order_release);
				write_position.store(position + 1, std::memory_order_release);
				NotifyConsumers();
			}

			usys_t Capacity() const EL_GETTER
			{
				return capacity;
			}

			TRingBufferProducer(const usys_t capacity) :
				capacity(capacity),
				slots(capacity > 0 ? std::make_unique<TSlot[]>(capacity) : nullptr),
				write_position(0),
				consumers_mutex(),
				consumers()
			{
				EL_ERROR(capacity == 0, TInvalidArgumentException, "capacity", "ring buffer capacity must be greater than zero");
			}

			~TRingBufferProducer()
			{
				const TMutexAutoLock lock(&consumers_mutex);
				EL_WARN(consumers.Count() != 0, TException, "ring buffer producer destroyed while consumers were still registered");
			}
	};

	template<typename T>
	class TRingBufferConsumer
	{
		friend class TRingBufferProducer<T>;

		private:
			using TProducer = TRingBufferProducer<T>;
			using position_t = typename TProducer::position_t;

			class TDataAvailableWaitable final : public IWaitable
			{
				private:
					TRingBufferConsumer* const consumer;
					THandleWaitable handle_waitable;

				public:
					bool IsReady() const final override
					{
						return consumer->Available() != 0 || handle_waitable.IsReady();
					}

					void Reset() const final override
					{
						handle_waitable.Reset();
						consumer->DrainNotification();
					}

					const THandleWaitable* HandleWaitable() const final override
					{
						return &handle_waitable;
					}

					TDataAvailableWaitable(TRingBufferConsumer* const consumer, const handle_t handle) :
						consumer(consumer),
						handle_waitable({ .read = true, .write = false, .other = false }, handle)
					{
					}
			};

			TProducer* const producer;
			position_t read_position;
			usys_t dropped_samples;
			THandle notification_handle;
			TDataAvailableWaitable data_available;

			void Notify()
			{
				#ifdef EL_OS_LINUX
					const u64_t value = 1;
					const ssize_t n = write(notification_handle, &value, sizeof(value));
					if(n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
						EL_THROW(TSyscallException, errno);
				#else
					#error "TRingBufferConsumer notification is not implemented on this platform"
				#endif
			}

			void DrainNotification() const
			{
				#ifdef EL_OS_LINUX
					u64_t value;
					for(;;)
					{
						const ssize_t n = read(notification_handle, &value, sizeof(value));
						if(n == sizeof(value))
							continue;
						if(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
							break;
						if(n < 0)
							EL_THROW(TSyscallException, errno);
						break;
					}
				#else
					#error "TRingBufferConsumer notification is not implemented on this platform"
				#endif
			}

		public:
			TRingBufferConsumer(const TRingBufferConsumer&) = delete;
			TRingBufferConsumer(TRingBufferConsumer&&) = delete;
			TRingBufferConsumer& operator=(const TRingBufferConsumer&) = delete;
			TRingBufferConsumer& operator=(TRingBufferConsumer&&) = delete;

			bool Read(T& value)
			{
				for(;;)
				{
					const position_t write_position = producer->write_position.load(std::memory_order_acquire);
					const position_t available = write_position - read_position;
					if(available == 0)
						return false;

					if(available > producer->capacity)
					{
						const position_t n_dropped = available - producer->capacity;
						read_position += n_dropped;
						dropped_samples += n_dropped;
					}

					typename TProducer::TSlot& slot = producer->slots[read_position % producer->capacity];
					slot.Lock();
					if(slot.position != read_position)
					{
						slot.Unlock();
						continue;
					}

					try
					{
						value = slot.value;
					}
					catch(...)
					{
						slot.Unlock();
						throw;
					}
					slot.Unlock();
					read_position++;
					return true;
				}
			}

			usys_t Available() const EL_GETTER
			{
				const position_t available = producer->write_position.load(std::memory_order_acquire) - read_position;
				return util::Min<usys_t>(available, producer->capacity);
			}

			usys_t DroppedSamples() const EL_GETTER
			{
				return dropped_samples;
			}

			const IWaitable& OnDataAvailable() const EL_GETTER
			{
				return data_available;
			}

			TRingBufferConsumer(TProducer& producer) :
				producer(&producer),
				read_position(producer.write_position.load(std::memory_order_acquire)),
				dropped_samples(0),
				#ifdef EL_OS_LINUX
					notification_handle(EL_SYSERR(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)), true),
				#else
					notification_handle(),
				#endif
				data_available(this, notification_handle)
			{
				producer.RegisterConsumer(this);
			}

			~TRingBufferConsumer()
			{
				producer->UnregisterConsumer(this);
			}
	};
}
