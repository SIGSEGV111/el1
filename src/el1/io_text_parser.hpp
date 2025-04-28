#pragma once

#include "error.hpp"
#include "io_collection_list.hpp"
#include "io_file.hpp"
#include "io_stream_buffer.hpp"
#include "io_text_encoding.hpp"
#include "io_text_string.hpp"
#include "io_text_terminal.hpp"
#include "io_types.hpp"
#include "util.hpp"
#include "util_function.hpp"
#include <any>
#include <optional>
#include <variant>

namespace el1::io::text::parser
{
	using namespace el1::io::types;
	using namespace el1::io::text::encoding;
	using namespace el1::io::text::string;
	using namespace el1::io::text::terminal;
	using namespace el1::io::collection::list;

	struct repeat_t
	{
		usys_t n_min;
		usys_t n_max;

		constexpr repeat_t(usys_t n) : n_min(n), n_max(n) {}
		constexpr repeat_t(usys_t n_min, usys_t n_max) : n_min(n_min), n_max(n_max) {}
	};

	namespace ast
	{
		template<typename T>
		struct arrayify_t {
			using type = TList<T>;
		};

		template<typename T>
		struct arrayify_t<TList<T>> {
			using type = TList<T>;
		};

		struct TCharRangeNode
		{
			using return_t = TUTF32;
			const TUTF32 from;
			const TUTF32 to;
			constexpr TCharRangeNode(const TUTF32 from, const TUTF32 to) : from(from), to(to) {}

			auto Parse(const array_t<TUTF32> chars, usys_t& pos) -> std::optional<return_t>
			{
				if(pos >= chars.Count())
					return std::nullopt;
				auto chr = chars[pos++];
				if(chr >= from && chr <= to)
					return chr;
				else
					return std::nullopt;
			}
		};

		template<usys_t n>
		struct TCharListNode
		{
			using return_t = TUTF32;
			const TUTF32 list[n];

			template<typename ... A>
			constexpr TCharListNode(A ... a) : list{a...} {}

			auto Parse(const array_t<TUTF32> chars, usys_t& pos) -> std::optional<return_t>
			{
				if(pos >= chars.Count())
					return std::nullopt;
				auto chr = chars[pos++];
				for(usys_t i = 0; i < n; i++)
					if(list[i] == chr)
						return chr;
				return std::nullopt;
			}
		};

		struct TLiteralNode
		{
			using return_t = array_t<const TUTF32>;
			const TString literal;
			TLiteralNode(TString _literal) : literal(std::move(_literal)) {}

			auto Parse(const array_t<TUTF32> chars, usys_t& pos) -> std::optional<return_t>
			{
				usys_t s = pos;
				for(auto lchr : literal.chars)
				{
					if(pos >= chars.Count() || lchr != chars[pos])
						return std::nullopt;
					pos++;
				}
				// term<<TString::Format("TLiteralNode(%q) returning success (pos@%d)\n", literal, pos);
				return array_t<const TUTF32>(&chars[s], pos - s);
			}
		};

		template<typename L, typename ... N>
		struct TTranslateNode
		{
			using return_t = typename std::result_of<L(typename N::return_t ...)>::type;
			L lambda;
			std::tuple<N...> nodes;
			constexpr TTranslateNode(L _lambda, std::tuple<N...> _nodes) : lambda(std::move(_lambda)), nodes(std::move(_nodes)) {}

			auto Parse(const array_t<TUTF32> chars, usys_t& pos) -> std::optional<return_t>
			{
				try
				{
					return std::apply([&](auto ... nodes) { return lambda(*nodes.Parse(chars, pos) ...); }, nodes);
				}
				catch(std::bad_optional_access)
				{
					return std::nullopt;
				}
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

			auto Parse(const array_t<TUTF32> chars, usys_t& pos) -> std::optional<return_t>
			{
				return_t r;
				for(usys_t i = 0; i < n_max; i++)
				{
					usys_t s = pos;
					auto v = node.Parse(chars, s);
					if(v == std::nullopt)
					{
						if(i < n_min)
							return std::nullopt;
						else
							break;
					}
					pos = s;
					r.Append(*v);
				}
				// term<<TString::Format("TRepeatNode(%d,%d) returning success (pos@%d)\n", n_min, n_max, pos);
				return r;
			}
		};

		struct discard_t {};

		template<typename N>
		struct TDiscardNode
		{
			using T = typename N::return_t;
			using return_t = discard_t;
			N node;

			auto Parse(const array_t<TUTF32> chars, usys_t& pos) -> std::optional<return_t>
			{
				if(node.Parse(chars, pos) == std::nullopt)
				{
					// term<<TString::Format("TDiscardNode() returning failure (pos@%d)\n", pos);
					return std::nullopt;
				}
				else
				{
					// term<<TString::Format("TDiscardNode() returning success (pos@%d)\n", pos);
					return discard_t();
				}
			}
		};

		template<typename N1, typename N2>
		struct TSequenceNode
		{
			using T1 = typename N1::return_t;
			using T2 = typename N2::return_t;
			using A1 = typename arrayify_t<T1>::type;
			using A2 = typename arrayify_t<T2>::type;
			using return_t =
				typename std::conditional<std::is_same<A1, A2>::value,
					A1,
					typename std::conditional<std::is_same<T1, discard_t>::value,
						T2,
						typename std::conditional<std::is_same<T2, discard_t>::value,
							T1,
							std::tuple<T1, T2>
						>::type
					>::type
				>::type;

			N1 lhs;
			N2 rhs;
			constexpr TSequenceNode(N1 lhs, N2 rhs) : lhs(lhs), rhs(rhs) {}

			auto Parse(const array_t<TUTF32> chars, usys_t& pos) -> std::optional<return_t>
			{
				auto v_lhs = lhs.Parse(chars, pos);
				if(v_lhs == std::nullopt)
				{
					// term<<TString::Format("TSequenceNode() returning failure lhs (pos@%d)\n", pos);
					return std::nullopt;
				}
				auto v_rhs = rhs.Parse(chars, pos);
				if(v_rhs == std::nullopt)
				{
					// term<<TString::Format("TSequenceNode() returning failure rhs (pos@%d)\n", pos);
					return std::nullopt;
				}

				// term<<TString::Format("TSequenceNode() returning success (pos@%d)\n", pos);

				if constexpr (std::is_same<T1, A1>::value || std::is_same<T2, A2>::value)
				{
					if constexpr (std::is_same<T1, A1>::value)
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
				else if constexpr (std::is_same<T1, discard_t>::value && std::is_same<T2, discard_t>::value)
				{
					return discard_t();
				}
				else if constexpr (std::is_same<T1, discard_t>::value)
				{
					return std::move(*v_rhs);
				}
				else if constexpr (std::is_same<T2, discard_t>::value)
				{
					return std::move(*v_lhs);
				}
				else if constexpr (std::is_same<T1, T2>::value)
				{
					TList<T1> l(2);
					l.MoveAppend(std::move(*v_lhs));
					l.MoveAppend(std::move(*v_rhs));
					return l;
				}
				else
				{
					return std::tuple<T1, T2>(*v_lhs, *v_rhs);
				}
			}
		};

		template<typename N1, typename N2>
		struct TXorNode
		{
			using T1 = typename N1::return_t;
			using T2 = typename N2::return_t;
			using A1 = typename arrayify_t<T1>::type;
			using A2 = typename arrayify_t<T2>::type;
			using return_t =
				typename std::conditional<std::is_same<T1, T2>::value,
					T1,
					typename std::conditional<std::is_same<A1, A2>::value,
						A1,
						std::variant<T1, T2>
					>::type
				>::type;
			N1 lhs;
			N2 rhs;
			constexpr TXorNode(N1 lhs, N2 rhs) : lhs(lhs), rhs(rhs) {}

			auto Parse(const array_t<TUTF32> chars, usys_t& pos) -> std::optional<return_t>
			{
				usys_t s = pos;
				auto v_lhs = lhs.Parse(chars, s);
				if(v_lhs != std::nullopt)
				{
					// term<<TString::Format("TXorNode() returning success lhs (pos@%d)\n", pos);
					pos = s;
					return std::move(*v_lhs);
				}

				auto v_rhs = lhs.Parse(chars, pos);
				if(v_rhs != std::nullopt)
				{
					// term<<TString::Format("TXorNode() returning success rhs (pos@%d)\n", pos);
					return std::move(*v_rhs);
				}

				// term<<TString::Format("TXorNode() returning failure (pos@%d)\n", pos);
				return std::nullopt;
			}
		};
	};

	template<typename N>
	struct TParser
	{
		N root;
		constexpr TParser(N root) : root(root) {}

		template<typename N2>
		constexpr auto operator+(TParser<N2> rhs) const
		{
			return TParser<ast::TSequenceNode<N, N2>>(ast::TSequenceNode<N, N2>(this->root, rhs.root));
		}

		template<typename N2>
		constexpr auto operator||(TParser<N2> rhs) const
		{
			return TParser<ast::TXorNode<N, N2>>(ast::TXorNode<N, N2>(this->root, rhs.root));
		}

		auto Parse(const TString& str) -> typename N::return_t
		{
			usys_t pos = 0;
			auto v = root.Parse(str.chars, pos);
			EL_ERROR(v == std::nullopt, TException, "unable to parse");
			EL_ERROR(pos != str.Length(), TException, TString::Format("unable to parse full string - only %d out of %d characters accepted by parser", pos, str.Length()));
			return *v;
		}
	};

	constexpr TParser<ast::TCharListNode<1>> operator""_P(const char chr)
	{
		return TParser(ast::TCharListNode<1>(chr));
	}

	TParser<ast::TLiteralNode> operator""_P(const char* const str, size_t len);

	template<typename N>
	constexpr auto Optional(TParser<N> parser)
	{
		return TParser(ast::TRepeatNode<N>(parser.root, 0, 1));
	}

	template<typename N>
	constexpr auto Repeat(TParser<N> parser, const usys_t n_min, const usys_t n_max)
	{
		return TParser(ast::TRepeatNode<N>(parser.root, n_min, n_max));
	}

	template<typename N>
	constexpr auto OneOrMore(TParser<N> parser)
	{
		return TParser(ast::TRepeatNode<N>(parser.root, 1, NEG1));
	}

	constexpr auto CharRange(const TUTF32 from, const TUTF32 to)
	{
		return TParser(ast::TCharRangeNode(from, to));
	}

	template<typename ... A>
	constexpr auto CharList(A ... a)
	{
		return TParser(ast::TCharListNode<sizeof...(a)>(a ...));
	}

	template<typename L, typename ... N>
	constexpr auto Translate(L lambda, TParser<N> ... parsers)
	{
		return TParser(ast::TTranslateNode<L,N...>(lambda, std::tuple<N...>(parsers.root ...)));
	}

	template<typename T, typename C, typename ... N>
	constexpr auto TranslateCast(TParser<N> ... parsers)
	{
		return Translate([](auto ... a){ return New<T,C>(a...); }, parsers...);
	}

	template<typename T, typename ... N>
	constexpr auto Translate(TParser<N> ... parsers)
	{
		return Translate([](auto ... a){ return T(a...); }, parsers...);
	}

	template<typename N>
	constexpr auto Discard(TParser<N> parser)
	{
		return TParser(ast::TDiscardNode<N>(parser.root));
	}
}
