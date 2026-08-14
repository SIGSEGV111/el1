#pragma once

#include "error.hpp"
#include "io_collection_list.hpp"
#include "io_stream.hpp"
#include "io_text_string.hpp"
#include "io_types.hpp"
#include "util.hpp"

#include <concepts>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace el1::io::text::parser
{
	using namespace el1::io::types;
	using namespace el1::io::text::string;
	using namespace el1::io::collection::list;

	/** A committed parser branch failed. Unlike a normal mismatch this must not backtrack. */
	struct TParseException : error::IException
	{
		const usys_t position;

		TString Message() const final override;
		error::IException* Clone() const override;
		explicit TParseException(const usys_t position) : position(position) {}
	};

	struct TParseLimits
	{
		usys_t max_recursion_depth = 256;
	};

	/** Shared state for one parse operation. */
	class TParseContext
	{
		stream::IBufferedSource<char32_t>& input;
		io::collection::array::array_t<const char32_t> buffer;
		TParseLimits limits;
		usys_t recursion_depth = 0;
		usys_t farthest_position = 0;

		struct TRecursiveParserBinding
		{
			const TRecursiveParserBinding* previous;
			const void* rule;
			const void* parser;
		};

		const TRecursiveParserBinding* active_recursive_parser = nullptr;

	public:
		class TRecursionGuard
		{
			TParseContext& context;
			const TRecursionGuard* const previous;
			const void* const rule;
			const usys_t position;

		public:
			TRecursionGuard(TParseContext& context, const void* const rule, const usys_t position)
				: context(context), previous(context.active_recursion), rule(rule), position(position)
			{
				for(const TRecursionGuard* active = previous; active != nullptr; active = active->previous)
					EL_ERROR(active->rule == rule && active->position == position, TLogicException);
				EL_ERROR(context.recursion_depth >= context.limits.max_recursion_depth, TException,
					"parser recursion depth limit exceeded");
				context.active_recursion = this;
				context.recursion_depth++;
			}

			TRecursionGuard(const TRecursionGuard&) = delete;
			TRecursionGuard& operator=(const TRecursionGuard&) = delete;
			TRecursionGuard(TRecursionGuard&&) = delete;
			TRecursionGuard& operator=(TRecursionGuard&&) = delete;

			~TRecursionGuard()
			{
				context.active_recursion = previous;
				context.recursion_depth--;
			}
		};

	private:
		const TRecursionGuard* active_recursion = nullptr;

	public:
		template<typename P>
		class TRecursiveParserGuard
		{
			TParseContext& context;
			const TRecursiveParserBinding binding;

		public:
			TRecursiveParserGuard(TParseContext& context, const void* const rule, const P& parser)
				: context(context), binding{context.active_recursive_parser, rule, &parser}
			{
				context.active_recursive_parser = &binding;
			}

			TRecursiveParserGuard(const TRecursiveParserGuard&) = delete;
			TRecursiveParserGuard& operator=(const TRecursiveParserGuard&) = delete;
			~TRecursiveParserGuard() { context.active_recursive_parser = binding.previous; }
		};

		explicit TParseContext(stream::IBufferedSource<char32_t>& input EL_LIFETIME_BOUND, const TParseLimits limits = {})
			: input(input), buffer(input.Head()), limits(limits) {}
		TParseContext(const TParseContext&) = delete;
		TParseContext& operator=(const TParseContext&) = delete;
		TParseContext(TParseContext&&) = delete;
		TParseContext& operator=(TParseContext&&) = delete;

		bool Ensure(const usys_t count)
		{
			if(buffer.Count() >= count)
				return true;
			const bool available = input.Ensure(count);
			buffer = input.Head();
			EL_ERROR(available && buffer.Count() < count, TLogicException);
			return available;
		}

		bool At(const usys_t offset, char32_t& out)
		{
			if(offset > farthest_position)
				farthest_position = offset;
			if(!Ensure(offset + 1))
				return false;
			out = buffer[offset];
			return true;
		}

		TStringView Capture(const usys_t begin, const usys_t end)
		{
			EL_ERROR(end < begin, TLogicException);
			EL_ERROR(end != 0 && !Ensure(end), TLogicException);
			if(begin == end)
				return {};
			return TStringView::FromUnsafePointer(buffer.ItemPtr(begin), end - begin);
		}

		template<typename P>
		const P* RecursiveParser(const void* const rule) const noexcept
		{
			for(const TRecursiveParserBinding* binding = active_recursive_parser; binding != nullptr; binding = binding->previous)
				if(binding->rule == rule)
					return static_cast<const P*>(binding->parser);
			return nullptr;
		}

		const TParseLimits& Limits() const noexcept { return limits; }
		usys_t FarthestPosition() const noexcept { return farthest_position; }
	};

	inline bool MatchLiteral(TParseContext& input, usys_t& pos, const TStringView literal)
	{
		usys_t p = pos;
		for(const char32_t expected : literal)
		{
			char32_t actual;
			if(!input.At(p, actual) || actual != expected)
				return false;
			p++;
		}
		pos = p;
		return true;
	}

	template<typename N>
	struct TParser;

	struct repeat_t
	{
		usys_t n_min;
		usys_t n_max;
		constexpr repeat_t(const usys_t n) : n_min(n), n_max(n) {}
		constexpr repeat_t(const usys_t n_min, const usys_t n_max) : n_min(n_min), n_max(n_max) {}
	};
}
