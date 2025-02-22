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
					while(fifo->WriteOut(*sink, fifo->Remaining()));

					auto w_fifo_ready = fifo->OnInputReady();
					if(w_fifo_ready == nullptr)
					{
						while(fifo->WriteOut(*sink, fifo->Remaining()));
						break;
					}
					w_fifo_ready->WaitFor();

					auto w_sink_ready = sink->OnOutputReady();
					if(w_sink_ready == nullptr)
					{
						while(fifo->WriteOut(*sink, fifo->Remaining()));
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
	class TPullBuffer
	{
		protected:
			ISource<T>* source;
			usys_t offset;
			io::collection::list::TList<T> buffer;

			void Extend(usys_t index)
			{
				io::collection::list::TListSink sink(&buffer);
				while(source && index >= buffer.Count())
				{
					for(;;)
					{
						if(sink.ReadIn(*source))
							break;
						auto w = source->OnInputReady();
						if(!w)
						{
							// eof
							source = nullptr;
							break;
						}
						w->WaitFor();
					}
				}
			}

		public:
			T* operator[](usys_t index)
			{
				EL_ERROR(index < offset, TLogicException);
				index -= offset;

				Extend(index);

				if(index >= buffer.Count())
					return nullptr;

				return &buffer[index];
			}

			void Discard(const usys_t n_discard)
			{
				EL_ERROR(n_discard > buffer.Count(), TLogicException);
				offset += n_discard;
				buffer.Remove(0, n_discard);
			}

			void ResetOffset()
			{
				offset = 0;
			}

			constexpr TPullBuffer(ISource<T>* source) : source(source), offset(0), buffer() {}
	};
}
