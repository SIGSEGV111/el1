#pragma once

#include "io_text_parser_base.hpp"
#include "io_text_parser_ast.hpp"

namespace el1::io::text::parser
{
	template<typename N>
	struct TParser
	{
		using node_t = N;
		using return_t = typename N::return_t;

		N root;
		constexpr explicit TParser(N root) : root(std::move(root)) {}

		/** Sequence two parsers. Both must match in order; on a normal mismatch the whole sequence backtracks.
		 * Discarded results are omitted, equal/list-like results are flattened into a TList, otherwise a tuple is returned. */
		template<typename N2>
		constexpr auto operator+(TParser<N2> rhs) const
		{
			return TParser<ast::TSequenceNode<N, N2>>(ast::TSequenceNode<N, N2>(root, rhs.root));
		}

		/** Ordered alternative. Tries the left parser first and, after a normal mismatch, retries the right parser from the same position.
		 * Committed TParseException failures are propagated and therefore never backtrack to the right alternative. */
		template<typename N2>
		constexpr auto operator||(TParser<N2> rhs) const
		{
			return TParser<ast::TXorNode<N, N2>>(ast::TXorNode<N, N2>(root, rhs.root));
		}

		auto TryParse(TParseContext& context, usys_t& pos) const -> std::optional<return_t>
		{
			usys_t p = pos;
			auto value = root.Parse(context, p);
			if(value)
				pos = p;
			return value;
		}

		auto TryParse(stream::IBufferedSource<char32_t>& input, usys_t& pos, const TParseLimits limits = {}) const -> std::optional<return_t>
		{
			TParseContext context(input, limits);
			return TryParse(context, pos);
		}

		auto TryParsePrefix(stream::IBufferedSource<char32_t>& input, usys_t& consumed, const TParseLimits limits = {}) const -> std::optional<return_t>
		{
			consumed = 0;
			return TryParse(input, consumed, limits);
		}

		auto Parse(const TStringView str, const TParseLimits limits = {}) const -> return_t
		{
			auto input = str.Source();
			TParseContext context(input, limits);
			usys_t pos = 0;
			auto value = TryParse(context, pos);
			EL_ERROR(!value, TException, U"unable to parse");
			char32_t extra;
			EL_ERROR(context.At(pos, extra), TException, TString::Format(U"unable to parse full string - only %d out of %d characters accepted by parser", pos, str.Length()));
			return std::move(*value);
		}


		/** Return completion candidates for input up to cursor. Characters after cursor are ignored and replacement ranges refer to str. */
		auto Complete(const TStringView str, const usys_t cursor, const TParseLimits limits = {}) const -> TList<completion_t>
		{
			EL_ERROR(cursor > str.Length(), TInvalidArgumentException, "cursor", "cursor must not exceed input length");
			auto input = str.Source();
			TCompletionContext context(input, cursor, limits);
			root.Complete(context, 0);
			return context.Take();
		}

		/** Return completion candidates at the end of str. */
		auto Complete(const TStringView str, const TParseLimits limits = {}) const -> TList<completion_t> { return Complete(str, str.Length(), limits); }
	};

	template<typename T, typename F>
	__attribute__((noinline)) auto ast::TRecursiveNode<T, F>::ParseUnbound(TParseContext& context, usys_t& pos) const -> std::optional<return_t>
	{
		const TParser<ast::TRecursiveCallNode<T, F>> self(ast::TRecursiveCallNode<T, F>{this});
		using parser_t = decltype(factory(self));
		static_assert(std::same_as<typename parser_t::return_t, T>, "recursive parser factory must return TParser<T>");
		const parser_t parser = factory(self);
		[[maybe_unused]] typename TParseContext::template TRecursiveParserGuard<parser_t> parser_guard(context, this, parser);
		return parser.TryParse(context, pos);
	}

	template<typename T, typename F>
	__attribute__((noinline)) bool ast::TRecursiveNode<T, F>::MatchUnbound(TParseContext& context, usys_t& pos) const
	{
		const TParser<ast::TRecursiveCallNode<T, F>> self(ast::TRecursiveCallNode<T, F>{this});
		using parser_t = decltype(factory(self));
		static_assert(std::same_as<typename parser_t::return_t, T>, "recursive parser factory must return TParser<T>");
		const parser_t parser = factory(self);
		[[maybe_unused]] typename TParseContext::template TRecursiveParserGuard<parser_t> parser_guard(context, this, parser);
		return parser.root.Match(context, pos);
	}

	template<typename T, typename F>
	__attribute__((noinline)) auto ast::TRecursiveNode<T, F>::CompleteUnbound(TCompletionContext& context, const usys_t pos) const -> TCompletionState
	{
		const TParser<ast::TRecursiveCallNode<T, F>> self(ast::TRecursiveCallNode<T, F>{this});
		using parser_t = decltype(factory(self));
		static_assert(std::same_as<typename parser_t::return_t, T>, "recursive parser factory must return TParser<T>");
		const parser_t parser = factory(self);
		[[maybe_unused]] typename TParseContext::template TRecursiveParserGuard<parser_t> parser_guard(context.ParseContext(), this, parser);
		return parser.root.Complete(context, pos);
	}

	template<typename T, typename F>
	auto ast::TRecursiveNode<T, F>::Parse(TParseContext& context, usys_t& pos) const -> std::optional<return_t>
	{
		[[maybe_unused]] TParseContext::TRecursionGuard recursion(context, this, pos);
		const TParser<ast::TRecursiveCallNode<T, F>> self(ast::TRecursiveCallNode<T, F>{this});
		using parser_t = decltype(factory(self));
		static_assert(std::same_as<typename parser_t::return_t, T>, "recursive parser factory must return TParser<T>");

		if(const parser_t* const parser = context.RecursiveParser<parser_t>(this))
			return parser->TryParse(context, pos);
		return ParseUnbound(context, pos);
	}

	template<typename T, typename F>
	bool ast::TRecursiveNode<T, F>::Match(TParseContext& context, usys_t& pos) const
	{
		[[maybe_unused]] TParseContext::TRecursionGuard recursion(context, this, pos);
		const TParser<ast::TRecursiveCallNode<T, F>> self(ast::TRecursiveCallNode<T, F>{this});
		using parser_t = decltype(factory(self));
		static_assert(std::same_as<typename parser_t::return_t, T>, "recursive parser factory must return TParser<T>");

		if(const parser_t* const parser = context.RecursiveParser<parser_t>(this))
			return parser->root.Match(context, pos);
		return MatchUnbound(context, pos);
	}

	template<typename T, typename F>
	auto ast::TRecursiveNode<T, F>::Complete(TCompletionContext& context, const usys_t pos) const -> TCompletionState
	{
		[[maybe_unused]] TParseContext::TRecursionGuard recursion(context.ParseContext(), this, pos);
		const TParser<ast::TRecursiveCallNode<T, F>> self(ast::TRecursiveCallNode<T, F>{this});
		using parser_t = decltype(factory(self));
		static_assert(std::same_as<typename parser_t::return_t, T>, "recursive parser factory must return TParser<T>");

		if(const parser_t* const parser = context.ParseContext().RecursiveParser<parser_t>(this))
			return parser->root.Complete(context, pos);
		return CompleteUnbound(context, pos);
	}

	/** Match exactly one UTF-32 character and return that character. */
	constexpr TParser<ast::TCharListNode<1>> operator""_P(const char32_t chr)
	{
		return TParser(ast::TCharListNode<1>(chr));
	}

	/** Match an exact UTF-32 string literal and return the matched literal as a TString. */
	constexpr TParser<ast::TLiteralNode> operator""_P(const char32_t* const str, const size_t len)
	{
		return TParser(ast::TLiteralNode(TStringView::FromUnsafePointer(str, len)));
	}

	/** Match zero or one occurrence and return it as std::optional<T>. The parser always succeeds unless the wrapped parser raises a committed error. */
	template<typename N> constexpr auto Maybe(TParser<N> parser) { return TParser(ast::TMaybeNode<N>{std::move(parser.root)}); }

	/** Match between n_min and n_max occurrences and return their results as a TList. Each successful occurrence must consume input. */
	template<typename N> constexpr auto Repeat(const usys_t n_min, const usys_t n_max, TParser<N> parser) { return TParser(ast::TRepeatNode<N>{std::move(parser.root), n_min, n_max}); }

	/** Match one or more occurrences and return their results as a TList. */
	template<typename N> constexpr auto OneOrMore(TParser<N> parser) { return TParser(ast::TRepeatNode<N>{parser.root, 1, NEG1}); }

	/** Match and return one character in the inclusive range [from, to]. */
	constexpr auto CharRange(const char32_t from, const char32_t to) { return TParser(ast::TCharRangeNode(from, to)); }

	/** Match and return one character equal to any of the supplied characters. */
	template<typename... A> constexpr auto CharList(A... a) { return TParser(ast::TCharListNode<sizeof...(a)>(a...)); }

	/** Conditionally enable a parser branch. If condition is false the node always produces a normal, non-consuming mismatch. */
	template<typename N> constexpr auto If(const bool condition, TParser<N> parser) { return TParser(ast::TIfNode<N>{condition, std::move(parser.root)}); }

	/** Complement a single-character matcher. It consumes and returns one character exactly when the wrapped matcher would reject it. */
	template<typename N> requires ast::is_char_matcher_v<N> constexpr auto operator~(TParser<N> parser) { return TParser(ast::TCharNotNode<N>{std::move(parser.root)}); }

	/** Define one first-character dispatch case. The matcher selects the case without consuming; parser performs the actual parse. */
	template<typename M, typename N> requires ast::is_char_matcher_v<M> constexpr auto Case(TParser<M> matcher, TParser<N> parser) { return ast::TDispatchCase<M,N>{std::move(matcher.root), std::move(parser.root)}; }

	/** Select exactly one parser from ordered Case() entries by inspecting the next character once. All cases must return the same type. */
	template<typename... C> constexpr auto Dispatch(C... cases) { return TParser(ast::TDispatchNode<C...>{std::tuple<C...>(std::move(cases)...)}); }

	/** Filter a successful parser result with predicate. A false predicate becomes a normal mismatch and allows alternatives to backtrack. */
	template<typename P, typename N> constexpr auto Where(P predicate, TParser<N> parser) { return TParser(ast::TWhereNode<P,N>{std::move(predicate), std::move(parser.root)}); }

	/** Validate a successful parser result with predicate. A false predicate raises TParseException and therefore commits the failure. */
	template<typename P, typename N> constexpr auto Validate(P predicate, TParser<N> parser) { return TParser(ast::TValidateNode<P,N>{std::move(predicate), std::move(parser.root)}); }

	/** Require parser to match at the current position. A normal mismatch is promoted to a committed TParseException. */
	template<typename N> constexpr auto Expect(TParser<N> parser) { return TParser(ast::TExpectNode<N>{std::move(parser.root)}); }

	/** Match only at end of input. It consumes nothing and returns a discarded result. */
	constexpr auto End() { return TParser(ast::TEndNode{}); }

	/** Parse without consuming input. The wrapped parser result is returned on success while the input position remains unchanged. */
	template<typename N> constexpr auto LookAhead(TParser<N> parser) { return TParser(ast::TLookAheadNode<N>{std::move(parser.root)}); }

	/** Parse all supplied parsers in sequence and pass their typed results as separate arguments to lambda. */
	template<typename L, typename... N> constexpr auto Translate(L lambda, TParser<N>... parsers) { return TParser(ast::TTranslateNode<L,N...>(std::move(lambda), std::tuple<N...>(std::move(parsers.root)...))); }

	/** Like Translate(), but lambda returns std::optional<T>; std::nullopt becomes a normal mismatch and permits backtracking. */
	template<typename L, typename... N> constexpr auto TryTranslate(L lambda, TParser<N>... parsers) { return TParser(ast::TTryTranslateNode<L,N...>{std::move(lambda), std::tuple<N...>(std::move(parsers.root)...)}); }

	/** Return the exact matched source text as a borrowed, zero-copy TStringView instead of materializing the wrapped parser result. */
	template<typename N> constexpr auto Capture(TParser<N> parser) { return TParser(ast::TCaptureNode<N>{std::move(parser.root)}); }

	/** Attach dynamic completion to parser. provider(prefix, sink) receives the source prefix from this node's start to the cursor and adds full replacement candidates through sink. */
	template<typename N, typename P>
	requires std::invocable<const std::decay_t<P>&, TStringView, TCompletionSink&>
	constexpr auto WithCompletion(TParser<N> parser, P&& provider)
	{
		using provider_t = std::decay_t<P>;
		return TParser(ast::TWithCompletionNode<provider_t, N>{std::forward<P>(provider), std::move(parser.root)});
	}

	/** Translate parser results by allocating concrete T and return it as std::unique_ptr<C> through New<T,C>(). */
	template<typename T, typename C, typename... N> constexpr auto TranslateCast(TParser<N>... parsers) { return Translate([](auto... a){ return New<T,C>(a...); }, parsers...); }

	/** Translate parser results by directly constructing T from them. */
	template<typename T, typename... N> constexpr auto Translate(TParser<N>... parsers) { return Translate([](auto... a){ return T(a...); }, parsers...); }

	/** Match parser but discard its semantic result so it does not contribute to sequence results. */
	template<typename N> constexpr auto Discard(TParser<N> parser) { return TParser(ast::TDiscardNode<N>{parser.root}); }

	/** Define a recursive grammar. factory receives a typed parser referring back to this rule; left/nullable recursion at the same position is rejected. */
	template<typename T, typename F>
	constexpr auto Recursive(F&& factory)
	{
		using TFactory = std::decay_t<F>;
		return TParser(ast::TRecursiveNode<T, TFactory>{std::forward<F>(factory)});
	}

	/** Match a list of parser values separated by separator, with an optional minimum/maximum item count. A consumed separator requires a following value. */
	template<typename N, typename S>
	constexpr auto SeparatedBy(TParser<N> parser, TParser<S> separator, const usys_t n_min = 0, const usys_t n_max = NEG1)
	{
		return TParser(ast::TSeparatedByNode<N, S>{parser.root, separator.root, n_min, n_max});
	}

	/** Match open, parser, close in sequence, discard both delimiters, and return only parser's semantic result. */
	template<typename O, typename N, typename C>
	constexpr auto Between(TParser<O> open, TParser<N> parser, TParser<C> close)
	{
		return Discard(std::move(open)) + std::move(parser) + Discard(std::move(close));
	}

}
