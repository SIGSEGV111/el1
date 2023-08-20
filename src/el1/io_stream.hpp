#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "error.hpp"
#include "util.hpp"
#include "system_waitable.hpp"
#include "system_handle.hpp"
#include "system_time.hpp"
#include <type_traits>

namespace el1::system::task
{
	class TFiber;
}

namespace el1::io::text::string
{
	class TString;
}

namespace el1::io::collection::list
{
	template<typename T>
	class TList;

	template<typename T>
	class array_t;
}

namespace io::file
{
	class TPath;
}

namespace el1::io::stream
{
	// streams only operate on trivially constructible, copyable and moveable datatypes
	using namespace types;

	struct TStreamDryException : error::IException
	{
		io::text::string::TString Message() const final override;
		error::IException* Clone() const override;
	};

	struct TSinkFloodedException : error::IException
	{
		io::text::string::TString Message() const final override;
		error::IException* Clone() const override;
	};

	template<typename T>
	class TSourcePipe;

	struct IStream;

	template<typename T>
	struct ISource;

	template<typename T>
	struct ISink;

	template<typename TPipe, typename L>
	class TFilterPipe;

	template<typename TPipe, typename L>
	class TMapPipe;

	template<typename TPipe, typename TTransformator>
	class TTransformPipe;

	template<typename ... TPipes>
	class TConcatPipe;

	template<typename TPipe>
	class TLimitPipe;

	template<typename T>
	iosize_t _Pump(ISource<T>& source, ISink<T>& sink, const iosize_t n_items_max, const bool blocking = false);

	template<typename T>
	iosize_t Pump(ISource<T>& source, ISink<T>& sink, const iosize_t n_items_max, const bool blocking = false);

	template<>
	iosize_t Pump<byte_t>(ISource<byte_t>& source, ISink<byte_t>& sink, const iosize_t n_items_max, const bool blocking);

	/**********************************************/

	template<typename T>
	struct IBufferedSource
	{
		// returns a preview of the available items in the source directly from its internal buffers
		// use Discard() or Read() to remove the items from the source when you no longer need them
		// this invalidates the previously returned buffer
		// and allows the source to retrieve or produce the next batch of items
		virtual collection::list::array_t<T> Peek() = 0;
	};

	struct IStream
	{
		virtual ~IStream() {}

		// if the stream is directly connected to an operating system object the stream can decide to to return the handle for the object
		virtual system::handle::handle_t Handle() { return system::handle::INVALID_HANDLE; }

		// closes the stream indicating that the consumer does not want to read or write any more data
		virtual void Close() {}
	};

	template<typename T>
	struct ISource : IStream
	{
		// Read() must only return 0 if the source is either dry or blocked
		// if OnInputReady() returns a waitable then the source is blocked and the caller should WaitFor()
		// if it returns nullptr, then the source is dry and will not produce new data any more

		// essentially you should call Read() repeatedly until it returns 0
		// once that happens check OnInputReady()
		// if it returns nullptr then your are done - the source is dry (has no more data now, will not have any more data in the future)
		// if it returns a waitable then use that to call WaitFor()
		// after the WaitFor() the source *should* have new data to be Read()
		// but if it doesn't then just repeat the process anyways
		// WaitFor() can return without actually waiting under some rare circumstances, but it will eventually actually wait
		virtual usys_t Read(T* const arr_items, const usys_t n_items_max) EL_WARN_UNUSED_RESULT = 0;
		virtual const system::waitable::IWaitable* OnInputReady() const { return nullptr; }

		// instructs the source to directly write the data to a sink - potentially avoiding to copy the data into a temporary buffer
		virtual iosize_t WriteOut(ISink<T>& sink, const iosize_t n_items_max = (iosize_t)-1, const bool allow_recursion = true);

		// similar to Read() but blocks until n_items_max have been read
		// it will read less items if the source runs dry or timeout expires
		// you can use OnInputReady() to check if the source ran dry
		virtual usys_t BlockingRead(T* const arr_items, const usys_t n_items_max, system::time::TTime timeout = -1, const bool absolute_time = false) EL_WARN_UNUSED_RESULT;

		// similar to BlockingRead() but blocks until exactly n_items have been read
		// throws an exception if the source fails to provide the requested amount of items
		virtual void ReadAll(T* const arr_items, const usys_t n_items);
		void ReadAll(io::collection::list::array_t<T> arr_items) { this->ReadAll(arr_items.ItemPtr(0), arr_items.Count()); }

		// closes only the input side of a full duplex connection - returns false if this is not possible
		virtual bool CloseInput() EL_WARN_UNUSED_RESULT { return false; }

		TSourcePipe<T> Pipe();
	};

	template<typename T>
	struct ISink : IStream
	{
		// see the description of ISource above - the ISink works just the same
		virtual usys_t Write(const T* const arr_items, const usys_t n_items_max) EL_WARN_UNUSED_RESULT = 0;
		virtual const system::waitable::IWaitable* OnOutputReady() const { return nullptr; }
		virtual iosize_t ReadIn(ISource<T>& source, const iosize_t n_items_max = (iosize_t)-1, const bool allow_recursion = true);
		usys_t BlockingWrite(const T* const arr_items, const usys_t n_items_max, system::time::TTime timeout = -1, const bool absolute_time = false) EL_WARN_UNUSED_RESULT;
		virtual void WriteAll(const T* const arr_items, const usys_t n_items);
		void WriteAll(io::collection::list::array_t<const T> arr_items) { this->WriteAll(arr_items.ItemPtr(0), arr_items.Count()); }

		virtual bool CloseOutput() EL_WARN_UNUSED_RESULT { return false; }
		virtual void Flush() {} // flushes any output buffers
	};

	template<typename TPipe, typename _TOut>
	struct IPipe : public ISource<_TOut>
	{
		using TOut = _TOut;

		// this function is called repeatedly to fetch the next item from the stream
		// once the pipe has run out of items it must return nullptr and continue to do so
		// the returned item pointer needs only to be valid until the next call
		virtual const TOut* NextItem() = 0;

		template<typename L>
		TFilterPipe<TPipe, L> Filter(L callable);

		template<typename L>
		TMapPipe<TPipe, L> Map(L callable);

		template<typename TOtherStream>
		TConcatPipe<TPipe, TOtherStream> Concat(TOtherStream* const other);

		template<typename TTransformator>
		TTransformPipe<TPipe, TTransformator> Transform(TTransformator&& transformator);

		template<typename TTransformator>
		TTransformPipe<TPipe, TTransformator&> Transform(TTransformator& transformator);

		TLimitPipe<TPipe> Limit(const iosize_t n_items_max);

		template<typename L>
		void ForEach(L callable);

		template<typename L, typename T = TOut>
		T Aggregate(L lambda, T init = T());

		iosize_t ToStream(ISink<TOut>& sink);
		usys_t AppendTo(collection::list::TList<TOut>& list);
		iosize_t Count();
		const TOut& First();
		io::collection::list::TList<TOut> Collect();

		usys_t Read(TOut* const arr_items, const usys_t n_items_max) override EL_WARN_UNUSED_RESULT;
	};

	template<typename T>
	class TCopyCatStream : public ISource<T>
	{
		protected:
			ISource<T>* const source;
			ISink<T>* const copy_dst;

		public:
			usys_t Read(T* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT
			{
				const usys_t r = source->Read(arr_items, n_items_max);
				copy_dst->WriteAll(arr_items, n_items_max);
				return r;
			}

			const system::waitable::IWaitable* OnInputReady() const final override
			{
				return source->OnInputReady();
			}

			TCopyCatStream(ISource<T>* const source, ISink<T>* const copy_dst) : source(source), copy_dst(copy_dst) {}
	};

	template<typename _TOut>
	struct TReinterpretCastTransformer
	{
		using TOut = _TOut;

		template<typename TSourceStream>
		const TOut* NextItem(TSourceStream* const source)
		{
			return reinterpret_cast<const TOut*>(source->NextItem());
		}
	};

	class TKernelStream : public ISink<byte_t>, public ISource<byte_t>
	{
		protected:
			system::handle::THandle handle;
			system::waitable::THandleWaitable w_input;
			system::waitable::THandleWaitable w_output;

		public:
			system::handle::handle_t Handle() final override { return this->handle; }

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::THandleWaitable* OnInputReady() const final override;
			bool CloseInput() final override;

			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::THandleWaitable* OnOutputReady() const final override;
			bool CloseOutput() final override;

			void Flush() final override;

			TKernelStream(system::handle::THandle handle);
			TKernelStream(const file::TPath& path);
	};

	/**********************************************/

	template<typename T>
	class TSourcePipe : public IPipe<TSourcePipe<T>, T>
	{
		protected:
			ISource<T>* const source;
			std::unique_ptr<T[]> buffer;
			usys_t index;
			usys_t n_items;

			static const usys_t N_ITEMS_BUFFER = util::Max<usys_t>(1, 4096U / sizeof(T));

		public:
			using TOut = T;
			using TIn = void;

			const TOut* NextItem() final override
			{
				for(;;)
				{
					if(index < n_items)
					{
						return buffer.get() + (index++);
					}
					else
					{
						index = 0;
						n_items = source->Read(buffer.get(), N_ITEMS_BUFFER);
						if(n_items == 0)
						{
							const system::waitable::IWaitable* const on_input_ready = source->OnInputReady();
							if(on_input_ready == nullptr)
								return nullptr;
							on_input_ready->WaitFor();
						}
					}
				}
			}

			TSourcePipe(ISource<T>* const source) : source(source), buffer(std::unique_ptr<T[]>(new T[N_ITEMS_BUFFER])), index(0), n_items(0) {}
			TSourcePipe(const TSourcePipe&) = delete;
			TSourcePipe(TSourcePipe&&) = default;
	};

	// template<typename TPipe, typename TOut>
	// struct ITransformator
	// {
	// 	virtual const TOut* NextItem() = 0;
	// 	virtual void Source(TPipe* const source) = 0;
	// };

	template<typename TPipe, typename L>
	class TFilterPipe : public IPipe<TFilterPipe<TPipe, L>, typename TPipe::TOut>
	{
		protected:
			TPipe* source;
			L callable;

		public:
			using TIn = typename TPipe::TOut;
			using TOut = TIn;

			const TOut* NextItem() final override
			{
				for(;;)
				{
					const TIn* const item = source->NextItem();

					if(item == nullptr)
						return nullptr;

					if(callable(*item))
						return item;
				}
			}

			TFilterPipe(TPipe* source, L callable) : source(source), callable(callable) {}
	};

	template<typename TPipe, typename L>
	class TMapPipe : public IPipe<TMapPipe<TPipe, L>, std::remove_cv_t<std::remove_reference_t<std::result_of_t<L(const typename TPipe::TOut&)>>>>
	{
		public:
			using TIn = typename TPipe::TOut;
			using TOut = std::remove_cv_t<std::remove_reference_t<std::result_of_t<L(const TIn&)>>>;

		protected:
			TPipe* source;
			L callable;
			TOut out;

		public:
			const TOut* NextItem() final override
			{
				const TIn* const item = source->NextItem();

				if(item == nullptr)
					return nullptr;

				out = callable(*item);
				return &out;
			}

			TMapPipe(TPipe* source, L callable) : source(source), callable(callable), out() {}
	};

	template<typename ... TPipes>
	class TConcatPipe : public IPipe<TConcatPipe<TPipes ...>, typename traits::type_at<0, TPipes ...>::type::TOut>
	{
		public:
			using TIn = typename traits::type_at<0, TPipes ...>::type::TOut;
			using TOut = TIn;

		protected:
			std::tuple<TPipes* ...> streams;

			template<std::size_t... Is>
			const TOut* NextItem(std::index_sequence<Is...>)
			{
				const TOut* item = nullptr;
				#ifdef EL_CC_CLANG
					#pragma clang diagnostic push
					#pragma clang diagnostic ignored "-Wunused-value"
				#endif
				( ((item = std::get<Is>(streams)->NextItem()) != nullptr) || ... );
				#ifdef EL_CC_CLANG
					#pragma clang diagnostic pop
				#endif
				return item;
			}

		public:
			const TOut* NextItem() final override
			{
				return NextItem(std::make_index_sequence<std::tuple_size<decltype(streams)>::value>{});
			}

			TConcatPipe(TPipes& ... streams) : streams(&streams ...) {}
	};

	template<typename TPipe, typename TTransformator>
	class TTransformPipe : public IPipe<TTransformPipe<TPipe, TTransformator>, typename std::remove_reference_t<TTransformator>::TOut>
	{
		public:
			using TIn = typename TPipe::TOut;
			using TOut = typename std::remove_reference_t<TTransformator>::TOut;

		protected:
			TPipe* const source;
			TTransformator transformator;

		public:
			const TOut* NextItem() final override
			{
				return transformator.NextItem(source);
			}

			TTransformPipe(TPipe* const source, TTransformator transformator) : source(source), transformator(transformator)
			{
			}
	};

	template<typename TPipe>
	class TLimitPipe : public IPipe<TLimitPipe<TPipe>, typename TPipe::TOut>
	{
		public:
			using TIn = typename TPipe::TOut;
			using TOut = TIn;

		protected:
			TPipe* const source;
			const iosize_t n_items_max;
			iosize_t i;

		public:
			const TOut* NextItem() final override
			{
				if(i < n_items_max)
				{
					i++;
					return source->NextItem();
				}
				else
				{
					return nullptr;
				}
			}

			TLimitPipe(TPipe* const source, const iosize_t n_items_max) : source(source), n_items_max(n_items_max), i(0)
			{
			}
	};

	/**********************************************/

	template<typename T>
	void ISource<T>::ReadAll(T* const arr_items, const usys_t n_items)
	{
		EL_ERROR(BlockingRead(arr_items, n_items) != n_items, TStreamDryException);
	}

	template<typename T>
	usys_t ISource<T>::BlockingRead(T* const arr_items, const usys_t n_items_max, system::time::TTime timeout, const bool absolute_time)
	{
		if(!absolute_time && timeout > 0)
			timeout += system::time::TTime::Now(system::time::EClock::MONOTONIC);

		usys_t n_read = 0;
		while(n_read < n_items_max)
		{
			const usys_t r = this->Read(arr_items + n_read, n_items_max - n_read);
			if(r == 0)
			{
				const system::waitable::IWaitable* const on_input_ready = this->OnInputReady();
				if(on_input_ready == nullptr)
				{
					break;
				}
				else
				{
					if(!on_input_ready->WaitFor(timeout, true))
						break;
				}
			}

			n_read += r;
		}
		return n_read;
	}

	template<typename T>
	iosize_t ISource<T>::WriteOut(ISink<T>& sink, const iosize_t n_items_max, const bool allow_recursion)
	{
		if(allow_recursion)
			return sink.ReadIn(*this, n_items_max, false);

		return Pump(*this, sink, n_items_max, false);
	}

	template<typename T>
	TSourcePipe<T> ISource<T>::Pipe()
	{
		return TSourcePipe<T>(this);
	}

	template<typename T>
	void ISink<T>::WriteAll(const T* const arr_items, const usys_t n_items)
	{
		usys_t w = 0;
		while(w < n_items)
		{
			const usys_t r = this->Write(arr_items + w, n_items - w);
			if(r == 0)
			{
				const system::waitable::IWaitable* const on_output_ready = this->OnOutputReady();
				EL_ERROR(on_output_ready == nullptr, TSinkFloodedException);
				on_output_ready->WaitFor();
			}
			w += r;
		}
	}

	template<typename T>
	usys_t ISink<T>::BlockingWrite(const T* const arr_items, const usys_t n_items_max, system::time::TTime timeout, const bool absolute_time)
	{
		if(!absolute_time && timeout > 0)
			timeout += system::time::TTime::Now(system::time::EClock::MONOTONIC);

		usys_t n_written = 0;
		while(n_written < n_items_max)
		{
			const usys_t r = this->Write(arr_items + n_written, n_items_max - n_written);
			if(r == 0)
			{
				const system::waitable::IWaitable* const on_output_ready = this->OnOutputReady();
				if(on_output_ready == nullptr)
				{
					break;
				}
				else
				{
					if(!on_output_ready->WaitFor(timeout, true))
						break;
				}
			}

			n_written += r;
		}
		return n_written;
	}

	template<typename T>
	iosize_t ISink<T>::ReadIn(ISource<T>& source, const iosize_t n_items_max, const bool allow_recursion)
	{
		if(allow_recursion)
			return source.WriteOut(*this, n_items_max, false);

		return Pump(source, *this, n_items_max, false);
	}

	/**********************************************/

	template<typename TPipe, typename TOut>
	usys_t IPipe<TPipe, TOut>::Read(TOut* const arr_items, const usys_t n_items_max)
	{
		TPipe* source = static_cast<TPipe*>(this);
		for(usys_t i = 0; i < n_items_max; i++)
		{
			const TOut* const item = source->NextItem();
			if(item == nullptr)
				return i;
			arr_items[i] = *item;
		}
		return n_items_max;
	}

	template<typename TPipe, typename TOut>
	iosize_t IPipe<TPipe, TOut>::ToStream(ISink<TOut>& sink)
	{
		TPipe* source = static_cast<TPipe*>(this);
		iosize_t n_total = 0;

		const usys_t sz_buffer = util::Max<usys_t>(1, 4096 / sizeof(TOut));
		TOut buffer[sz_buffer];

		for(;;)
		{
			usys_t n_read = 0;
			for(; n_read < sz_buffer; n_read++)
			{
				const TOut* const item = source->NextItem();
				if(item == nullptr)
					break;

				buffer[n_read] = *item;
			}

			sink.WriteAll(buffer, n_read);

			n_total += n_read;

			if(n_read < sz_buffer)
				return n_total;
		}
	}

	template<typename TPipe, typename TOut>
	usys_t IPipe<TPipe, TOut>::AppendTo(collection::list::TList<TOut>& list)
	{
		TPipe* source = static_cast<TPipe*>(this);
		usys_t i = 0;
		for(;;)
		{
			const TOut* const item = source->NextItem();
			if(item == nullptr)
				return i;
			list.Append(*item);
			i++;
		}
		return i;
	}

	template<typename TPipe, typename TOut>
	template<typename L>
	void IPipe<TPipe, TOut>::ForEach(L callable)
	{
		TPipe* source = static_cast<TPipe*>(this);
		for(;;)
		{
			const TOut* const item = source->NextItem();
			if(item == nullptr)
				return;
			callable(*item);
		}
	}

	template<typename TPipe, typename TOut>
	template<typename L, typename T>
	T IPipe<TPipe, TOut>::Aggregate(L lambda, T init)
	{
		T result = init;

		TPipe* source = static_cast<TPipe*>(this);
		for(;;)
		{
			const TOut* const item = source->NextItem();
			if(item == nullptr)
				break;

			lambda(result, *item);
		}

		return result;
	}

	template<typename TPipe, typename TOut>
	iosize_t IPipe<TPipe, TOut>::Count()
	{
		iosize_t count = 0;
		TPipe* source = static_cast<TPipe*>(this);

		for(const TOut* item = source->NextItem(); item != nullptr; item = source->NextItem())
			count++;

		return count;
	}

	template<typename TPipe, typename TOut>
	const TOut& IPipe<TPipe, TOut>::First()
	{
		TPipe* source = static_cast<TPipe*>(this);
		const TOut* const item = source->NextItem();
		EL_ERROR(item == nullptr, TStreamDryException);
		return *item;
	}

	template<typename TPipe, typename TOut>
	TLimitPipe<TPipe> IPipe<TPipe, TOut>::Limit(const iosize_t n_items_max)
	{
		TPipe* source = static_cast<TPipe*>(this);
		return TLimitPipe<TPipe>(source, n_items_max);
	}

	template<typename TPipe, typename TOut>
	template<typename L>
	TFilterPipe<TPipe, L> IPipe<TPipe, TOut>::Filter(L callable)
	{
		return TFilterPipe<TPipe, L>(static_cast<TPipe*>(this), callable);
	}

	template<typename TPipe, typename TOut>
	template<typename L>
	TMapPipe<TPipe, L> IPipe<TPipe, TOut>::Map(L callable)
	{
		return TMapPipe<TPipe, L>(static_cast<TPipe*>(this), callable);
	}

	template<typename TPipe, typename TOut>
	template<typename TTransformator>
	TTransformPipe<TPipe, TTransformator> IPipe<TPipe, TOut>::Transform(TTransformator&& transformator)
	{
		return TTransformPipe<TPipe, TTransformator>(static_cast<TPipe*>(this), std::move(transformator));
	}

	template<typename TPipe, typename TOut>
	template<typename TTransformator>
	TTransformPipe<TPipe, TTransformator&> IPipe<TPipe, TOut>::Transform(TTransformator& transformator)
	{
		return TTransformPipe<TPipe, TTransformator&>(static_cast<TPipe*>(this), transformator);
	}

	template<typename ... TPipes>
	TConcatPipe<TPipes ...> Concat(TPipes ... pipes)
	{
		return TConcatPipe<TPipes ...>(pipes ...);
	}

	/**********************************************/

	template<typename T>
	iosize_t _Pump(ISource<T>& source, ISink<T>& sink, const iosize_t n_items_max, const bool blocking)
	{
		const usys_t sz_buffer = util::Max<usys_t>(1U, 1024U / sizeof(T));
		iosize_t n_pumped = 0;
		T buffer[sz_buffer];

		while(n_pumped < n_items_max)
		{
			const usys_t r = blocking ? source.BlockingRead(buffer, sz_buffer) : source.Read(buffer, sz_buffer);
			if(r == 0)
				break;

			sink.WriteAll(buffer, r);
			n_pumped += r;
		}

		return n_pumped;
	}

	template<typename T>
	iosize_t Pump(ISource<T>& source, ISink<T>& sink, const iosize_t n_items_max, const bool blocking)
	{
		return _Pump<T>(source, sink, n_items_max);
	}

	template<>
	iosize_t Pump<byte_t>(ISource<byte_t>& source, ISink<byte_t>& sink, const iosize_t n_items_max, const bool blocking);
}

