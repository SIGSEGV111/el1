#pragma once

#include "io_stream.hpp"
#include "system_waitable.hpp"
#include "system_task.hpp"
#include "util.hpp"

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
				bool IsReady() const final override { return fifo->Remaining() > 0 || fifo->fib_producer == nullptr || !fifo->fib_producer->IsAlive(); }
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

		public:
			// returns an array of items ready to be read from the fifo
			// this function does not mark these items as read and thus does not free up space for the producer
			// use Discard() or Read() for this purpose
			collection::list::array_t<T> Peek() final override
			{
				if(Remaining() == 0 && fib_consumer != fib_producer && fib_producer != nullptr && fib_consumer != nullptr && fib_producer->IsAlive())
					fib_producer->SwitchTo();

				if(idx_read <= idx_write)
					return collection::list::array_t<T>(arr_items_fifo + idx_read, idx_write - idx_read);
				else
					return collection::list::array_t<T>(arr_items_fifo + idx_read, N_ITEMS - idx_read);
			}

			// discards the next n items from the fifo
			// essentially freeing the space for new items
			void Discard(const usys_t n_discard) final override
			{
				const usys_t n_remaining = Remaining();
				EL_ERROR(n_discard > n_remaining, TInvalidArgumentException, "n_discard", "n_discard cannot be larger than Remaining()");

				if(n_discard > 0 && idx_write == N_ITEMS)
					idx_write = idx_read;

				idx_read += n_discard;
				if(idx_read >= N_ITEMS)
					idx_read -= N_ITEMS;
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

			iosize_t WriteOut(ISink<T>& sink, const iosize_t _n_items_max, const bool) final override
			{
				iosize_t n_read = 0;
				const usys_t n_items_max = util::Min<usys_t>(Remaining(), _n_items_max);

				while(n_read < n_items_max)
				{
					const usys_t n_batch_max = idx_read < idx_write ? idx_write - idx_read : N_ITEMS - idx_read;
					const usys_t n_remaining = n_items_max - n_read;
					const usys_t n_now = util::Min(n_batch_max, n_remaining);
					const usys_t n_written = sink.Write(arr_items_fifo + idx_read, n_now);

					EL_ERROR(n_written > n_now, TLogicException);
					n_read += n_written;
					idx_read += n_written;
					if(idx_read >= N_ITEMS)
						idx_read = 0;

					if(n_written == 0)
					{
						auto w = sink.OnOutputReady();
						if(w == nullptr)
							break;
						w->WaitFor();
					}
				}

				return n_read;
			}

			usys_t Read(T* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT
			{
				usys_t n_read = 0;

				while(n_read < n_items_max && Remaining() > 0)
				{
					const usys_t r = util::Min<usys_t>(Remaining(), n_items_max - n_read);

					if(r > 0 && idx_write == N_ITEMS)
						idx_write = idx_read;

					for(usys_t i = 0; i < r; i++)
					{
						arr_items[n_read + i] = arr_items_fifo[idx_read];
						idx_read++;
						if(idx_read >= N_ITEMS)
							idx_read = 0;
					}

					n_read += r;

					if(n_read < n_items_max && fib_consumer != fib_producer && fib_producer != nullptr && fib_consumer != nullptr && fib_producer->IsAlive())
						fib_producer->SwitchTo();
				}

				return n_read;
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

			u32_t Remaining() const
			{
				if(idx_write == N_ITEMS)
					return N_ITEMS;
				return idx_write >= idx_read ? idx_write - idx_read : (N_ITEMS - idx_read) + idx_write;
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
