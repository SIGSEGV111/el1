#pragma once

#include "io_stream.hpp"
#include "io_stream_fifo.hpp"
#include "io_collection_list.hpp"
#include "system_waitable.hpp"
#include "system_task.hpp"
#include "util.hpp"

namespace el1::io::stream::buffer
{
	template<typename T>
	class TWriteThroughBuffer : public ISink<T>
	{
		protected:
			using TFifo = fifo::TFifo<T, util::Max<u32_t>(2, 4U*4096U / sizeof(T))>;
			ISink<T>* const sink;
			std::unique_ptr<TFifo> fifo;
			system::task::TFiber fib_flusher;

			void FlusherMain()
			{
				for(;;)
				{
					while(fifo->WriteOut(*sink, fifo->Count()));

					auto w_fifo_ready = fifo->OnInputReady();
					if(w_fifo_ready == nullptr)
					{
						while(fifo->WriteOut(*sink, fifo->Count()));
						break;
					}
					w_fifo_ready->WaitFor();

					auto w_sink_ready = sink->OnOutputReady();
					if(w_sink_ready == nullptr)
					{
						while(fifo->WriteOut(*sink, fifo->Count()));
						break;
					}
					w_sink_ready->WaitFor();
				}
			}

		public:
			usys_t Write(const T* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT
			{
				return fifo->Write(arr_items, n_items_max);
			}

			const system::waitable::IWaitable* OnOutputReady() const final override
			{
				return fifo->OnOutputReady();
			}

			TWriteThroughBuffer(ISink<T>* const sink) : sink(sink)
			{
				fifo = New<TFifo>(system::task::TFiber::Self(), &fib_flusher);
				fib_flusher.Start(util::function::TFunction(this, &TWriteThroughBuffer<T>::FlusherMain));
			}
	};

	template<typename T>
	class EL_LIFETIME_POINTER TPullBuffer : public IBufferedSource<T>
	{
		protected:
			ISource<T>* source;
			io::collection::list::TList<T> buffer;
			usys_t pos = 0;

		public:
			usys_t Count() const noexcept final override
			{
				return buffer.Count() - pos;
			}

			bool Ensure(const usys_t count) final override
			{
				if(count <= Count())
					return true;
				if(pos != 0)
				{
					buffer.Remove(0, pos);
					pos = 0;
				}

				io::collection::list::TListSink sink(&buffer);
				while(source != nullptr && buffer.Count() < count)
				{
					if(sink.ReadIn(*source) != 0)
						continue;

					const auto* const waitable = source->OnInputReady();
					if(waitable == nullptr)
					{
						source = nullptr;
						break;
					}
					waitable->WaitFor();
				}
				return buffer.Count() >= count;
			}

			const T& operator[](const usys_t index) const final override
			{
				EL_ERROR(index >= Count(), TStreamDryException);
				return buffer[pos + index];
			}

			io::collection::array::array_t<const T> Head() const noexcept EL_LIFETIME_BOUND final override
			{
				const usys_t count = Count();
				return io::collection::array::array_t<const T>::FromUnsafePointer(count == 0 ? nullptr : buffer.ItemPtr(pos), count);
			}

			void Shift(const usys_t count) final override
			{
				EL_ERROR(count > Count(), TStreamDryException);
				pos += count;
			}

			const system::waitable::IWaitable* OnInputReady() const final override
			{
				return Count() != 0 || source == nullptr ? nullptr : source->OnInputReady();
			}

			explicit constexpr TPullBuffer(ISource<T>* source EL_LIFETIME_BOUND) : source(source), buffer() {}
	};

}
