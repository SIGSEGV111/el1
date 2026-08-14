#pragma once

#include "io_stream.hpp"
#include "system_waitable.hpp"
#include "system_task.hpp"
#include "util.hpp"

#include <algorithm>

namespace el1::io::stream::fifo
{
	using namespace io::types;

	// this FIFO implementation allows two fibers to exchange data via the stream API
	// it needs to know the two fibers operating the FIFO in order to optimize switching
	// and to correctly monitor the fibers and thus provide waitables (if required)
	template<typename T, u32_t N_ITEMS = util::Max<u32_t>(2, 256U / sizeof(T))>
	class TFifo : public ISink<T>, public IBufferedSource<T>
	{
		protected:
			struct TInputWaitable : system::waitable::IWaitable
			{
				const TFifo* const fifo;
				bool IsReady() const final override { return fifo->Count() > 0 || fifo->fib_producer == nullptr || !fifo->fib_producer->IsAlive(); }
				TInputWaitable(const TFifo* const fifo) : fifo(fifo) {}
			};

			struct TOutputWaitable : system::waitable::IWaitable
			{
				const TFifo* const fifo;
				bool IsReady() const final override { return fifo->Space() > 0 || fifo->fib_consumer == nullptr || !fifo->fib_consumer->IsAlive(); }
				TOutputWaitable(const TFifo* const fifo) : fifo(fifo) {}
			};

			system::task::TFiber* fib_producer;
			system::task::TFiber* fib_consumer;
			const TOutputWaitable on_output_ready;
			const TInputWaitable on_input_ready;
			u32_t idx_write;
			u32_t idx_read;
			T arr_items_fifo[N_ITEMS];

		protected:
			void Linearize()
			{
				const usys_t count = Count();
				if(count == 0)
				{
					idx_read = 0;
					idx_write = 0;
					return;
				}
				if(idx_read == 0)
					return;

				std::rotate(arr_items_fifo, arr_items_fifo + idx_read, arr_items_fifo + N_ITEMS);
				idx_read = 0;
				idx_write = count == N_ITEMS ? N_ITEMS : (u32_t)count;
			}

		public:
			usys_t Count() const noexcept final override
			{
				if(idx_write == N_ITEMS)
					return N_ITEMS;
				return idx_write >= idx_read ? idx_write - idx_read : (N_ITEMS - idx_read) + idx_write;
			}

			bool Ensure(const usys_t count) final override
			{
				if(count > N_ITEMS)
					return false;

				while(Count() < count && fib_consumer != fib_producer && fib_producer != nullptr && fib_consumer != nullptr && fib_producer->IsAlive())
				{
					const usys_t before = Count();
					fib_producer->SwitchTo();
					if(Count() <= before)
						break;
				}

				if(Count() < count)
					return false;
				if(count != 0 && Head().Count() < Count())
					Linearize();
				return Head().Count() >= count;
			}

			const T& operator[](const usys_t index) const final override
			{
				EL_ERROR(index >= Count(), TStreamDryException);
				return arr_items_fifo[(idx_read + index) % N_ITEMS];
			}

			collection::array::array_t<const T> Head() const noexcept EL_LIFETIME_BOUND final override
			{
				const usys_t count = Count();
				if(count == 0)
					return {};
				const usys_t contiguous = util::Min<usys_t>(count, N_ITEMS - idx_read);
				return collection::array::array_t<const T>::FromUnsafePointer(arr_items_fifo + idx_read, contiguous);
			}

			collection::array::array_t<T> HeadMutable() EL_LIFETIME_BOUND
			{
				const usys_t count = Count();
				if(count == 0)
					return {};
				const usys_t contiguous = util::Min<usys_t>(count, N_ITEMS - idx_read);
				return collection::array::array_t<T>::FromUnsafePointer(arr_items_fifo + idx_read, contiguous);
			}

			void Shift(const usys_t count) final override
			{
				EL_ERROR(count > Count(), TInvalidArgumentException, "count", "count cannot be larger than Count()");
				if(count == 0)
					return;

				if(idx_write == N_ITEMS)
					idx_write = idx_read;
				idx_read = (idx_read + count) % N_ITEMS;
			}

			usys_t Write(const T* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT
			{
				usys_t n_written = 0;

				while(n_written < n_items_max && Space() > 0)
				{
					const usys_t w = util::Min<usys_t>(Space(), n_items_max - n_written);

					for(usys_t i = 0; i < w; i++)
					{
						arr_items_fifo[idx_write] = arr_items[n_written + i];
						idx_write++;
						if(idx_write >= N_ITEMS)
							idx_write = 0;
					}

					if(idx_write == idx_read)
						idx_write = N_ITEMS;

					n_written += w;

					if(n_written < n_items_max && fib_consumer != fib_producer && fib_producer != nullptr && fib_consumer != nullptr && fib_consumer->IsAlive())
						fib_consumer->SwitchTo();
				}

				return n_written;
			}

			iosize_t WriteOut(ISink<T>& sink, const iosize_t n_items_max, const bool) final override
			{
				const usys_t limit = n_items_max == (iosize_t)-1 ? Count() : util::Min<usys_t>(Count(), n_items_max);
				usys_t n_written_total = 0;
				while(n_written_total < limit)
				{
					const auto head = Head();
					const usys_t n_now = util::Min(head.Count(), limit - n_written_total);
					const usys_t n_written = sink.Write(head.Data(), n_now);
					EL_ERROR(n_written > n_now, TLogicException);
					Shift(n_written);
					n_written_total += n_written;

					if(n_written == 0)
					{
						const auto* const waitable = sink.OnOutputReady();
						if(waitable == nullptr)
							break;
						waitable->WaitFor();
					}
				}
				return n_written_total;
			}

			const system::waitable::IWaitable* OnOutputReady() const final override
			{
				return fib_consumer != nullptr && fib_consumer->IsAlive() ? &on_output_ready : nullptr;
			}

			const system::waitable::IWaitable* OnInputReady() const final override
			{
				return fib_producer != nullptr && fib_producer->IsAlive() ? &on_input_ready : nullptr;
			}

			u32_t Space() const
			{
				if(idx_write == N_ITEMS)
					return 0;
				return idx_write >= idx_read ? (N_ITEMS - idx_write) + idx_read : idx_read - idx_write;
			}

			void Close() final override
			{
				CloseInput();
				CloseOutput();
			}

			bool CloseInput() final override
			{
				fib_consumer = nullptr;
				return true;
			}

			bool CloseOutput() final override
			{
				fib_producer = nullptr;
				return true;
			}

			TFifo(system::task::TFiber* const fib_producer, system::task::TFiber* const fib_consumer) :
				fib_producer(fib_producer), fib_consumer(fib_consumer),
				on_output_ready(this),
				on_input_ready(this),
				idx_write(0), idx_read(0)
			{
			}

			TFifo() :
				fib_producer(system::task::TFiber::Self()),
				fib_consumer(system::task::TFiber::Self()),
				on_output_ready(this),
				on_input_ready(this),
				idx_write(0), idx_read(0)
			{
			}
	};
}
