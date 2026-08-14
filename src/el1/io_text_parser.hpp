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

	namespace ast
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

		public:
			auto Parse(TParseContext& context, usys_t& pos) const -> std::optional<return_t>;
			bool Match(TParseContext& context, usys_t& pos) const;
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
		};
	}

	template<typename N>
	struct TParser
	{
		using node_t = N;
		using return_t = typename N::return_t;

		N root;
		constexpr explicit TParser(N root) : root(std::move(root)) {}

		template<typename N2>
		constexpr auto operator+(TParser<N2> rhs) const
		{
			return TParser<ast::TSequenceNode<N, N2>>(ast::TSequenceNode<N, N2>(root, rhs.root));
		}

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
			EL_ERROR(!value, TException, "unable to parse");
			char32_t extra;
			EL_ERROR(context.At(pos, extra), TException, TString::Format(U"unable to parse full string - only %d out of %d characters accepted by parser", pos, str.Length()));
			return std::move(*value);
		}

		auto Parse(const TString& str, const TParseLimits limits = {}) const -> return_t { return Parse(str.View(), limits); }
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

	constexpr TParser<ast::TCharListNode<1>> operator""_P(const char32_t chr)
	{
		return TParser(ast::TCharListNode<1>(chr));
	}

	constexpr TParser<ast::TLiteralNode> operator""_P(const char32_t* const str, const size_t len)
	{
		return TParser(ast::TLiteralNode(TStringView::FromUnsafePointer(str, len)));
	}

	template<typename N> constexpr auto Optional(TParser<N> parser) { return TParser(ast::TRepeatNode<N>{parser.root, 0, 1}); }
	template<typename N> constexpr auto Repeat(TParser<N> parser, const usys_t n_min, const usys_t n_max) { return TParser(ast::TRepeatNode<N>{parser.root, n_min, n_max}); }
	template<typename N> constexpr auto OneOrMore(TParser<N> parser) { return TParser(ast::TRepeatNode<N>{parser.root, 1, NEG1}); }
	constexpr auto CharRange(const char32_t from, const char32_t to) { return TParser(ast::TCharRangeNode(from, to)); }
	template<typename... A> constexpr auto CharList(A... a) { return TParser(ast::TCharListNode<sizeof...(a)>(a...)); }
	template<typename N> constexpr auto If(const bool condition, TParser<N> parser) { return TParser(ast::TIfNode<N>{condition, std::move(parser.root)}); }
	template<typename N> requires ast::is_char_matcher_v<N> constexpr auto operator~(TParser<N> parser) { return TParser(ast::TCharNotNode<N>{std::move(parser.root)}); }
	template<typename M, typename N> requires ast::is_char_matcher_v<M> constexpr auto Case(TParser<M> matcher, TParser<N> parser) { return ast::TDispatchCase<M,N>{std::move(matcher.root), std::move(parser.root)}; }
	template<typename... C> constexpr auto Dispatch(C... cases) { return TParser(ast::TDispatchNode<C...>{std::tuple<C...>(std::move(cases)...)}); }
	template<typename P, typename N> constexpr auto Where(P predicate, TParser<N> parser) { return TParser(ast::TWhereNode<P,N>{std::move(predicate), std::move(parser.root)}); }
	template<typename P, typename N> constexpr auto Validate(P predicate, TParser<N> parser) { return TParser(ast::TValidateNode<P,N>{std::move(predicate), std::move(parser.root)}); }
	template<typename N> constexpr auto Expect(TParser<N> parser) { return TParser(ast::TExpectNode<N>{std::move(parser.root)}); }
	constexpr auto End() { return TParser(ast::TEndNode{}); }
	template<typename N> constexpr auto LookAhead(TParser<N> parser) { return TParser(ast::TLookAheadNode<N>{std::move(parser.root)}); }
	template<typename L, typename... N> constexpr auto Translate(L lambda, TParser<N>... parsers) { return TParser(ast::TTranslateNode<L,N...>(std::move(lambda), std::tuple<N...>(std::move(parsers.root)...))); }
	template<typename L, typename... N> constexpr auto TryTranslate(L lambda, TParser<N>... parsers) { return TParser(ast::TTryTranslateNode<L,N...>{std::move(lambda), std::tuple<N...>(std::move(parsers.root)...)}); }
	template<typename N> constexpr auto Capture(TParser<N> parser) { return TParser(ast::TCaptureNode<N>{std::move(parser.root)}); }
	template<typename T, typename C, typename... N> constexpr auto TranslateCast(TParser<N>... parsers) { return Translate([](auto... a){ return New<T,C>(a...); }, parsers...); }
	template<typename T, typename... N> constexpr auto Translate(TParser<N>... parsers) { return Translate([](auto... a){ return T(a...); }, parsers...); }
	template<typename N> constexpr auto Discard(TParser<N> parser) { return TParser(ast::TDiscardNode<N>{parser.root}); }

	template<typename T, typename F>
	constexpr auto Recursive(F&& factory)
	{
		using TFactory = std::decay_t<F>;
		return TParser(ast::TRecursiveNode<T, TFactory>{std::forward<F>(factory)});
	}

	template<typename N, typename S>
	constexpr auto SeparatedBy(TParser<N> parser, TParser<S> separator, const usys_t n_min = 0, const usys_t n_max = NEG1)
	{
		return TParser(ast::TSeparatedByNode<N, S>{parser.root, separator.root, n_min, n_max});
	}

	template<typename O, typename N, typename C>
	constexpr auto Between(TParser<O> open, TParser<N> parser, TParser<C> close)
	{
		return Discard(std::move(open)) + std::move(parser) + Discard(std::move(close));
	}
}
