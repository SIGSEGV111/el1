#pragma once

#include "io_text_string.hpp"
#include "io_serialization_binary.hpp"
#include "io_serialization_json.hpp"
#include "system_task.hpp"
#include "debug.hpp"

#include <limits>
#include <tuple>
#include <type_traits>

namespace el1::system::logbook
{
	using namespace io::types;
	using namespace io::collection::array;
	using namespace io::collection::list;
	using namespace io::text::string;
	using namespace io::text::format;
	using namespace io::format::json;
	using namespace io::serialization;
	using namespace system::time;

	using obj_id_t = u32_t;

	template<typename T>
	obj_id_t GetObjectIdentity(const T* const object)
	{
		const usys_t value = reinterpret_cast<usys_t>(object);
		if constexpr(sizeof(usys_t) > sizeof(obj_id_t))
			return static_cast<obj_id_t>(value ^ (value >> (sizeof(obj_id_t) * 8U)));
		else
			return static_cast<obj_id_t>(value);
	}

	enum class ECategory : u8_t
	{
		LIVENESS,
		STATE_CHANGE,
		DEGRADED,
		PERFORMANCE,
		PROGRESS,
		EXCEPTION
	};

	enum class EVerbosity : u8_t
	{
		TRACE,
		DEBUG,
		DIAG,
		VERBOSE,
		OPERATIONAL,
		TERSE
	};

	struct TLogFilter
	{
		using mask_t = u64_t;

		mask_t mask = 0;

		static constexpr usys_t N_CATEGORIES = static_cast<usys_t>(ECategory::EXCEPTION) + 1;
		static constexpr usys_t N_VERBOSITIES = static_cast<usys_t>(EVerbosity::TERSE) + 1;
		static_assert(N_CATEGORIES * N_VERBOSITIES <= sizeof(mask_t) * 8U);

		constexpr bool Matches(const ECategory category, const EVerbosity verbosity) const noexcept
		{
			return (mask & Bit(category, verbosity)) != 0;
		}

		constexpr TLogFilter operator|(const TLogFilter other) const noexcept
		{
			return TLogFilter{mask | other.mask};
		}

		static constexpr TLogFilter None() noexcept
		{
			return {};
		}

		static constexpr TLogFilter All() noexcept
		{
			return TLogFilter{(mask_t{1} << (N_CATEGORIES * N_VERBOSITIES)) - 1};
		}

		static constexpr TLogFilter AtLeastVerbosity(const EVerbosity minimum) noexcept
		{
			mask_t result = 0;
			for(usys_t category = 0; category < N_CATEGORIES; category++)
				for(usys_t verbosity = static_cast<usys_t>(minimum); verbosity < N_VERBOSITIES; verbosity++)
					result |= mask_t{1} << (category * N_VERBOSITIES + verbosity);
			return TLogFilter{result};
		}

		static constexpr TLogFilter Category(const ECategory category, const EVerbosity minimum = EVerbosity::TRACE) noexcept
		{
			mask_t result = 0;
			for(usys_t verbosity = static_cast<usys_t>(minimum); verbosity < N_VERBOSITIES; verbosity++)
				result |= Bit(category, static_cast<EVerbosity>(verbosity));
			return TLogFilter{result};
		}

	private:
		static constexpr mask_t Bit(const ECategory category, const EVerbosity verbosity) noexcept
		{
			return mask_t{1} << (static_cast<usys_t>(category) * N_VERBOSITIES + static_cast<usys_t>(verbosity));
		}
	};

	struct TLogContextDataBase
	{
		mutable TLogContextDataBase* prev = nullptr;
		mutable TLogContextDataBase* next = nullptr;

		static const TLogContextDataBase* Tail() noexcept;

		TLogContextDataBase();
		TLogContextDataBase(const TLogContextDataBase&) = delete;
		TLogContextDataBase& operator=(const TLogContextDataBase&) = delete;
		virtual ~TLogContextDataBase();
	};

	template<typename T>
	struct TLogContextData final : TLogContextDataBase
	{
		const char32_t* const key;
		const T* const data;

		TLogContextData(const char32_t* const key, const T* const data) : key(key), data(data) {}
	};

	struct ILogSiteBase;

	struct EL_PACKED TLogRecord
	{
		static const TTime T_PROGRAM_START;

		u64_t	ts : 48,
				sz_args : 16;
		const ILogSiteBase* site;
		const void* call_site;
		obj_id_t object_id;

		usys_t Size() const noexcept
		{
			return sizeof(TLogRecord) + static_cast<usys_t>(sz_args);
		}

		array_t<const byte_t> SerializedArgs() const noexcept
		{
			return array_t<const byte_t>::FromUnsafePointer(
				reinterpret_cast<const byte_t*>(this + 1),
				static_cast<usys_t>(sz_args)
			);
		}
	};

	struct ILogSiteBase
	{
		const char32_t* const format_string;
		const ECategory category;
		const EVerbosity verbosity;

		ILogSiteBase(const char32_t* const format_string, const ECategory category, const EVerbosity verbosity) :
			format_string(format_string), category(category), verbosity(verbosity)
		{
		}

		virtual TString FormatMessage(const TLogRecord& record) const = 0;
		virtual TJsonArray Arguments(const TLogRecord& record) const = 0;
		virtual ~ILogSiteBase() = default;
	};

	namespace detail
	{
		template<typename ... A>
		std::tuple<A ...> DeserializeArguments(const TLogRecord& record)
		{
			std::tuple<A ...> args{};
			if constexpr(sizeof...(A) != 0)
			{
				const auto serialized_args = record.SerializedArgs();
				auto source = serialized_args.Source();
				binary::packed::TReader reader(&source);
				std::apply(
					[&](A& ... arg)
					{
						(io::serialization::Deserialize(reader, arg), ...);
					},
					args
				);
			}
			return args;
		}
	}

	template<TFixedString FORMAT, typename O, typename ... A>
	struct TLogSite final : ILogSiteBase
	{
		using args_t = std::tuple<A ...>;
		using format_t = TFormatString<A ...>;

		static constexpr format_t FORMAT_STRING{FORMAT.data};

		TLogSite(const ECategory category, const EVerbosity verbosity) :
			ILogSiteBase(FORMAT_STRING.literal, category, verbosity)
		{
		}

		args_t DeserializeArguments(const TLogRecord& record) const
		{
			return detail::DeserializeArguments<A ...>(record);
		}

		TString FormatMessage(const TLogRecord& record) const final override
		{
			const args_t args = DeserializeArguments(record);
			TString str;

			if constexpr(!std::is_void_v<O>)
			{
				if(record.object_id != 0)
				{
					constexpr debug::TTypeNameView type_name = debug::GetTypeName<O>();
					str = TString::Format(
						U"[%s@%u] ",
						TString(type_name.data, type_name.length),
						record.object_id
					);
				}
			}

			std::apply(
				[&](const A& ... arg)
				{
					str += TString::Format(FORMAT_STRING, arg ...);
				},
				args
			);
			return str;
		}

		TJsonArray Arguments(const TLogRecord& record) const final override
		{
			const args_t args = DeserializeArguments(record);
			TJsonArray arr;
			std::apply(
				[&](const A& ... arg)
				{
					(arr.Append(io::serialization::json::ToValue(arg)), ...);
				},
				args
			);
			return arr;
		}
	};

	struct ILogSink
	{
	private:
		friend class TLogBook;
		TLogFilter pass_through_filter;

	public:
		const bool receive_committed_records;

		explicit ILogSink(
			const TLogFilter pass_through_filter = TLogFilter::None(),
			const bool receive_committed_records = true
		) noexcept :
			pass_through_filter(pass_through_filter),
			receive_committed_records(receive_committed_records)
		{
		}

		TLogFilter PassThroughFilter() const noexcept { return pass_through_filter; }

		virtual void Write(
			const task::TThread* thread,
			array_t<const byte_t> records,
			u64_t n_overwritten_events,
			u64_t n_dropped_events
		) = 0;

		/**
		 * Delivery of a complete committed flight recorder. The default keeps
		 * existing sinks source-compatible by forwarding to Write().
		 */
		virtual void WriteCommitted(
			const task::TThread* thread,
			array_t<const byte_t> records,
			u64_t n_overwritten_events,
			u64_t n_dropped_events
		)
		{
			Write(thread, records, n_overwritten_events, n_dropped_events);
		}

		virtual ~ILogSink() = default;
	};

	/**
	 * Human-readable stderr sink. TLogBook owns and enables one process-wide
	 * instance by default; applications normally configure it through TLogBook.
	 *
	 * Matching console records are delivered immediately through pass-through.
	 * When a thread commits its flight recorder, the console receives the complete
	 * recorder again in recorder order, including records below the live filter.
	 */
	class TConsoleLogSink final : public ILogSink
	{
		task::TSimpleMutex write_lock;

		void WriteUnlocked(
			const task::TThread* thread,
			array_t<const byte_t> records,
			u64_t n_overwritten_events,
			u64_t n_dropped_events
		);

	public:
		explicit TConsoleLogSink(
			TLogFilter filter = TLogFilter::AtLeastVerbosity(EVerbosity::OPERATIONAL)
		) noexcept;

		void Write(
			const task::TThread* thread,
			array_t<const byte_t> records,
			u64_t n_overwritten_events,
			u64_t n_dropped_events
		) final override;

		void WriteCommitted(
			const task::TThread* thread,
			array_t<const byte_t> records,
			u64_t n_overwritten_events,
			u64_t n_dropped_events
		) final override;
	};

	TStringView CategoryName(ECategory category) noexcept;
	TStringView VerbosityName(EVerbosity verbosity) noexcept;

	class TLogBook
	{
		friend class TFlightRecorder;
		static bool ShouldPassThrough(const ILogSiteBase& site) noexcept;
		static void PassThrough(
			const task::TThread* thread,
			array_t<const byte_t> record
		) noexcept;
		static void Write(
			const task::TThread* thread,
			array_t<const byte_t> records,
			u64_t n_overwritten_events,
			u64_t n_dropped_events
		) noexcept;

	public:
		static constexpr TLogFilter DEFAULT_CONSOLE_FILTER = TLogFilter::AtLeastVerbosity(EVerbosity::OPERATIONAL);

		static void RegisterSink(ILogSink* sink);
		static void UnregisterSink(ILogSink* sink);

		/** Enable/disable the process-wide stderr console sink. Enabled by default. */
		static void SetConsoleEnabled(bool enabled);
		static bool ConsoleEnabled();

		/** Configure which records are printed immediately by the default console sink. */
		static void SetConsoleFilter(TLogFilter filter);
		static TLogFilter ConsoleFilter();
	};

	class TFlightRecorder
	{
		class TCountingSink final : public io::stream::ISink<byte_t>
		{
		public:
			usys_t count = 0;

			usys_t Write(const byte_t*, const usys_t n_items_max) final override
			{
				EL_ERROR(n_items_max > NEG1 - count, io::stream::TLimitExceededException);
				count += n_items_max;
				return n_items_max;
			}
		};

		class TRingSink final : public io::stream::ISink<byte_t>
		{
			TFlightRecorder* const recorder;
		public:
			explicit TRingSink(TFlightRecorder* const recorder) : recorder(recorder) {}
			usys_t Write(const byte_t* const data, const usys_t n_items_max) final override;
		};

		std::unique_ptr<byte_t[]> buffer;
		const task::TThread* const thread;
		const usys_t sz_buffer;
		usys_t head = 0;
		usys_t tail = 0;
		usys_t sz_used = 0;
		u64_t n_overwritten_events = 0;
		u64_t n_dropped_events = 0;
		bool discarded = true;

		void ReadBytes(usys_t offset, void* destination, usys_t n_bytes) const noexcept;
		void WriteBytes(const void* source, usys_t n_bytes) noexcept;
		void Reserve(usys_t n_bytes);
		void Clear() noexcept;
		u64_t Timestamp() const noexcept;

	public:
		static usys_t DEFAULT_SIZE_BYTES;

		explicit TFlightRecorder(const task::TThread* thread, usys_t sz_bytes = DEFAULT_SIZE_BYTES);
		TFlightRecorder(const TFlightRecorder&) = delete;
		TFlightRecorder& operator=(const TFlightRecorder&) = delete;
		~TFlightRecorder();

		usys_t Size() const noexcept { return sz_used; }
		usys_t Capacity() const noexcept { return sz_buffer; }
		u64_t OverwrittenEvents() const noexcept { return n_overwritten_events; }
		u64_t DroppedEvents() const noexcept { return n_dropped_events; }

		void Commit() noexcept;
		void Discard() noexcept;

		template<typename ... A>
		void Write(const ILogSiteBase* const site, const void* const call_site, const obj_id_t object_id, A const& ... args) noexcept
		{
			try
			{
				usys_t sz_args = 0;
				if constexpr(sizeof...(A) != 0)
				{
					TCountingSink counter;
					binary::packed::TWriter writer(&counter);
					(io::serialization::Serialize(writer, args), ...);
					sz_args = counter.count;
				}

				if(sz_args > std::numeric_limits<u16_t>::max() || sizeof(TLogRecord) + sz_args > sz_buffer)
				{
					n_dropped_events++;
					discarded = false;
					return;
				}

				const usys_t sz_record = sizeof(TLogRecord) + sz_args;
				Reserve(sz_record);
				const usys_t tail_before = tail;

				try
				{
					TLogRecord record{};
					record.ts = Timestamp();
					record.sz_args = sz_args;
					record.site = site;
					record.call_site = call_site;
					record.object_id = object_id;
					WriteBytes(&record, sizeof(record));

					if constexpr(sizeof...(A) != 0)
					{
						TRingSink sink(this);
						binary::packed::TWriter writer(&sink);
						(io::serialization::Serialize(writer, args), ...);
					}
				}
				catch(...)
				{
					tail = tail_before;
					n_dropped_events++;
					discarded = false;
					return;
				}

				sz_used += sz_record;
				discarded = false;

				if(TLogBook::ShouldPassThrough(*site))
				{
					try
					{
						TList<byte_t> record_data;
						record_data.Clear(sz_record);
						const usys_t n_first = util::Min(sz_record, sz_buffer - tail_before);
						record_data.Append(buffer.get() + tail_before, n_first);
						if(n_first != sz_record)
							record_data.Append(buffer.get(), sz_record - n_first);
						TLogBook::PassThrough(thread, record_data.View());
					}
					catch(...)
					{
					}
				}
			}
			catch(...)
			{
				n_dropped_events++;
				discarded = false;
			}
		}
	};

	template<typename ... A>
	void WriteRecord(const ILogSiteBase* const site, const void* const call_site, const obj_id_t object_id, A const& ... args) noexcept
	{
		task::TThread* const thread = task::TThread::Self();
		if(thread != nullptr)
			thread->FlightRecorder().Write(site, call_site, object_id, args ...);
	}

	template<ECategory CATEGORY, EVerbosity VERBOSITY, TFixedString FORMAT, typename ... A>
	requires (IsValidFormat<std::type_identity_t<std::decay_t<const A>> ...>(FORMAT.data))
	EL_NOINLINE void WriteLog(A const& ... args) noexcept
	{
		using site_t = TLogSite<FORMAT, void, std::type_identity_t<std::decay_t<const A>> ...>;
		static const site_t site(CATEGORY, VERBOSITY);
		const void* const call_site = __builtin_extract_return_addr(__builtin_return_address(0));
		WriteRecord(&site, call_site, 0, args ...);
	}

	template<ECategory CATEGORY, EVerbosity VERBOSITY, TFixedString FORMAT, typename O, typename ... A>
	requires (IsValidFormat<std::type_identity_t<std::decay_t<const A>> ...>(FORMAT.data))
	EL_NOINLINE void WriteLog(const O* const object, A const& ... args) noexcept
	{
		using object_t = std::remove_cv_t<O>;
		using site_t = TLogSite<FORMAT, object_t, std::type_identity_t<std::decay_t<const A>> ...>;
		static const site_t site(CATEGORY, VERBOSITY);
		const void* const call_site = __builtin_extract_return_addr(__builtin_return_address(0));
		WriteRecord(&site, call_site, GetObjectIdentity(object), args ...);
	}
}
