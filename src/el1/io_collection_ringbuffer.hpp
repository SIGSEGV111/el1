#pragma once

#include "def.hpp"
#include "error.hpp"
#include "system_task.hpp"
#include "system_waitable.hpp"
#include "util.hpp"
#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>

namespace el1::io::collection::ringbuffer
{
	using namespace io::types;
	using namespace io::text::string;
	using namespace system::task;
	using namespace system::waitable;

	struct TRingBufferOverrunException : error::IException
	{
		const usys_t dropped_samples;

		TString Message() const final override
		{
			return TString::Format("ring buffer consumer was overrun by producer; %d samples were dropped", dropped_samples);
		}

		error::IException* Clone() const final override
		{
			return new TRingBufferOverrunException(*this);
		}

		TRingBufferOverrunException(const usys_t dropped_samples) : dropped_samples(dropped_samples)
		{
		}
	};

	template<typename T>
	class TRingBufferConsumer;

	// Single-producer/multi-consumer overwriting ring buffer.
	//
	// The producer never waits for consumers. Each consumer owns an independent
	// read position. A slow consumer either skips overwritten records or throws
	// TRingBufferOverrunException when Read(..., true) is used.
	//
	// Slot generations let consumers detect whether the producer wrapped while a
	// record was copied. T must be safe to transfer as raw bytes. A concurrent
	// overwrite may overlap the memcpy; the second generation check rejects that
	// copy. This deliberate lock-free trade-off keeps the producer non-blocking.
	template<typename T>
	class TRingBufferProducer
	{
		friend class TRingBufferConsumer<T>;

		static_assert(std::is_trivially_copyable_v<T>, "ring buffer records must be trivially copyable");
		static_assert(std::is_trivially_move_constructible_v<T>, "ring buffer records must be trivially move constructible");
		static_assert(std::is_trivially_move_assignable_v<T>, "ring buffer records must be trivially move assignable");

		private:
			using position_t = u32_t;
			static constexpr position_t INVALID_GENERATION = std::numeric_limits<position_t>::max();

			struct TSlot
			{
				std::atomic<position_t> generation;
				alignas(T) byte_t storage[sizeof(T)];

				TSlot() : generation(INVALID_GENERATION)
				{
				}
			};

			enum class ERegistrationState : u8_t
			{
				INACTIVE,
				RESERVED,
				ACTIVE,
			};

			struct TConsumerRegistration
			{
				TSignal data_available;
				std::atomic<ERegistrationState> state;
				TConsumerRegistration* next;

				TConsumerRegistration(TConsumerRegistration* const next) :
					data_available(),
					state(ERegistrationState::RESERVED),
					next(next)
				{
				}
			};

			const position_t capacity;
			std::unique_ptr<TSlot[]> slots;
			std::atomic<position_t> write_position;
			std::atomic<TConsumerRegistration*> consumer_registrations;

			TConsumerRegistration* RegisterConsumer()
			{
				for(TConsumerRegistration* registration = consumer_registrations.load(std::memory_order_acquire); registration != nullptr; registration = registration->next)
				{
					ERegistrationState expected = ERegistrationState::INACTIVE;
					if(registration->state.compare_exchange_strong(expected, ERegistrationState::RESERVED, std::memory_order_acq_rel))
					{
						registration->data_available.Reset();
						registration->state.store(ERegistrationState::ACTIVE, std::memory_order_release);
						return registration;
					}
				}

				TConsumerRegistration* head = consumer_registrations.load(std::memory_order_acquire);
				auto* const registration = new TConsumerRegistration(head);
				while(!consumer_registrations.compare_exchange_weak(head, registration, std::memory_order_release, std::memory_order_acquire))
					registration->next = head;

				registration->state.store(ERegistrationState::ACTIVE, std::memory_order_release);
				return registration;
			}

			void NotifyConsumers()
			{
				for(TConsumerRegistration* registration = consumer_registrations.load(std::memory_order_acquire); registration != nullptr; registration = registration->next)
					if(registration->state.load(std::memory_order_acquire) == ERegistrationState::ACTIVE)
						registration->data_available.Raise();
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

				std::memcpy(slot.storage, &value, sizeof(T));

				// Publishing the generation makes the complete record visible. Publishing
				// write_position afterwards makes the new record visible to consumers.
				slot.generation.store(position, std::memory_order_release);
				write_position.store(position + 1, std::memory_order_release);
				NotifyConsumers();
			}

			usys_t Capacity() const EL_GETTER
			{
				return capacity;
			}

			TRingBufferProducer(const usys_t requested_capacity) :
				capacity(static_cast<position_t>(requested_capacity)),
				slots(requested_capacity > 0 ? std::make_unique<TSlot[]>(requested_capacity) : nullptr),
				write_position(0),
				consumer_registrations(nullptr)
			{
				EL_ERROR(requested_capacity == 0, error::TInvalidArgumentException, "capacity", "ring buffer capacity must be greater than zero");
				EL_ERROR(requested_capacity > std::numeric_limits<position_t>::max() / 2U, error::TInvalidArgumentException, "capacity", "ring buffer capacity is too large for generation arithmetic");
			}

			~TRingBufferProducer()
			{
				TConsumerRegistration* registration = consumer_registrations.load(std::memory_order_acquire);
				while(registration != nullptr)
				{
					EL_WARN(registration->state.load(std::memory_order_acquire) != ERegistrationState::INACTIVE, error::TException, "ring buffer producer destroyed while consumers were still registered");
					TConsumerRegistration* const next = registration->next;
					delete registration;
					registration = next;
				}
			}
	};

	template<typename T>
	class TRingBufferConsumer
	{
		static_assert(std::is_trivially_copyable_v<T>, "ring buffer records must be trivially copyable");
		static_assert(std::is_trivially_move_constructible_v<T>, "ring buffer records must be trivially move constructible");
		static_assert(std::is_trivially_move_assignable_v<T>, "ring buffer records must be trivially move assignable");

		private:
			using TProducer = TRingBufferProducer<T>;
			using position_t = typename TProducer::position_t;

			class TDataAvailableWaitable final : public IWaitable
			{
				private:
					TRingBufferConsumer* const consumer;

				public:
					bool IsReady() const final override
					{
						return consumer->Available() != 0 || consumer->registration->data_available.IsReady();
					}

					void Reset() const final override
					{
						consumer->registration->data_available.Reset();
					}

					const THandleWaitable* HandleWaitable() const final override
					{
						return consumer->registration->data_available.HandleWaitable();
					}

					TDataAvailableWaitable(TRingBufferConsumer* const consumer) : consumer(consumer)
					{
					}
			};

			TProducer* const producer;
			position_t read_position;
			typename TProducer::TConsumerRegistration* const registration;
			usys_t dropped_samples;
			TDataAvailableWaitable data_available;

			void HandleOverrun(const position_t observed_write_position, const bool throw_on_overrun)
			{
				const position_t available = observed_write_position - read_position;
				if(available <= producer->capacity)
					return;

				const position_t n_dropped = available - producer->capacity;
				read_position += n_dropped;
				dropped_samples += n_dropped;

				if(throw_on_overrun)
					EL_THROW(TRingBufferOverrunException, static_cast<usys_t>(n_dropped));
			}

		public:
			TRingBufferConsumer(const TRingBufferConsumer&) = delete;
			TRingBufferConsumer(TRingBufferConsumer&&) = delete;
			TRingBufferConsumer& operator=(const TRingBufferConsumer&) = delete;
			TRingBufferConsumer& operator=(TRingBufferConsumer&&) = delete;

			bool Read(T& value, const bool throw_on_overrun = false)
			{
				for(;;)
				{
					position_t observed_write_position = producer->write_position.load(std::memory_order_acquire);
					if(observed_write_position == read_position)
						return false;

					HandleOverrun(observed_write_position, throw_on_overrun);

					const position_t expected_generation = read_position;
					typename TProducer::TSlot& slot = producer->slots[expected_generation % producer->capacity];
					const position_t generation_before = slot.generation.load(std::memory_order_acquire);

					if(generation_before != expected_generation)
					{
						HandleOverrun(producer->write_position.load(std::memory_order_acquire), throw_on_overrun);
						continue;
					}

					std::memcpy(&value, slot.storage, sizeof(T));
					std::atomic_thread_fence(std::memory_order_acquire);

					const position_t generation_after = slot.generation.load(std::memory_order_acquire);
					observed_write_position = producer->write_position.load(std::memory_order_acquire);

					if(generation_after != expected_generation || observed_write_position - expected_generation > producer->capacity)
					{
						HandleOverrun(observed_write_position, throw_on_overrun);
						continue;
					}

					read_position = expected_generation + 1;
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
				registration(producer.RegisterConsumer()),
				dropped_samples(0),
				data_available(this)
			{
			}

			~TRingBufferConsumer()
			{
				registration->state.store(TProducer::ERegistrationState::INACTIVE, std::memory_order_release);
			}
	};
}
