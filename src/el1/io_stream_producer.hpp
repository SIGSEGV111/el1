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
			collection::list::array_t<T> head_buffer;
			usys_t idx_head;

			void FiberMain()
			{
				producer(fifo);
			}

		public:
			TOut* NextItem()
			{
				if(idx_head >= head_buffer.Count())
				{
					fifo.Shift(idx_head);
					head_buffer = fifo.HeadMutable();
					while(head_buffer.Count() == 0)
					{
						const auto* waitable = fifo.OnInputReady();
						if(waitable == nullptr)
							return nullptr;

						waitable->WaitFor();
						head_buffer = fifo.HeadMutable();
					}
					idx_head = 0;
				}

				return head_buffer.ItemPtr(idx_head++);
			}

			TProducerPipe(TProducerFunction producer) :
				producer(producer),
				fifo(&fiber, system::task::TFiber::Self()),
				head_buffer(),
				idx_head(0)
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
