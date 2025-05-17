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

namespace el1::io::file
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

	struct IStream
	{
		virtual ~IStream() {}

		/**
		* Retrieves the handle of the operating system object associated with the stream, if any.
		* @return The handle to the operating system object or system::handle::INVALID_HANDLE if not applicable.
		*/
		virtual system::handle::handle_t Handle() { return system::handle::INVALID_HANDLE; }

		/**
		* Closes the stream, indicating that no more data will be read or written.
		*/
		virtual void Close() {}
	};

	template<typename T>
	struct ISource : IStream
	{
		/**
		* Reads data from the source into the provided array.
		* This function should only return 0 if the source is either dry or blocked.
		* To determine if the source is blocked, use OnInputReady().
		* If OnInputReady() returns a waitable object, the source is blocked and the caller should use WaitFor().
		* If it returns nullptr, the source is dry and will not produce any more data.
		*
		* @param arr_items The array to store read items.
		* @param n_items_max The maximum number of items to read.
		* @return The number of items read.
		*/
		virtual usys_t Read(T* const arr_items, const usys_t n_items_max) EL_WARN_UNUSED_RESULT = 0;

		/**
		* Checks if the source is ready to provide more input data.
		* @return A waitable object if the source is blocked, or nullptr if the source is dry.
		*/
		virtual const system::waitable::IWaitable* OnInputReady() const { return nullptr; }

		/**
		* Directly writes data from the source to a sink, potentially avoiding copying data into a temporary buffer.
		*
		* @param sink The sink to which data should be written.
		* @param n_items_max The maximum number of items to write. Defaults to -1 (as much as possible, but not blocking).
		* @param allow_recursion Whether the implementation is allowed to call ReadIn() on the sink instead.
		* @return The number of items written.
		*/
		virtual iosize_t WriteOut(ISink<T>& sink, const iosize_t n_items_max = (iosize_t)-1, const bool allow_recursion = true);

		/**
		* Similar to Read(), but blocks until the specified number of items have been read, the source runs dry, or a timeout occurs.
		*
		* @param arr_items The array to store read items.
		* @param n_items_max The maximum number of items to read.
		* @param timeout The maximum time to wait. Defaults to -1 (no timeout).
		* @param absolute_time Whether the timeout is an absolute time. Defaults to false.
		* @return The number of items read.
		*/
		virtual usys_t BlockingRead(T* const arr_items, const usys_t n_items_max, system::time::TTime timeout = -1, const bool absolute_time = false) EL_WARN_UNUSED_RESULT;

		/**
		* Blocks until exactly the specified number of items have been read.
		* Throws an exception if the source fails to provide the requested amount of items.
		*
		* @param arr_items The array to store read items.
		* @param n_items The exact number of items to read.
		*/
		virtual void ReadAll(T* const arr_items, const usys_t n_items);

		/**
		* Blocks until exactly the specified number of items have been read into the provided array.
		* Throws an exception if the source fails to provide the requested amount of items.
		*
		* @param arr_items The array to store read items.
		*/
		void ReadAll(io::collection::list::array_t<T> arr_items) { this->ReadAll(arr_items.ItemPtr(0), arr_items.Count()); }

		/**
		* Closes the input side of a full-duplex connection.
		* Returns false if closing the input side is not possible.
		*
		* @return True if the input was successfully closed, false otherwise.
		*/
		virtual bool CloseInput() EL_WARN_UNUSED_RESULT { return false; }

		/**
		* Discards the specified number of items from the source.
		*
		* @param n_items The number of items to discard.
		*/
		virtual void Discard(const usys_t n_items)
		{
			usys_t n_read = 0;
			const usys_t n_batch = util::Max<usys_t>(1, 512 / sizeof(T));
			T buffer[n_batch];

			while (n_read < n_items)
			{
				const usys_t n_remaining = n_items - n_read;
				const usys_t n_now = util::Min(n_remaining, n_batch);
				ReadAll(buffer, n_now);
				n_read += n_now;
			}
		}

		TSourcePipe<T> Pipe();
	};

	template<typename T>
	struct ISink : IStream
	{
		/**
		* Writes data to the sink from the provided array.
		* Basic semantics follow that of ISource::Read().
		*
		* @param arr_items The array containing items to write.
		* @param n_items_max The maximum number of items to write.
		* @return The number of items written.
		*/
		virtual usys_t Write(const T* const arr_items, const usys_t n_items_max) EL_WARN_UNUSED_RESULT = 0;

		/**
		* Checks if the sink is ready to accept more output data.
		*
		* @return A waitable object if the sink is blocked, or nullptr if the sink is ready.
		*/
		virtual const system::waitable::IWaitable* OnOutputReady() const { return nullptr; }

		/**
		* Directly reads data from a source into the sink, potentially avoiding copying data into a temporary buffer.
		*
		* @param source The source from which data should be read.
		* @param n_items_max The maximum number of items to read. Defaults to -1 (all items).
		* @param allow_recursion Whether the implementation is allowed to call WriteOut() on the source instead.
		* @return The number of items read.
		*/
		virtual iosize_t ReadIn(ISource<T>& source, const iosize_t n_items_max = (iosize_t)-1, const bool allow_recursion = true);

		/**
		* Blocks until the specified number of items have been written, the sink becomes full, or a timeout occurs.
		*
		* @param arr_items The array containing items to write.
		* @param n_items_max The maximum number of items to write.
		* @param timeout The maximum time to wait. Defaults to -1 (no timeout).
		* @param absolute_time Whether the timeout is an absolute time. Defaults to false.
		* @return The number of items written.
		*/
		usys_t BlockingWrite(const T* const arr_items, const usys_t n_items_max, system::time::TTime timeout = -1, const bool absolute_time = false) EL_WARN_UNUSED_RESULT;

		/**
		* Blocks until exactly the specified number of items have been written.
		* Throws an exception if the sink fails to accept the requested amount of items.
		*
		* @param arr_items The array containing items to write.
		* @param n_items The exact number of items to write.
		*/
		virtual void WriteAll(const T* const arr_items, const usys_t n_items);

		/**
		* Blocks until exactly the specified number of items have been written from the provided array.
		* Throws an exception if the sink fails to accept the requested amount of items.
		*
		* @param arr_items The array containing items to write.
		*/
		void WriteAll(io::collection::list::array_t<const T> arr_items) { this->WriteAll(arr_items.ItemPtr(0), arr_items.Count()); }

		/**
		* Closes the output side of a full-duplex connection.
		* Returns false if closing the output side is not possible.
		*
		* @return True if the output was successfully closed, false otherwise.
		*/
		virtual bool CloseOutput() EL_WARN_UNUSED_RESULT { return false; }

		/**
		* Flushes any output buffers, ensuring all data is written.
		*/
		virtual void Flush() {}
	};

	template<typename T>
	struct IBufferedSource : ISource<T>
	{
		// returns a preview of the available items in the source directly from its internal buffers
		// use Discard() or Read() to remove the items from the source when you no longer need them
		// this invalidates the previously returned buffer
		// and allows the source to retrieve or produce the next batch of items
		virtual collection::list::array_t<T> Peek() = 0;
	};

	using IBinarySink = ISink<byte_t>;
	using IBinarySource = ISource<byte_t>;

	template<typename TPipe, typename TOut, bool is_copy_constructible>
	struct TPipeSource
	{
	};

	template<typename TPipe, typename TOut>
	struct TPipeSource<TPipe, TOut, true> : public ISource<typename std::remove_const<TOut>::type>
	{
		using TOutBase = typename std::remove_const<TOut>::type;
		usys_t Read(TOutBase* const arr_items, const usys_t n_items_max) override EL_WARN_UNUSED_RESULT;
	};

	template<typename TPipe, typename TOut>
	struct TPipeSource<TPipe, TOut, false>
	{
	};

	template<typename TPipe, typename _TOut>
	struct IPipe : public TPipeSource<TPipe, _TOut, std::is_copy_constructible_v<_TOut> >
	{
		using TOut = _TOut;

		// this function is called repeatedly to fetch the next item from the stream
		// once the pipe has run out of items it must return nullptr and continue to do so
		// the returned item pointer needs only to be valid until the next call
		virtual TOut* NextItem() = 0;

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
		io::collection::list::TList<TOut> Collect(const usys_t n_prealloc = 0);
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
		auto NextItem(TSourceStream* const source)
		{
			return reinterpret_cast<const TOut*>(source->NextItem());
		}
	};

	class TKernelStream : public ISink<byte_t>, public ISource<byte_t>
	{
		public:
			system::handle::THandle handle;

		protected:
			system::waitable::THandleWaitable w_input;
			system::waitable::THandleWaitable w_output;

		public:

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

			TOut* NextItem() final override
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
	// 	virtual TOut* NextItem() final override = 0;
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

			TOut* NextItem() final override
			{
				for(;;)
				{
					auto item = source->NextItem();

					if(item == nullptr)
						return nullptr;

					if(callable(*item))
						return item;
				}
			}

			TFilterPipe(TPipe* source, L callable) : source(source), callable(callable) {}
	};

	template<typename TPipe>
	class TLimitPipe : public IPipe<TLimitPipe<TPipe>, typename TPipe::TOut>
	{
		protected:
			TPipe* source;
			iosize_t n_remaining;

		public:
			using TIn = typename TPipe::TOut;
			using TOut = TIn;

			TOut* NextItem() final override
			{
				if(EL_UNLIKELY(n_remaining == 0))
					return nullptr;

				n_remaining--;
				return source->NextItem();
			}

			TLimitPipe(TPipe* source, const iosize_t n_items_limit) : source(source), n_remaining(n_items_limit) {}
	};

	template<typename T>
	class TLimitSource : public ISource<T>
	{
		protected:
			ISource<T>* source;
			iosize_t n_remaining;

		public:
			usys_t Read(T* const arr_items, const usys_t n_items_max) EL_WARN_UNUSED_RESULT
			{
				if(n_remaining == 0)
					return 0;

				const usys_t n_max = util::Min<usys_t>(n_remaining, n_items_max);
				const usys_t r = source->Read(arr_items, n_max);
				n_remaining -= r;
				return r;
			}

			const system::waitable::IWaitable* OnInputReady() const
			{
				if(n_remaining == 0)
					return nullptr;
				return this->source->OnInputReady();
			}

			TLimitSource(ISource<T>* source, const iosize_t n_items_limit) : source(source), n_remaining(n_items_limit) {}
	};


	template<typename TPipe, typename L>
	class TMapPipe : public IPipe<TMapPipe<TPipe, L>, std::remove_cv_t<std::remove_reference_t<std::result_of_t<L(typename TPipe::TOut&)>>>>
	{
		public:
			using TIn = typename TPipe::TOut;
			using TOut = std::remove_cv_t<std::remove_reference_t<std::result_of_t<L(TIn&)>>>;

		protected:
			TPipe* source;
			L callable;
			TOut out;

		public:
			TOut* NextItem() final override
			{
				auto item = source->NextItem();

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
			TOut* NextItem(std::index_sequence<Is...>)
			{
				TOut* item = nullptr;
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
			TOut* NextItem() final override
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
			std::remove_reference_t<TTransformator> transformator;

		public:
			TOut* NextItem() final override
			{
				return transformator.NextItem(source);
			}

			TTransformPipe(TPipe* const source, TTransformator&& transformator) : source(source), transformator(std::move(transformator))
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
	usys_t TPipeSource<TPipe, TOut, true>::Read(TOutBase* const arr_items, const usys_t n_items_max)
	{
		TPipe* source = static_cast<TPipe*>(this);
		for(usys_t i = 0; i < n_items_max; i++)
		{
			auto item = source->NextItem();
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
				auto item = source->NextItem();
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
			auto item = source->NextItem();
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
			auto item = source->NextItem();
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
			auto item = source->NextItem();
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
		auto item = source->NextItem();
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
	iosize_t Pump(ISource<T>& source, ISink<T>& sink, const iosize_t n_items_max, const bool)
	{
		return _Pump<T>(source, sink, n_items_max);
	}

	template<>
	iosize_t Pump<byte_t>(ISource<byte_t>& source, ISink<byte_t>& sink, const iosize_t n_items_max, const bool blocking);
}

