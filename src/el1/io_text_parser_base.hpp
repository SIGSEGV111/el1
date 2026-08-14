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

	/** One completion candidate. replacement replaces the half-open source range [replace_begin, replace_end). */
	struct completion_t
	{
		usys_t replace_begin;
		usys_t replace_end;
		TString replacement;
	};

	/** A range-bound sink passed to dynamic completion providers. Add() accepts complete replacement text, not only a suffix. */
	class TCompletionSink
	{
		TList<completion_t>& completions;
		const usys_t replace_begin;
		const usys_t replace_end;

		friend class TCompletionContext;
		TCompletionSink(TList<completion_t>& completions, const usys_t replace_begin, const usys_t replace_end)
			: completions(completions), replace_begin(replace_begin), replace_end(replace_end) {}

	public:
		bool Add(TString replacement)
		{
			for(const auto& completion : completions)
				if(completion.replace_begin == replace_begin && completion.replace_end == replace_end && completion.replacement == replacement)
					return false;
			completions.Append(completion_t{replace_begin, replace_end, std::move(replacement)});
			return true;
		}

		bool Add(const TStringView replacement) { return Add(TString(replacement)); }
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

	/** Internal result of one completion traversal. A node can be both matched and incomplete, e.g. Maybe()/Repeat() at the cursor. */
	struct TCompletionState
	{
		bool matched;
		bool incomplete;
		usys_t position;

		static constexpr TCompletionState Mismatch(const usys_t position) noexcept { return {false, false, position}; }
		static constexpr TCompletionState Incomplete(const usys_t position) noexcept { return {false, true, position}; }
		static constexpr TCompletionState Matched(const usys_t position, const bool incomplete = false) noexcept { return {true, incomplete, position}; }
	};

	/** Completion-specific state layered over TParseContext. The cursor acts as an artificial EOF even if the source contains a suffix. */
	class TCompletionContext
	{
		TParseContext parse_context;
		TList<completion_t> completions;
		const usys_t cursor;

	public:
		TCompletionContext(stream::IBufferedSource<char32_t>& input EL_LIFETIME_BOUND, const usys_t cursor, const TParseLimits limits = {})
			: parse_context(input, limits), cursor(cursor) {}
		TCompletionContext(const TCompletionContext&) = delete;
		TCompletionContext& operator=(const TCompletionContext&) = delete;

		bool At(const usys_t offset, char32_t& out)
		{
			return offset < cursor && parse_context.At(offset, out);
		}

		bool Ensure(const usys_t count)
		{
			return count <= cursor && parse_context.Ensure(count);
		}

		TStringView Capture(const usys_t begin, const usys_t end)
		{
			EL_ERROR(begin > end || end > cursor, TLogicException);
			return parse_context.Capture(begin, end);
		}

		TCompletionSink Sink(const usys_t replace_begin, const usys_t replace_end)
		{
			EL_ERROR(replace_begin > replace_end || replace_end > cursor, TLogicException);
			return TCompletionSink(completions, replace_begin, replace_end);
		}

		bool Suggest(const usys_t replace_begin, const usys_t replace_end, TString replacement)
		{
			return Sink(replace_begin, replace_end).Add(std::move(replacement));
		}

		bool Suggest(const usys_t replace_begin, const usys_t replace_end, const TStringView replacement)
		{
			return Sink(replace_begin, replace_end).Add(replacement);
		}

		TParseContext& ParseContext() noexcept { return parse_context; }
		const TParseContext& ParseContext() const noexcept { return parse_context; }
		usys_t Cursor() const noexcept { return cursor; }
		usys_t Count() const noexcept { return completions.Count(); }
		TList<completion_t> Take() { return std::move(completions); }
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
