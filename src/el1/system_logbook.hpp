#pragma once

#include "io_text_string.hpp"
#include "debug.hpp"

namespace el1::system::logbook
{
	using namespace io::text::string;

	enum class ECategory
	{
		LIVENESS,
		STATE_CHANGE,
		DEGRADED,
		PERFORMANCE,
		PROGRESS,
		EXCEPTION
	};

	struct TLogRecord
	{
		TString msg;
		u64_t ts;	// EClock::MONOTONIC in nanoseconds
		const void* ip;	// instruction pointer of log call site
		ECategory category;
	};

	template<typename T>
	TString GetObjectIdentity(const T* const object)
	{
		constexpr debug::TTypeNameView type_name = debug::GetTypeName<T>();
		return TString::Format(U"%s@%x", TString(type_name.data, type_name.length), reinterpret_cast<usys_t>(object));
	}

	template<typename O, typename ... A>
	[[gnu::noinline]] static void WriteLog(const ECategory category, const O* const object, const io::text::format::TFormatString<std::type_identity_t<std::decay_t<const A>>...> format, A const& ...args)
	{
		TLogRecord record;
		if(object)
		{
			constexpr io::text::format::TFormatString<TString> object_prefix(U"[%s]");
			record.msg = TString::Format(object_prefix + format, GetObjectIdentity(object), args...);
		}
		else
			record.msg = TString::Format(format, args...);
		record.ip = __builtin_extract_return_addr(__builtin_return_address(0));
		record.category = category;
	}
}
