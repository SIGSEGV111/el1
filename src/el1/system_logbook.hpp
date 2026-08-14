#pragma once

#include "io_text_string.hpp"

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

	struct ILogRecord
	{

		virtual TString RenderMessage() const = 0;
	};

	/*
	 * Return Address == Call-Site
	 * Category
	 * Message Text
	 */

	template<typename O, typename ... A>
	static void WriteLog(const ECategory category, const O* const object, const io::text::format::TFormatString<std::type_identity_t<std::decay_t<const A>>...> format, A const& ...args)
	{
		struct TRecord : ILogRecord
		{
			const TStringView format;
			const std::tuple<A...> args;

			TString RenderMessage() const final override
			{
				return std::apply(
					[this](auto const&... args)
					{
						return TString::Format(format, args...);
					},
					args
				);
			}

			TRecord(
				TStringView _format,
				std::tuple<A...> _args
			) : format(std::move(_format)), args(std::move(_args)) {}
		};

		(void)New<TRecord>(TStringView::FromUnsafePointer(format.literal, format.length), std::tuple<A...>(std::move(args)...));
	}
}
