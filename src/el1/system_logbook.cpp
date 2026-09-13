#include "system_logbook.hpp"

#include <atomic>
#include <string.h>

namespace el1::system::logbook
{
	const TTime TLogRecord::T_PROGRAM_START = TTime::Now(EClock::MONOTONIC);

	namespace
	{
		struct TContextState
		{
			task::TSimpleMutex lock;
			TLogContextDataBase* tail = nullptr;
		};

		TContextState& ContextState()
		{
			static TContextState* const state = new TContextState;
			return *state;
		}

		struct TLogBookState
		{
			task::TSimpleMutex lock;
			TList<ILogSink*> sinks;
			std::atomic<TLogFilter::mask_t> pass_through_mask{0};
		};

		TLogBookState& LogBookState()
		{
			static TLogBookState* const state = new TLogBookState;
			return *state;
		}

		void UpdatePassThroughMask(TLogBookState& state) noexcept
		{
			TLogFilter::mask_t mask = 0;
			for(const ILogSink* const sink : state.sinks)
				mask |= sink->pass_through_filter.mask;
			state.pass_through_mask.store(mask, std::memory_order_release);
		}
	}

	usys_t TFlightRecorder::DEFAULT_SIZE_BYTES = 64U * 1024U;

	const TLogContextDataBase* TLogContextDataBase::Tail() noexcept
	{
		return ContextState().tail;
	}

	TLogContextDataBase::TLogContextDataBase()
	{
		TContextState& state = ContextState();
		const task::TMutexAutoLock guard(&state.lock);
		prev = state.tail;
		if(state.tail != nullptr)
			state.tail->next = this;
		state.tail = this;
	}

	TLogContextDataBase::~TLogContextDataBase()
	{
		TContextState& state = ContextState();
		const task::TMutexAutoLock guard(&state.lock);
		if(prev != nullptr)
			prev->next = next;
		if(next != nullptr)
			next->prev = prev;
		if(state.tail == this)
			state.tail = prev;
	}

	void TLogBook::RegisterSink(ILogSink* const sink)
	{
		EL_ERROR(sink == nullptr, error::TLogicException);
		TLogBookState& state = LogBookState();
		const task::TMutexAutoLock guard(&state.lock);
		if(!state.sinks.Contains(sink))
		{
			state.sinks.Append(sink);
			UpdatePassThroughMask(state);
		}
	}

	void TLogBook::UnregisterSink(ILogSink* const sink)
	{
		TLogBookState& state = LogBookState();
		const task::TMutexAutoLock guard(&state.lock);
		state.sinks.RemoveItem(sink, NEG1);
		UpdatePassThroughMask(state);
	}

	bool TLogBook::ShouldPassThrough(const ILogSiteBase& site) noexcept
	{
		const TLogBookState& state = LogBookState();
		const TLogFilter aggregate{state.pass_through_mask.load(std::memory_order_acquire)};
		return aggregate.Matches(site.category, site.verbosity);
	}

	void TLogBook::PassThrough(
		const task::TThread* const thread,
		const array_t<const byte_t> record
	) noexcept
	{
		try
		{
			if(record.Count() < sizeof(TLogRecord))
				return;

			const TLogRecord& log_record = *reinterpret_cast<const TLogRecord*>(record.Data());
			if(log_record.site == nullptr || log_record.Size() != record.Count())
				return;

			TList<ILogSink*> matching_sinks;
			{
				TLogBookState& state = LogBookState();
				const task::TMutexAutoLock guard(&state.lock);
				for(ILogSink* const sink : state.sinks)
					if(sink->pass_through_filter.Matches(log_record.site->category, log_record.site->verbosity))
						matching_sinks.Append(sink);
			}

			for(ILogSink* const sink : matching_sinks)
			{
				try
				{
					sink->Write(thread, record, 0, 0);
				}
				catch(...)
				{
				}
			}
		}
		catch(...)
		{
		}
	}

	void TLogBook::Write(
		const task::TThread* const thread,
		const array_t<const byte_t> records,
		const u64_t n_overwritten_events,
		const u64_t n_dropped_events
	) noexcept
	{
		try
		{
			TList<ILogSink*> current_sinks;
			{
				TLogBookState& state = LogBookState();
				const task::TMutexAutoLock guard(&state.lock);
				current_sinks = state.sinks;
			}

			for(ILogSink* const sink : current_sinks)
			{
				try
				{
					sink->Write(thread, records, n_overwritten_events, n_dropped_events);
				}
				catch(...)
				{
				}
			}
		}
		catch(...)
		{
		}
	}

	usys_t TFlightRecorder::TRingSink::Write(const byte_t* const data, const usys_t n_items_max)
	{
		recorder->WriteBytes(data, n_items_max);
		return n_items_max;
	}

	TFlightRecorder::TFlightRecorder(const task::TThread* const thread, const usys_t sz_bytes) :
		buffer(sz_bytes == 0 ? nullptr : new byte_t[sz_bytes]),
		thread(thread),
		sz_buffer(sz_bytes)
	{
	}

	TFlightRecorder::~TFlightRecorder()
	{
		if(!discarded && (sz_used != 0 || n_overwritten_events != 0 || n_dropped_events != 0))
			Commit();
	}

	void TFlightRecorder::ReadBytes(usys_t offset, void* const destination, usys_t n_bytes) const noexcept
	{
		byte_t* dst = reinterpret_cast<byte_t*>(destination);
		while(n_bytes != 0)
		{
			const usys_t n_chunk = util::Min(n_bytes, sz_buffer - offset);
			memcpy(dst, buffer.get() + offset, n_chunk);
			dst += n_chunk;
			n_bytes -= n_chunk;
			offset = (offset + n_chunk) % sz_buffer;
		}
	}

	void TFlightRecorder::WriteBytes(const void* const source, usys_t n_bytes) noexcept
	{
		const byte_t* src = reinterpret_cast<const byte_t*>(source);
		while(n_bytes != 0)
		{
			const usys_t n_chunk = util::Min(n_bytes, sz_buffer - tail);
			memcpy(buffer.get() + tail, src, n_chunk);
			src += n_chunk;
			n_bytes -= n_chunk;
			tail = (tail + n_chunk) % sz_buffer;
		}
	}

	void TFlightRecorder::Reserve(const usys_t n_bytes)
	{
		EL_ERROR(n_bytes > sz_buffer, error::TLogicException);
		while(sz_buffer - sz_used < n_bytes)
		{
			TLogRecord record{};
			ReadBytes(head, &record, sizeof(record));
			const usys_t sz_record = record.Size();
			EL_ERROR(sz_record > sz_used || sz_record > sz_buffer, error::TLogicException);
			head = (head + sz_record) % sz_buffer;
			sz_used -= sz_record;
			n_overwritten_events++;
		}
	}

	void TFlightRecorder::Clear() noexcept
	{
		head = 0;
		tail = 0;
		sz_used = 0;
	}

	u64_t TFlightRecorder::Timestamp() const noexcept
	{
		try
		{
			const s64_t value = (TTime::Now(EClock::MONOTONIC) - TLogRecord::T_PROGRAM_START).ConvertToI(EUnit::MICROSECONDS);
			if(value <= 0)
				return 0;
			constexpr u64_t MAX_VALUE = (1ULL << 48U) - 1ULL;
			return util::Min<u64_t>(static_cast<u64_t>(value), MAX_VALUE);
		}
		catch(...)
		{
			return 0;
		}
	}

	void TFlightRecorder::Commit() noexcept
	{
		try
		{
			TList<byte_t> records;
			if(sz_used != 0)
			{
				records.Clear(sz_used);
				const usys_t n_first = util::Min(sz_used, sz_buffer - head);
				records.Append(buffer.get() + head, n_first);
				if(n_first != sz_used)
					records.Append(buffer.get(), sz_used - n_first);
			}

			TLogBook::Write(thread, records.View(), n_overwritten_events, n_dropped_events);
		}
		catch(...)
		{
		}

		Clear();
		n_overwritten_events = 0;
		n_dropped_events = 0;
		discarded = true;
	}

	void TFlightRecorder::Discard() noexcept
	{
		Clear();
		n_overwritten_events = 0;
		n_dropped_events = 0;
		discarded = true;
	}
}
