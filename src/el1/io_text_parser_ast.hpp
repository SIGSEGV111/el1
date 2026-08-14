#pragma once

#include "io_text_parser_base.hpp"

namespace el1::io::text::parser::ast
{
	template<typename T> struct arrayify_t { using type = TList<T>; };
	template<typename T> struct arrayify_t<TList<T>> { using type = TList<T>; };

	template<typename T> struct optional_value_t;
	template<typename T> struct optional_value_t<std::optional<T>> { using type = T; };

	struct discard_t {};

	template<typename N>
	struct TIfNode
	{
		using return_t = typename N::return_t;
		bool condition;
		N node;

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			if(!condition)
				return std::nullopt;
			return node.Parse(input, pos);
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			return condition && node.Match(input, pos);
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			return condition ? node.Complete(input, pos) : TCompletionState::Mismatch(pos);
		}

		constexpr bool Matches(const char32_t chr) const noexcept
			requires requires { { node.Matches(chr) } -> std::convertible_to<bool>; }
		{
			return condition && node.Matches(chr);
		}
	};

	template<typename P, typename N>
	struct TWhereNode
	{
		using return_t = typename N::return_t;
		[[no_unique_address]] P predicate;
		N node;

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			usys_t p = pos;
			auto value = node.Parse(input, p);
			if(!value || !predicate(*value))
				return std::nullopt;
			pos = p;
			return value;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			return Parse(input, pos).has_value();
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			auto state = node.Complete(input, pos);
			if(!state.matched)
				return state;

			usys_t p = pos;
			auto value = node.Parse(input.ParseContext(), p);
			EL_ERROR(!value || p != state.position, TLogicException);
			if(!predicate(*value))
				return {false, state.incomplete, pos};
			return state;
		}
	};

	template<typename P, typename N>
	struct TValidateNode
	{
		using return_t = typename N::return_t;
		[[no_unique_address]] P predicate;
		N node;

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			const usys_t begin = pos;
			usys_t p = pos;
			auto value = node.Parse(input, p);
			if(!value)
				return std::nullopt;
			if(!predicate(*value))
				EL_THROW(TParseException, begin);
			pos = p;
			return value;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			return Parse(input, pos).has_value();
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			auto state = node.Complete(input, pos);
			if(!state.matched)
				return state;

			usys_t p = pos;
			auto value = node.Parse(input.ParseContext(), p);
			EL_ERROR(!value || p != state.position, TLogicException);
			if(!predicate(*value))
			{
				if(state.incomplete)
					return {false, true, pos};
				EL_THROW(TParseException, pos);
			}
			return state;
		}
	};

	template<typename N>
	struct TExpectNode
	{
		using return_t = typename N::return_t;
		N node;

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			const usys_t begin = pos;
			const usys_t farthest_before = input.FarthestPosition();
			usys_t p = pos;
			auto value = node.Parse(input, p);
			if(!value)
			{
				const usys_t farthest_after = input.FarthestPosition();
				EL_THROW(TParseException, farthest_after > farthest_before ? farthest_after : begin);
			}
			pos = p;
			return value;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			const usys_t begin = pos;
			const usys_t farthest_before = input.FarthestPosition();
			usys_t p = pos;
			if(!node.Match(input, p))
			{
				const usys_t farthest_after = input.FarthestPosition();
				EL_THROW(TParseException, farthest_after > farthest_before ? farthest_after : begin);
			}
			pos = p;
			return true;
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			auto state = node.Complete(input, pos);
			if(state.matched || state.incomplete)
				return state;
			EL_THROW(TParseException, pos);
		}
	};

	struct TEndNode
	{
		using return_t = discard_t;

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			char32_t chr;
			return input.At(pos, chr) ? std::nullopt : std::optional<return_t>(discard_t{});
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			char32_t chr;
			return !input.At(pos, chr);
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			char32_t chr;
			return input.At(pos, chr) ? TCompletionState::Mismatch(pos) : TCompletionState::Matched(pos);
		}
	};

	struct TCharRangeNode
	{
		using return_t = char32_t;
		char32_t from;
		char32_t to;
		constexpr TCharRangeNode(const char32_t from, const char32_t to) : from(from), to(to) {}

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			char32_t chr;
			if(!input.At(pos, chr) || !Matches(chr))
				return std::nullopt;
			pos++;
			return chr;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			char32_t chr;
			if(!input.At(pos, chr) || !Matches(chr))
				return false;
			pos++;
			return true;
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			char32_t chr;
			if(!input.At(pos, chr))
				return TCompletionState::Incomplete(pos);
			return Matches(chr) ? TCompletionState::Matched(pos + 1) : TCompletionState::Mismatch(pos);
		}

		constexpr bool Matches(const char32_t chr) const noexcept { return chr >= from && chr <= to; }
	};

	template<usys_t N>
	struct TCharListNode
	{
		using return_t = char32_t;
		char32_t list[N];
		template<typename... A>
		constexpr TCharListNode(A... a) : list{static_cast<char32_t>(a)...} {}

		constexpr bool Matches(const char32_t chr) const noexcept
		{
			for(const char32_t item : list)
				if(item == chr)
					return true;
			return false;
		}

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			char32_t chr;
			if(!input.At(pos, chr) || !Matches(chr))
				return std::nullopt;
			pos++;
			return chr;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			char32_t chr;
			if(!input.At(pos, chr) || !Matches(chr))
				return false;
			pos++;
			return true;
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			char32_t chr;
			if(input.At(pos, chr))
				return Matches(chr) ? TCompletionState::Matched(pos + 1) : TCompletionState::Mismatch(pos);

			for(const char32_t item : list)
			{
				const char32_t replacement[] = {item};
				input.Suggest(pos, pos, TStringView::FromUnsafePointer(replacement, 1));
			}
			return TCompletionState::Incomplete(pos);
		}
	};

	struct TLiteralNode
	{
		using return_t = TString;
		TStringView literal;
		constexpr explicit TLiteralNode(const TStringView literal) : literal(literal) {}

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			usys_t p = pos;
			if(!MatchLiteral(input, p, literal))
				return std::nullopt;
			pos = p;
			return TString(literal);
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			usys_t p = pos;
			if(!MatchLiteral(input, p, literal))
				return false;
			pos = p;
			return true;
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			usys_t p = pos;
			for(const char32_t expected : literal)
			{
				char32_t actual;
				if(!input.At(p, actual))
				{
					input.Suggest(pos, p, literal);
					return TCompletionState::Incomplete(pos);
				}
				if(actual != expected)
					return TCompletionState::Mismatch(pos);
				p++;
			}
			return TCompletionState::Matched(p);
		}
	};

	template<typename L, typename... N>
	struct TTranslateNode
	{
		using return_t = std::invoke_result_t<L, typename N::return_t...>;
		using values_t = std::tuple<std::optional<typename N::return_t>...>;
		L lambda;
		std::tuple<N...> nodes;
		constexpr TTranslateNode(L lambda, std::tuple<N...> nodes) : lambda(std::move(lambda)), nodes(std::move(nodes)) {}

		template<usys_t I = 0>
		bool ParseNodes(TParseContext& input, usys_t& pos, values_t& values) const
		{
			if constexpr(I == sizeof...(N))
				return true;
			else
			{
				auto value = std::get<I>(nodes).Parse(input, pos);
				if(!value)
					return false;
				std::get<I>(values) = std::move(value);
				return ParseNodes<I + 1>(input, pos, values);
			}
		}

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			usys_t p = pos;
			values_t values;
			if(!ParseNodes(input, p, values))
				return std::nullopt;
			auto result = std::apply([&](auto&... value) { return lambda(std::move(*value)...); }, values);
			pos = p;
			return result;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			return Parse(input, pos).has_value();
		}

		template<usys_t I = 0>
		auto CompleteNodes(TCompletionContext& input, const usys_t pos, const bool incomplete = false) const -> TCompletionState
		{
			if constexpr(I == sizeof...(N))
				return TCompletionState::Matched(pos, incomplete);
			else
			{
				auto state = std::get<I>(nodes).Complete(input, pos);
				if(!state.matched)
					return {false, incomplete || state.incomplete, pos};
				return CompleteNodes<I + 1>(input, state.position, incomplete || state.incomplete);
			}
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			return CompleteNodes(input, pos);
		}
	};

	template<typename L, typename... N>
	struct TTryTranslateNode
	{
		using optional_t = std::invoke_result_t<L, typename N::return_t...>;
		using return_t = typename optional_value_t<optional_t>::type;
		using values_t = std::tuple<std::optional<typename N::return_t>...>;
		[[no_unique_address]] L lambda;
		std::tuple<N...> nodes;

		template<usys_t I = 0>
		bool ParseNodes(TParseContext& input, usys_t& pos, values_t& values) const
		{
			if constexpr(I == sizeof...(N))
				return true;
			else
			{
				auto value = std::get<I>(nodes).Parse(input, pos);
				if(!value)
					return false;
				std::get<I>(values) = std::move(value);
				return ParseNodes<I + 1>(input, pos, values);
			}
		}

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			usys_t p = pos;
			values_t values;
			if(!ParseNodes(input, p, values))
				return std::nullopt;
			auto result = std::apply([&](auto&... value) { return lambda(std::move(*value)...); }, values);
			if(!result)
				return std::nullopt;
			pos = p;
			return result;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			return Parse(input, pos).has_value();
		}

		template<usys_t I = 0>
		auto CompleteNodes(TCompletionContext& input, const usys_t pos, const bool incomplete = false) const -> TCompletionState
		{
			if constexpr(I == sizeof...(N))
				return TCompletionState::Matched(pos, incomplete);
			else
			{
				auto state = std::get<I>(nodes).Complete(input, pos);
				if(!state.matched)
					return {false, incomplete || state.incomplete, pos};
				return CompleteNodes<I + 1>(input, state.position, incomplete || state.incomplete);
			}
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			auto state = CompleteNodes(input, pos);
			if(!state.matched)
				return state;

			usys_t p = pos;
			if(!Parse(input.ParseContext(), p))
				return {false, state.incomplete, pos};
			EL_ERROR(p != state.position, TLogicException);
			return state;
		}
	};

	template<typename N>
	struct TCaptureNode
	{
		using return_t = TStringView;
		N node;

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			const usys_t begin = pos;
			usys_t p = pos;
			if(!node.Match(input, p))
				return std::nullopt;
			auto captured = input.Capture(begin, p);
			pos = p;
			return captured;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			return node.Match(input, pos);
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			return node.Complete(input, pos);
		}
	};

	template<typename N>
	struct TRepeatNode
	{
		using T = typename N::return_t;
		using return_t = typename arrayify_t<T>::type;
		N node;
		usys_t n_min;
		usys_t n_max;

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			return_t result;
			usys_t p = pos;
			for(usys_t i = 0; i < n_max; i++)
			{
				usys_t next = p;
				auto value = node.Parse(input, next);
				if(!value)
				{
					if(i < n_min)
						return std::nullopt;
					break;
				}
				EL_ERROR(next == p, TLogicException);
				p = next;
				result.Append(std::move(*value));
			}
			pos = p;
			return result;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			usys_t p = pos;
			for(usys_t i = 0; i < n_max; i++)
			{
				usys_t next = p;
				if(!node.Match(input, next))
				{
					if(i < n_min)
						return false;
					break;
				}
				EL_ERROR(next == p, TLogicException);
				p = next;
			}
			pos = p;
			return true;
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			usys_t p = pos;
			bool incomplete = false;
			usys_t count = 0;
			for(; count < n_max; count++)
			{
				auto state = node.Complete(input, p);
				incomplete = incomplete || state.incomplete;
				if(!state.matched)
					return count >= n_min ? TCompletionState::Matched(p, incomplete) : TCompletionState{false, incomplete, pos};
				EL_ERROR(state.position == p, TLogicException);
				p = state.position;
			}
			return count >= n_min ? TCompletionState::Matched(p, incomplete) : TCompletionState{false, incomplete, pos};
		}
	};

	template<typename N>
	struct TDiscardNode
	{
		using return_t = discard_t;
		N node;
		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			usys_t p = pos;
			if(!node.Match(input, p))
				return std::nullopt;
			pos = p;
			return discard_t{};
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			return node.Match(input, pos);
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			return node.Complete(input, pos);
		}
	};

	template<typename N>
	struct TLookAheadNode
	{
		using return_t = typename N::return_t;
		N node;

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			usys_t p = pos;
			return node.Parse(input, p);
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			usys_t p = pos;
			return node.Match(input, p);
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			auto state = node.Complete(input, pos);
			if(state.matched)
				state.position = pos;
			return state;
		}
	};

	template<typename N1, typename N2>
	struct TSequenceNode
	{
		using T1 = typename N1::return_t;
		using T2 = typename N2::return_t;
		using A1 = typename arrayify_t<T1>::type;
		using A2 = typename arrayify_t<T2>::type;
		using return_t = typename std::conditional<std::is_same_v<T1, discard_t> && std::is_same_v<T2, discard_t>, discard_t,
			typename std::conditional<std::is_same_v<T1, discard_t>, T2,
				typename std::conditional<std::is_same_v<T2, discard_t>, T1,
					typename std::conditional<std::is_same_v<A1, A2>, A1, std::tuple<T1, T2>>::type>::type>::type>::type;

		N1 lhs;
		N2 rhs;
		constexpr TSequenceNode(N1 lhs, N2 rhs) : lhs(std::move(lhs)), rhs(std::move(rhs)) {}

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			usys_t p = pos;
			auto v_lhs = lhs.Parse(input, p);
			if(!v_lhs)
				return std::nullopt;
			auto v_rhs = rhs.Parse(input, p);
			if(!v_rhs)
				return std::nullopt;

			pos = p;
			if constexpr(std::is_same_v<T1, discard_t> && std::is_same_v<T2, discard_t>)
				return discard_t{};
			else if constexpr(std::is_same_v<T1, discard_t>)
				return std::move(*v_rhs);
			else if constexpr(std::is_same_v<T2, discard_t>)
				return std::move(*v_lhs);
			else if constexpr(std::is_same_v<T1, A1> || std::is_same_v<T2, A2>)
			{
				if constexpr(std::is_same_v<T1, A1>)
				{
					(*v_lhs).MoveAppend(std::move(*v_rhs));
					return std::move(*v_lhs);
				}
				else
				{
					(*v_rhs).MoveInsert(0, std::move(*v_lhs));
					return std::move(*v_rhs);
				}
			}
			else if constexpr(std::is_same_v<T1, T2>)
			{
				TList<T1> list(2);
				list.MoveAppend(std::move(*v_lhs));
				list.MoveAppend(std::move(*v_rhs));
				return list;
			}
			else
				return std::tuple<T1, T2>(std::move(*v_lhs), std::move(*v_rhs));
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			usys_t p = pos;
			if(!lhs.Match(input, p) || !rhs.Match(input, p))
				return false;
			pos = p;
			return true;
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			auto lhs_state = lhs.Complete(input, pos);
			if(!lhs_state.matched)
				return lhs_state;
			auto rhs_state = rhs.Complete(input, lhs_state.position);
			return {rhs_state.matched, lhs_state.incomplete || rhs_state.incomplete, rhs_state.matched ? rhs_state.position : pos};
		}
	};

	template<typename N1, typename N2>
	struct TXorNode
	{
		using T1 = typename N1::return_t;
		using T2 = typename N2::return_t;
		using A1 = typename arrayify_t<T1>::type;
		using A2 = typename arrayify_t<T2>::type;
		using return_t = typename std::conditional<std::is_same_v<T1, T2>, T1,
			typename std::conditional<std::is_same_v<A1, A2>, A1, std::variant<T1, T2>>::type>::type;
		N1 lhs;
		N2 rhs;
		constexpr TXorNode(N1 lhs, N2 rhs) : lhs(std::move(lhs)), rhs(std::move(rhs)) {}

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			if constexpr(requires(char32_t chr) { { lhs.Matches(chr) } -> std::convertible_to<bool>; { rhs.Matches(chr) } -> std::convertible_to<bool>; })
			{
				static_assert(std::same_as<return_t, char32_t>);
				char32_t chr;
				if(!input.At(pos, chr) || !Matches(chr))
					return std::nullopt;
				pos++;
				return chr;
			}

			usys_t p = pos;
			if(auto value = lhs.Parse(input, p))
			{
				pos = p;
				return return_t(std::move(*value));
			}
			p = pos;
			if(auto value = rhs.Parse(input, p))
			{
				pos = p;
				return return_t(std::move(*value));
			}
			return std::nullopt;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			if constexpr(requires(char32_t chr) { { lhs.Matches(chr) } -> std::convertible_to<bool>; { rhs.Matches(chr) } -> std::convertible_to<bool>; })
			{
				char32_t chr;
				if(!input.At(pos, chr) || !Matches(chr))
					return false;
				pos++;
				return true;
			}

			usys_t p = pos;
			if(lhs.Match(input, p))
			{
				pos = p;
				return true;
			}
			p = pos;
			if(rhs.Match(input, p))
			{
				pos = p;
				return true;
			}
			return false;
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			auto lhs_state = lhs.Complete(input, pos);
			if(lhs_state.matched)
				return lhs_state;

			auto rhs_state = rhs.Complete(input, pos);
			if(rhs_state.matched)
				return {true, lhs_state.incomplete || rhs_state.incomplete, rhs_state.position};
			return {false, lhs_state.incomplete || rhs_state.incomplete, pos};
		}

		constexpr bool Matches(const char32_t chr) const noexcept
			requires requires { { lhs.Matches(chr) } -> std::convertible_to<bool>; { rhs.Matches(chr) } -> std::convertible_to<bool>; }
		{
			return lhs.Matches(chr) || rhs.Matches(chr);
		}
	};

	template<typename N>
	struct TCharNotNode
	{
		using return_t = char32_t;
		N node;

		constexpr bool Matches(const char32_t chr) const noexcept { return !node.Matches(chr); }

		auto Parse(TParseContext& input, usys_t& pos) const -> std::optional<return_t>
		{
			char32_t chr;
			if(!input.At(pos, chr) || !Matches(chr))
				return std::nullopt;
			pos++;
			return chr;
		}

		bool Match(TParseContext& input, usys_t& pos) const
		{
			char32_t chr;
			if(!input.At(pos, chr) || !Matches(chr))
				return false;
			pos++;
			return true;
		}

		auto Complete(TCompletionContext& input, const usys_t pos) const -> TCompletionState
		{
			char32_t chr;
			if(!input.At(pos, chr))
				return TCompletionState::Incomplete(pos);
			return Matches(chr) ? TCompletionState::Matched(pos + 1) : TCompletionState::Mismatch(pos);
		}
	};

	template<typename N> struct is_char_matcher_t : std::false_type {};
	template<> struct is_char_matcher_t<TCharRangeNode> : std::true_type {};
	template<usys_t N> struct is_char_matcher_t<TCharListNode<N>> : std::true_type {};
	template<typename N> struct is_char_matcher_t<TIfNode<N>> : is_char_matcher_t<N> {};
	template<typename N1, typename N2> struct is_char_matcher_t<TXorNode<N1, N2>>
		: std::bool_constant<is_char_matcher_t<N1>::value && is_char_matcher_t<N2>::value> {};
	template<typename N> struct is_char_matcher_t<TCharNotNode<N>> : is_char_matcher_t<N> {};
	template<typename N> inline constexpr bool is_char_matcher_v = is_char_matcher_t<N>::value;

	template<typename M, typename N>
	struct TDispatchCase
	{
		using return_t = typename N::return_t;
		M matcher;
		N node;
	};

	template<typename... C>
	struct TDispatchNode
	{
		static_assert(sizeof...(C) > 0);
		using first_case_t = std::tuple_element_t<0, std::tuple<C...>>;
		using return_t = typename first_case_t::return_t;
		static_assert((std::same_as<return_t, typename C::return_t> && ...), "all Dispatch cases must return the same type");
		std::tuple<C...> cases;

	private:
		template<usys_t I = 0>
		auto ParseCase(const char32_t chr, TParseContext& context, usys_t& pos) const -> std::optional<return_t>
		{
			if constexpr(I == sizeof...(C))
			{
				return std::nullopt;
			}
			else
			{
				const auto& entry = std::get<I>(cases);
				if(entry.matcher.Matches(chr))
					return entry.node.Parse(context, pos);
				return ParseCase<I + 1>(chr, context, pos);
			}
		}

		template<usys_t I = 0>
		bool MatchCase(const char32_t chr, TParseContext& context, usys_t& pos) const
		{
			if constexpr(I == sizeof...(C))
			{
				return false;
			}
			else
			{
				const auto& entry = std::get<I>(cases);
				if(entry.matcher.Matches(chr))
					return entry.node.Match(context, pos);
				return MatchCase<I + 1>(chr, context, pos);
			}
		}

		template<usys_t I = 0>
		auto CompleteCase(const char32_t chr, TCompletionContext& context, const usys_t pos) const -> TCompletionState
		{
			if constexpr(I == sizeof...(C))
			{
				return TCompletionState::Mismatch(pos);
			}
			else
			{
				const auto& entry = std::get<I>(cases);
				if(entry.matcher.Matches(chr))
					return entry.node.Complete(context, pos);
				return CompleteCase<I + 1>(chr, context, pos);
			}
		}

		template<usys_t I = 0>
		bool CompleteAll(TCompletionContext& context, const usys_t pos) const
		{
			if constexpr(I == sizeof...(C))
				return false;
			else
			{
				const auto& entry = std::get<I>(cases);
				const auto state = entry.node.Complete(context, pos);
				const bool rest = CompleteAll<I + 1>(context, pos);
				return state.matched || state.incomplete || rest;
			}
		}

	public:
		auto Parse(TParseContext& context, usys_t& pos) const -> std::optional<return_t>
		{
			char32_t chr;
			if(!context.At(pos, chr))
				return std::nullopt;
			return ParseCase(chr, context, pos);
		}

		bool Match(TParseContext& context, usys_t& pos) const
		{
			char32_t chr;
			return context.At(pos, chr) && MatchCase(chr, context, pos);
		}

		auto Complete(TCompletionContext& context, const usys_t pos) const -> TCompletionState
		{
			char32_t chr;
			if(context.At(pos, chr))
				return CompleteCase(chr, context, pos);
			return CompleteAll(context, pos) ? TCompletionState::Incomplete(pos) : TCompletionState::Mismatch(pos);
		}
	};

	template<typename P, typename N>
	struct TWithCompletionNode
	{
		using return_t = typename N::return_t;
		[[no_unique_address]] P provider;
		N node;

		auto Parse(TParseContext& context, usys_t& pos) const -> std::optional<return_t>
		{
			return node.Parse(context, pos);
		}

		bool Match(TParseContext& context, usys_t& pos) const
		{
			return node.Match(context, pos);
		}

		auto Complete(TCompletionContext& context, const usys_t pos) const -> TCompletionState
		{
			auto state = node.Complete(context, pos);
			const usys_t cursor = context.Cursor();
			if(cursor < pos || !(state.incomplete || (state.matched && state.position == cursor)))
				return state;

			const usys_t before = context.Count();
			auto sink = context.Sink(pos, cursor);
			provider(context.Capture(pos, cursor), sink);
			if(context.Count() != before)
				state.incomplete = true;
			return state;
		}
	};

	template<typename T, typename F>
	struct TRecursiveNode;

	template<typename T, typename F>
	struct TRecursiveCallNode
	{
		using return_t = T;
		const TRecursiveNode<T, F>* owner;

		auto Parse(TParseContext& context, usys_t& pos) const -> std::optional<return_t>
		{
			return owner->Parse(context, pos);
		}

		bool Match(TParseContext& context, usys_t& pos) const
		{
			return owner->Match(context, pos);
		}

		auto Complete(TCompletionContext& context, const usys_t pos) const -> TCompletionState
		{
			return owner->Complete(context, pos);
		}
	};

	template<typename T, typename F>
	struct TRecursiveNode
	{
		using return_t = T;
		[[no_unique_address]] F factory;

	private:
		// Keep construction of the potentially large recursive grammar out of the
		// recursive hot path. It is needed only once per parse context.
		__attribute__((noinline)) auto ParseUnbound(TParseContext& context, usys_t& pos) const -> std::optional<return_t>;
		__attribute__((noinline)) bool MatchUnbound(TParseContext& context, usys_t& pos) const;
		__attribute__((noinline)) auto CompleteUnbound(TCompletionContext& context, usys_t pos) const -> TCompletionState;

	public:
		auto Parse(TParseContext& context, usys_t& pos) const -> std::optional<return_t>;
		bool Match(TParseContext& context, usys_t& pos) const;
		auto Complete(TCompletionContext& context, usys_t pos) const -> TCompletionState;
	};

	template<typename N, typename S>
	struct TSeparatedByNode
	{
		using T = typename N::return_t;
		using return_t = TList<T>;
		N node;
		S separator;
		usys_t n_min;
		usys_t n_max;

		auto Parse(TParseContext& context, usys_t& pos) const -> std::optional<return_t>
		{
			return_t result;
			usys_t p = pos;
			if(n_max == 0)
			{
				if(n_min != 0)
					return std::nullopt;
				return result;
			}

			usys_t next = p;
			auto first = node.Parse(context, next);
			if(!first)
			{
				if(n_min != 0)
					return std::nullopt;
				return result;
			}
			EL_ERROR(next == p, TLogicException);
			p = next;
			result.Append(std::move(*first));

			while(result.Count() < n_max)
			{
				next = p;
				if(!separator.Parse(context, next))
					break;
				EL_ERROR(next == p, TLogicException);

				usys_t after_value = next;
				auto value = node.Parse(context, after_value);
				if(!value)
					return std::nullopt;
				EL_ERROR(after_value == next, TLogicException);
				p = after_value;
				result.Append(std::move(*value));
			}

			if(result.Count() < n_min)
				return std::nullopt;
			pos = p;
			return result;
		}

		bool Match(TParseContext& context, usys_t& pos) const
		{
			usys_t p = pos;
			if(n_max == 0)
			{
				if(n_min != 0)
					return false;
				return true;
			}

			usys_t count = 0;
			usys_t next = p;
			if(!node.Match(context, next))
			{
				if(n_min != 0)
					return false;
				return true;
			}
			EL_ERROR(next == p, TLogicException);
			p = next;
			count = 1;

			while(count < n_max)
			{
				next = p;
				if(!separator.Match(context, next))
					break;
				EL_ERROR(next == p, TLogicException);

				usys_t after_value = next;
				if(!node.Match(context, after_value))
					return false;
				EL_ERROR(after_value == next, TLogicException);
				p = after_value;
				count++;
			}

			if(count < n_min)
				return false;
			pos = p;
			return true;
		}

		auto Complete(TCompletionContext& context, const usys_t pos) const -> TCompletionState
		{
			if(n_max == 0)
				return n_min == 0 ? TCompletionState::Matched(pos) : TCompletionState::Mismatch(pos);

			usys_t p = pos;
			usys_t count = 0;
			bool incomplete = false;

			auto first = node.Complete(context, p);
			incomplete = incomplete || first.incomplete;
			if(!first.matched)
				return n_min == 0 ? TCompletionState::Matched(pos, incomplete) : TCompletionState{false, incomplete, pos};
			EL_ERROR(first.position == p, TLogicException);
			p = first.position;
			count = 1;

			while(count < n_max)
			{
				auto separator_state = separator.Complete(context, p);
				incomplete = incomplete || separator_state.incomplete;
				if(!separator_state.matched)
					return count >= n_min ? TCompletionState::Matched(p, incomplete) : TCompletionState{false, incomplete, pos};
				EL_ERROR(separator_state.position == p, TLogicException);

				auto value_state = node.Complete(context, separator_state.position);
				incomplete = incomplete || value_state.incomplete;
				if(!value_state.matched)
					return {false, incomplete, pos};
				EL_ERROR(value_state.position == separator_state.position, TLogicException);
				p = value_state.position;
				count++;
			}

			return count >= n_min ? TCompletionState::Matched(p, incomplete) : TCompletionState{false, incomplete, pos};
		}
	};
}
