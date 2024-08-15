#pragma once

#include "io_stream.hpp"
#include "io_stream_fifo.hpp"
#include "util_function.hpp"
#include "system_task.hpp"

namespace el1::io::stream::producer
{
	template<typename T>
	class TProducerPipe : public IPipe<TProducerPipe<T>, T>
	{
		public:
			using TProducerFunction = util::function::TFunction<void, ISink<T>&>;
			using TOut = T;

		protected:
			TProducerFunction producer;
			system::task::TFiber fiber;
			fifo::TFifo<T> fifo;
			collection::list::array_t<T> peek_buffer;
			usys_t idx_peek;

			void FiberMain()
			{
				producer(fifo);
			}

		public:
			const TOut* NextItem()
			{
				if(idx_peek >= peek_buffer.Count())
				{
					fifo.Discard(idx_peek);
					peek_buffer = fifo.Peek();
					while(peek_buffer.Count() == 0)
					{
						const auto* waitable = fifo.OnInputReady();
						if(waitable == nullptr)
							return nullptr;

						waitable->WaitFor();
						peek_buffer = fifo.Peek();
					}
					idx_peek = 0;
				}

				return peek_buffer.ItemPtr(idx_peek++);
			}

			TProducerPipe(TProducerFunction producer) :
				producer(producer),
				fifo(&fiber, system::task::TFiber::Self()),
				peek_buffer(nullptr, 0),
				idx_peek(0)
			{
				fiber.Start(util::function::TFunction<void>(this, &TProducerPipe::FiberMain));
			}
	};

	template<typename T>
	TProducerPipe<T> Produce(util::function::TFunction<void, ISink<T>&> producer)
	{
		return TProducerPipe<T>(producer);
	}

	template<typename T>
	using FProducer = util::function::TFunction<void, ISink<T>&>;
}
