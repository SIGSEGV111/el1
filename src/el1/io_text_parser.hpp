#pragma once

#include "error.hpp"
#include "io_collection_list.hpp"
#include "io_text_string.hpp"
#include "io_types.hpp"
#include "util.hpp"

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

	/** Random-lookahead text input. Offsets are relative to the current input head. */
	struct IInput
	{
		virtual bool Peek(usys_t offset, char32_t& out) = 0;
		virtual ~IInput() = default;
	};

	class TStringInput final : public IInput
	{
		TStringView text;
	public:
		explicit constexpr TStringInput(const TStringView text) : text(text) {}
		bool Peek(const usys_t offset, char32_t& out) final
		{
			if(offset >= text.Length())
				return false;
			out = text[offset];
			return true;
		}
	};

	inline bool MatchLiteral(IInput& input, usys_t& pos, const TStringView literal)
	{
		usys_t p = pos;
		for(const char32_t expected : literal)
		{
			char32_t actual;
			if(!input.Peek(p, actual) || actual != expected)
				return false;
			p++;
		}
		pos = p;
		return true;
	}

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

		struct TCharRangeNode
		{
			using return_t = char32_t;
			char32_t from;
			char32_t to;
			constexpr TCharRangeNode(const char32_t from, const char32_t to) : from(from), to(to) {}

			auto Parse(IInput& input, usys_t& pos) const -> std::optional<return_t>
			{
				char32_t chr;
				if(!input.Peek(pos, chr) || chr < from || chr > to)
					return std::nullopt;
				pos++;
				return chr;
			}
		};

		template<usys_t N>
		struct TCharListNode
		{
			using return_t = char32_t;
			char32_t list[N];
			template<typename... A>
			constexpr TCharListNode(A... a) : list{static_cast<char32_t>(a)...} {}

			auto Parse(IInput& input, usys_t& pos) const -> std::optional<return_t>
			{
				char32_t chr;
				if(!input.Peek(pos, chr))
					return std::nullopt;
				for(const char32_t item : list)
					if(item == chr)
					{
						pos++;
						return chr;
					}
				return std::nullopt;
			}
		};

		struct TLiteralNode
		{
			using return_t = TString;
			TStringView literal;
			constexpr explicit TLiteralNode(const TStringView literal) : literal(literal) {}

			auto Parse(IInput& input, usys_t& pos) const -> std::optional<return_t>
			{
				usys_t p = pos;
				if(!MatchLiteral(input, p, literal))
					return std::nullopt;
				pos = p;
				return TString(literal);
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
			bool ParseNodes(IInput& input, usys_t& pos, values_t& values) const
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

			auto Parse(IInput& input, usys_t& pos) const -> std::optional<return_t>
			{
				usys_t p = pos;
				values_t values;
				if(!ParseNodes(input, p, values))
					return std::nullopt;
				auto result = std::apply([&](auto&... value) { return lambda(std::move(*value)...); }, values);
				pos = p;
				return result;
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

			auto Parse(IInput& input, usys_t& pos) const -> std::optional<return_t>
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
		};

		struct discard_t {};

		template<typename N>
		struct TDiscardNode
		{
			using return_t = discard_t;
			N node;
			auto Parse(IInput& input, usys_t& pos) const -> std::optional<return_t>
			{
				usys_t p = pos;
				if(!node.Parse(input, p))
					return std::nullopt;
				pos = p;
				return discard_t{};
			}
		};

		template<typename N1, typename N2>
		struct TSequenceNode
		{
			using T1 = typename N1::return_t;
			using T2 = typename N2::return_t;
			using A1 = typename arrayify_t<T1>::type;
			using A2 = typename arrayify_t<T2>::type;
			using return_t = typename std::conditional<std::is_same_v<A1, A2>, A1,
				typename std::conditional<std::is_same_v<T1, discard_t>, T2,
					typename std::conditional<std::is_same_v<T2, discard_t>, T1, std::tuple<T1, T2>>::type>::type>::type;

			N1 lhs;
			N2 rhs;
			constexpr TSequenceNode(N1 lhs, N2 rhs) : lhs(std::move(lhs)), rhs(std::move(rhs)) {}

			auto Parse(IInput& input, usys_t& pos) const -> std::optional<return_t>
			{
				usys_t p = pos;
				auto v_lhs = lhs.Parse(input, p);
				if(!v_lhs)
					return std::nullopt;
				auto v_rhs = rhs.Parse(input, p);
				if(!v_rhs)
					return std::nullopt;

				pos = p;
				if constexpr(std::is_same_v<T1, A1> || std::is_same_v<T2, A2>)
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
				else if constexpr(std::is_same_v<T1, discard_t> && std::is_same_v<T2, discard_t>)
					return discard_t{};
				else if constexpr(std::is_same_v<T1, discard_t>)
					return std::move(*v_rhs);
				else if constexpr(std::is_same_v<T2, discard_t>)
					return std::move(*v_lhs);
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

			auto Parse(IInput& input, usys_t& pos) const -> std::optional<return_t>
			{
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
		};
	}

	template<typename N>
	struct TParser
	{
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

		auto TryParse(IInput& input, usys_t& pos) const -> std::optional<typename N::return_t>
		{
			usys_t p = pos;
			auto value = root.Parse(input, p);
			if(value)
				pos = p;
			return value;
		}

		auto TryParsePrefix(IInput& input, usys_t& consumed) const -> std::optional<typename N::return_t>
		{
			consumed = 0;
			return TryParse(input, consumed);
		}

		auto Parse(const TStringView str) const -> typename N::return_t
		{
			TStringInput input(str);
			usys_t pos = 0;
			auto value = root.Parse(input, pos);
			EL_ERROR(!value, TException, "unable to parse");
			char32_t extra;
			EL_ERROR(input.Peek(pos, extra), TException, TString::Format(U"unable to parse full string - only %d out of %d characters accepted by parser", pos, str.Length()));
			return std::move(*value);
		}

		auto Parse(const TString& str) const -> typename N::return_t { return Parse(str.View()); }
	};

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
	template<typename L, typename... N> constexpr auto Translate(L lambda, TParser<N>... parsers) { return TParser(ast::TTranslateNode<L,N...>(std::move(lambda), std::tuple<N...>(parsers.root...))); }
	template<typename T, typename C, typename... N> constexpr auto TranslateCast(TParser<N>... parsers) { return Translate([](auto... a){ return New<T,C>(a...); }, parsers...); }
	template<typename T, typename... N> constexpr auto Translate(TParser<N>... parsers) { return Translate([](auto... a){ return T(a...); }, parsers...); }
	template<typename N> constexpr auto Discard(TParser<N> parser) { return TParser(ast::TDiscardNode<N>{parser.root}); }
}
