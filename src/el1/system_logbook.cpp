#include "system_logbook.hpp"
#include "io_text_terminal.hpp"

#include <atomic>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

		bool IsFalseEnvironmentValue(const char* const value) noexcept
		{
			if(value == nullptr)
				return false;
			return strcmp(value, "0") == 0 ||
				strcmp(value, "false") == 0 || strcmp(value, "FALSE") == 0 ||
				strcmp(value, "off") == 0 || strcmp(value, "OFF") == 0 ||
				strcmp(value, "no") == 0 || strcmp(value, "NO") == 0;
		}

		TLogFilter ConsoleFilterFromEnvironment() noexcept
		{
			const char* value = getenv("EL1_LOG_CONSOLE_VERBOSITY");
			if(value == nullptr || *value == 0)
				value = getenv("EL1_LOG_VERBOSITY");
			if(value == nullptr || *value == 0)
				return TLogBook::DEFAULT_CONSOLE_FILTER;

			if(strcmp(value, "trace") == 0 || strcmp(value, "TRACE") == 0)
				return TLogFilter::AtLeastVerbosity(EVerbosity::TRACE);
			if(strcmp(value, "debug") == 0 || strcmp(value, "DEBUG") == 0)
				return TLogFilter::AtLeastVerbosity(EVerbosity::DEBUG);
			if(strcmp(value, "diag") == 0 || strcmp(value, "DIAG") == 0)
				return TLogFilter::AtLeastVerbosity(EVerbosity::DIAG);
			if(strcmp(value, "verbose") == 0 || strcmp(value, "VERBOSE") == 0)
				return TLogFilter::AtLeastVerbosity(EVerbosity::VERBOSE);
			if(strcmp(value, "operational") == 0 || strcmp(value, "OPERATIONAL") == 0 ||
				strcmp(value, "info") == 0 || strcmp(value, "INFO") == 0)
				return TLogFilter::AtLeastVerbosity(EVerbosity::OPERATIONAL);
			if(strcmp(value, "terse") == 0 || strcmp(value, "TERSE") == 0)
				return TLogFilter::AtLeastVerbosity(EVerbosity::TERSE);
			if(strcmp(value, "off") == 0 || strcmp(value, "OFF") == 0 || strcmp(value, "none") == 0 || strcmp(value, "NONE") == 0)
				return TLogFilter::None();

			return TLogBook::DEFAULT_CONSOLE_FILTER;
		}

		struct TLogBookState
		{
			task::TSimpleMutex lock;
			TConsoleLogSink console_sink;
			TList<ILogSink*> sinks;
			bool console_enabled;
			std::atomic<TLogFilter::mask_t> pass_through_mask{0};

			TLogBookState() :
				console_sink(ConsoleFilterFromEnvironment()),
				console_enabled(!IsFalseEnvironmentValue(getenv("EL1_LOG_CONSOLE")))
			{
				if(console_enabled)
				{
					sinks.Append(&console_sink);
					pass_through_mask.store(console_sink.PassThroughFilter().mask, std::memory_order_relaxed);
				}
			}
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
				mask |= sink->PassThroughFilter().mask;
			state.pass_through_mask.store(mask, std::memory_order_release);
		}

		TString ConsoleTimestamp(const TLogRecord& record)
		{
			const TTime record_monotonic = TLogRecord::T_PROGRAM_START + TTime::ConvertFrom(EUnit::MICROSECONDS, static_cast<s64_t>(record.ts));
			const TTime age = TTime::Now(EClock::MONOTONIC) - record_monotonic;
			const TTime record_realtime = TTime::Now(EClock::REALTIME) - age;
			const time_t seconds = static_cast<time_t>(record_realtime.Seconds());
			tm local_time = {};
			EL_ERROR(localtime_r(&seconds, &local_time) == nullptr, error::TException, U"failed to convert console timestamp to local time");

			char time[16] = {};
			EL_ERROR(strftime(time, sizeof(time), "%H:%M:%S", &local_time) == 0, error::TException, U"failed to format console timestamp");
			const u64_t tenths = static_cast<u64_t>(record_realtime.Attoseconds() / 100000000000000000LL);
			return TString::Format(U"%s.%d", TString(time), tenths);
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

	TStringView CategoryName(const ECategory category) noexcept
	{
		switch(category)
		{
			case ECategory::LIVENESS:    return U"LIVENESS";
			case ECategory::STATE_CHANGE:return U"STATE_CHANGE";
			case ECategory::DEGRADED:    return U"DEGRADED";
			case ECategory::PERFORMANCE: return U"PERFORMANCE";
			case ECategory::PROGRESS:    return U"PROGRESS";
			case ECategory::EXCEPTION:   return U"EXCEPTION";
		}
		return U"UNKNOWN";
	}

	TStringView VerbosityName(const EVerbosity verbosity) noexcept
	{
		switch(verbosity)
		{
			case EVerbosity::TRACE:      return U"TRACE";
			case EVerbosity::DEBUG:      return U"DEBUG";
			case EVerbosity::DIAG:       return U"DIAG";
			case EVerbosity::VERBOSE:    return U"VERBOSE";
			case EVerbosity::OPERATIONAL:return U"OPERATIONAL";
			case EVerbosity::TERSE:      return U"TERSE";
		}
		return U"UNKNOWN";
	}

	TConsoleLogSink::TConsoleLogSink(const TLogFilter filter) noexcept :
		ILogSink(filter, true)
	{
	}

	void TConsoleLogSink::WriteUnlocked(
		const task::TThread* const thread,
		const array_t<const byte_t> records,
		const u64_t n_overwritten_events,
		const u64_t n_dropped_events
	)
	{
		const TStringView thread_name = thread == nullptr ? TStringView(U"-") : thread->Name().View();

		usys_t offset = 0;
		while(offset + sizeof(TLogRecord) <= records.Count())
		{
			const TLogRecord& record = *reinterpret_cast<const TLogRecord*>(records.Data() + offset);
			const usys_t sz_record = record.Size();
			if(record.site == nullptr || sz_record < sizeof(TLogRecord) || sz_record > records.Count() - offset)
				break;

			const TString message = record.site->FormatMessage(record);
			io::text::terminal::term << TString::Format(
				U"[%s] %s/%s [%s] %s\n",
				ConsoleTimestamp(record),
				VerbosityName(record.site->verbosity),
				CategoryName(record.site->category),
				thread_name,
				message
			);
			offset += sz_record;
		}

		if(n_overwritten_events != 0 || n_dropped_events != 0)
		{
			io::text::terminal::term << TString::Format(
				U"[logbook] [%s] flight recorder lost events: overwritten=%d dropped=%d\n",
				thread_name,
				n_overwritten_events,
				n_dropped_events
			);
		}
	}

	void TConsoleLogSink::Write(
		const task::TThread* const thread,
		const array_t<const byte_t> records,
		const u64_t n_overwritten_events,
		const u64_t n_dropped_events
	)
	{
		const task::TMutexAutoLock guard(&write_lock);
		WriteUnlocked(thread, records, n_overwritten_events, n_dropped_events);
	}

	void TConsoleLogSink::WriteCommitted(
		const task::TThread* const thread,
		const array_t<const byte_t> records,
		const u64_t n_overwritten_events,
		const u64_t n_dropped_events
	)
	{
		const task::TMutexAutoLock guard(&write_lock);
		const TStringView thread_name = thread == nullptr ? TStringView(U"-") : thread->Name().View();
		io::text::terminal::term << TString::Format(
			U"[logbook] [%s] --- flight recorder replay ---\n",
			thread_name
		);
		WriteUnlocked(thread, records, n_overwritten_events, n_dropped_events);
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

	void TLogBook::SetConsoleEnabled(const bool enabled)
	{
		TLogBookState& state = LogBookState();
		const task::TMutexAutoLock guard(&state.lock);
		if(state.console_enabled == enabled)
			return;

		state.console_enabled = enabled;
		if(enabled)
		{
			if(!state.sinks.Contains(&state.console_sink))
				state.sinks.Append(&state.console_sink);
		}
		else
			state.sinks.RemoveItem(&state.console_sink, NEG1);
		UpdatePassThroughMask(state);
	}

	bool TLogBook::ConsoleEnabled()
	{
		TLogBookState& state = LogBookState();
		const task::TMutexAutoLock guard(&state.lock);
		return state.console_enabled;
	}

	void TLogBook::SetConsoleFilter(const TLogFilter filter)
	{
		TLogBookState& state = LogBookState();
		const task::TMutexAutoLock guard(&state.lock);
		state.console_sink.pass_through_filter = filter;
		UpdatePassThroughMask(state);
	}

	TLogFilter TLogBook::ConsoleFilter()
	{
		TLogBookState& state = LogBookState();
		const task::TMutexAutoLock guard(&state.lock);
		return state.console_sink.pass_through_filter;
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
					if(sink->PassThroughFilter().Matches(log_record.site->category, log_record.site->verbosity))
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
					if(sink->receive_committed_records)
						sink->WriteCommitted(thread, records, n_overwritten_events, n_dropped_events);
					else if(n_overwritten_events != 0 || n_dropped_events != 0)
						sink->WriteCommitted(thread, {}, n_overwritten_events, n_dropped_events);
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
