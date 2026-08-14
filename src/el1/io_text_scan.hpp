#pragma once

#include "io_text_format.hpp"
#include "io_text_parser.hpp"
#include "io_text_string.hpp"
#include "io_bcd.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>

namespace el1::io::text::scan
{
	using namespace io::types;
	using format::TDefaultFormatSpec;
	using format::TFormatSpecView;

	template<typename T>
	struct TScanner;

	namespace detail
	{
		class TScanContext
		{
			stream::IBufferedSource<char32_t>& source;
			io::collection::array::array_t<const char32_t> buffer;

		public:
			explicit TScanContext(stream::IBufferedSource<char32_t>& source EL_LIFETIME_BOUND)
				: source(source), buffer(source.Head()) {}

			bool Ensure(const usys_t count)
			{
				if(buffer.Count() >= count)
					return true;
				const bool available = source.Ensure(count);
				buffer = source.Head();
				EL_ERROR(available && buffer.Count() < count, error::TLogicException);
				return available;
			}

			bool At(const usys_t index, char32_t& out)
			{
				if(!Ensure(index + 1))
					return false;
				out = buffer[index];
				return true;
			}

			string::TStringView Capture(const usys_t begin, const usys_t end)
			{
				EL_ERROR(end < begin || (end != 0 && !Ensure(end)), error::TLogicException);
				if(begin == end)
					return {};
				return string::TStringView::FromUnsafePointer(buffer.ItemPtr(begin), end - begin);
			}

			void Refresh() noexcept { buffer = source.Head(); }
			stream::IBufferedSource<char32_t>& Source() noexcept { return source; }
		};

		inline bool At(detail::TScanContext& input, const usys_t index, char32_t& out)
		{
			return input.At(index, out);
		}

		inline bool IsWhitespace(const char32_t chr)
		{
			return string::WHITESPACE_CHARS.Contains(chr);
		}

		inline void SkipWhitespace(detail::TScanContext& input, usys_t& pos)
		{
			char32_t chr;
			while(detail::At(input, pos, chr) && IsWhitespace(chr))
				pos++;
		}

		constexpr int DigitValue(const char32_t chr) noexcept
		{
			if(chr >= U'0' && chr <= U'9') return (int)(chr - U'0');
			if(chr >= U'a' && chr <= U'f') return 10 + (int)(chr - U'a');
			if(chr >= U'A' && chr <= U'F') return 10 + (int)(chr - U'A');
			return -1;
		}

		struct TNumericScannerBase
		{
			static constexpr bool Supports(const char32_t code) noexcept
			{
				return format::detail::IsNumericCode(code);
			}

			static constexpr unsigned Radix(const char32_t code) noexcept
			{
				return format::detail::TNumericFormatterBase::Radix(code);
			}

			static constexpr bool Validate(const char32_t, const TDefaultFormatSpec& spec) noexcept
			{
				return !spec.has_pad && !spec.has_precision;
			}
		};

		template<typename T, typename = void>
		struct TScannerSpec { using type = TDefaultFormatSpec; };

		template<typename T>
		struct TScannerSpec<T, std::void_t<typename TScanner<T>::TSpec>> { using type = typename TScanner<T>::TSpec; };

		template<typename T>
		using scanner_spec_t = typename TScannerSpec<T>::type;

		template<typename T>
		constexpr bool ParseScannerSpec(scanner_spec_t<T>& out, const char32_t code, const TFormatSpecView& text) noexcept
		{
			if constexpr(requires { { TScanner<T>::ParseSpec(out, code, text) } -> std::convertible_to<bool>; })
				return TScanner<T>::ParseSpec(out, code, text);
			else
			{
				if(text.IsBraced())
					return false;
				if constexpr(std::same_as<scanner_spec_t<T>, TDefaultFormatSpec>)
					return format::ParseDefaultFormatSpec(out, text);
				else
					return false;
			}
		}

		template<typename T>
		constexpr bool ValidateScannerSpec(const char32_t code, const scanner_spec_t<T>& spec) noexcept
		{
			if constexpr(requires { { TScanner<T>::Validate(code, spec) } -> std::convertible_to<bool>; })
				return TScanner<T>::Validate(code, spec);
			return true;
		}

		template<typename T>
		concept CDirectScanner = requires(detail::TScanContext& input, usys_t& pos, const char32_t code, const scanner_spec_t<T>& spec)
		{
			{ TScanner<T>::Scan(input, pos, code, spec) } -> std::same_as<std::optional<T>>;
		};

		template<typename T>
		concept CParserScanner = requires(detail::TScanContext& input, usys_t& pos, const char32_t code, const scanner_spec_t<T>& spec)
		{
			{ TScanner<T>::Parser(code, spec).TryParse(input.Source(), pos) } -> std::same_as<std::optional<T>>;
		};

		template<typename T>
		concept CScanner = requires(const char32_t code)
		{
			{ TScanner<T>::Supports(code) } -> std::convertible_to<bool>;
		} && (CDirectScanner<T> || CParserScanner<T>);

		template<typename T>
		std::optional<T> ScanValue(detail::TScanContext& input, usys_t& pos, const char32_t code, const scanner_spec_t<T>& spec)
		{
			if constexpr(CDirectScanner<T>)
				return TScanner<T>::Scan(input, pos, code, spec);
			else
			{
				auto value = TScanner<T>::Parser(code, spec).TryParse(input.Source(), pos);
				input.Refresh();
				return value;
			}
		}

		inline std::optional<string::TStringView> NumericToken(detail::TScanContext& input, usys_t& pos, const unsigned radix, const bool decimal, const bool exponent, const TDefaultFormatSpec& spec)
		{
			usys_t p = pos;
			SkipWhitespace(input, p);
			const usys_t field_begin = p;
			const usys_t field_end = spec.has_width ? field_begin + spec.width : NEG1;
			auto within = [&](const usys_t at) { return field_end == NEG1 || at < field_end; };

			char32_t chr;
			if(within(p) && detail::At(input, p, chr) && (chr == U'+' || chr == U'-'))
				p++;

			bool have_digit = false;
			bool have_decimal = false;
			while(within(p) && detail::At(input, p, chr))
			{
				const int digit = DigitValue(chr);
				if(digit >= 0 && (unsigned)digit < radix)
				{
					have_digit = true;
					p++;
					continue;
				}
				if(decimal && !have_decimal && chr == U'.')
				{
					have_decimal = true;
					p++;
					continue;
				}
				break;
			}
			if(!have_digit)
				return std::nullopt;

			if(exponent && radix == 10 && within(p) && detail::At(input, p, chr) && (chr == U'e' || chr == U'E'))
			{
				p++;
				if(within(p) && detail::At(input, p, chr) && (chr == U'+' || chr == U'-'))
					p++;

				bool have_exponent_digit = false;
				while(within(p) && detail::At(input, p, chr) && chr >= U'0' && chr <= U'9')
				{
					have_exponent_digit = true;
					p++;
				}
				if(!have_exponent_digit)
					return std::nullopt;
			}

			EL_ERROR(p <= field_begin || !input.Ensure(p), error::TLogicException);
			pos = p;
			return input.Capture(field_begin, p);
		}

		template<typename T>
		requires (std::is_integral_v<T> || std::is_enum_v<T>)
		std::optional<T> TokenToInteger(const string::TStringView token, const unsigned radix)
		{
			T result;
			bool ok;
			switch(radix)
			{
				case 2: ok = bcd::TBCD::ParseIntegerMSD<T, 2>(token, result); break;
				case 8: ok = bcd::TBCD::ParseIntegerMSD<T, 8>(token, result); break;
				case 10: ok = bcd::TBCD::ParseIntegerMSD<T, 10>(token, result); break;
				case 16: ok = bcd::TBCD::ParseIntegerMSD<T, 16>(token, result); break;
				default: return std::nullopt;
			}
			if(!ok)
				return std::nullopt;
			return result;
		}

		inline bcd::TBCD TokenToBCD(const string::TStringView token, const unsigned radix)
		{
			EL_ERROR(radix < 2 || radix > 36, error::TLogicException);
			return bcd::TBCD::FromStringMSD(token, (bcd::digit_t)radix);
		}

		template<typename T>
		requires std::is_floating_point_v<T>
		std::optional<T> TokenToFloating(const string::TStringView token)
		{
			try
			{
				const T result = (T)bcd::TBCD::ParseDoubleMSD(token, 10, 0, true);
				if(!std::isfinite(result))
					return std::nullopt;
				return result;
			}
			catch(const error::IException&)
			{
				return std::nullopt;
			}
		}

	}

	template<typename T>
	requires (((std::is_integral_v<T> && !format::detail::IsCharacterType<T>) || std::is_enum_v<T>) || std::is_floating_point_v<T>)
	std::optional<T> ParseNumber(const string::TStringView token, const unsigned radix = 10)
	{
		if constexpr(std::is_floating_point_v<T>)
		{
			if(radix == 10)
				return detail::TokenToFloating<T>(token);
			try { return (T)bcd::TBCD::ParseDoubleMSD(token, (bcd::digit_t)radix); }
			catch(const error::IException&) { return std::nullopt; }
		}
		else
		{
			return detail::TokenToInteger<T>(token, radix);
		}
	}

	template<typename T>
	requires ((std::is_integral_v<T> && !format::detail::IsCharacterType<T>) || std::is_enum_v<T>)
	struct TScanner<T> : detail::TNumericScannerBase
	{
		static std::optional<T> Scan(detail::TScanContext& input, usys_t& pos, const char32_t code, const TDefaultFormatSpec& spec)
		{
			const unsigned radix = Radix(code);
			auto token = detail::NumericToken(input, pos, radix, false, false, spec);
			if(!token)
				return std::nullopt;
			return ParseNumber<T>(*token, radix);
		}
	};

	template<typename T>
	requires std::is_floating_point_v<T>
	struct TScanner<T> : detail::TNumericScannerBase
	{
		static std::optional<T> Scan(detail::TScanContext& input, usys_t& pos, const char32_t code, const TDefaultFormatSpec& spec)
		{
			const unsigned radix = Radix(code);
			auto token = detail::NumericToken(input, pos, radix, true, radix == 10, spec);
			if(!token)
				return std::nullopt;
			return ParseNumber<T>(*token, radix);
		}
	};

	template<>
	struct TScanner<bcd::TBCD> : detail::TNumericScannerBase
	{
		static std::optional<bcd::TBCD> Scan(detail::TScanContext& input, usys_t& pos, const char32_t code, const TDefaultFormatSpec& spec)
		{
			const unsigned radix = Radix(code);
			auto token = detail::NumericToken(input, pos, radix, true, false, spec);
			if(!token)
				return std::nullopt;
			try { return detail::TokenToBCD(*token, radix); }
			catch(const error::IException&) { return std::nullopt; }
		}
	};

	template<typename T>
	requires (!std::derived_from<T, bcd::TBCD> && requires(T value, const bcd::TBCD& parsed)
	{
		{ value.ToBCD() } -> std::convertible_to<bcd::TBCD>;
		value = parsed;
	})
	struct TScanner<T> : detail::TNumericScannerBase
	{
		static std::optional<T> Scan(detail::TScanContext& input, usys_t& pos, const char32_t code, const TDefaultFormatSpec& spec)
		{
			const unsigned radix = Radix(code);
			auto token = detail::NumericToken(input, pos, radix, true, false, spec);
			if(!token)
				return std::nullopt;
			try
			{
				T result;
				result = detail::TokenToBCD(*token, radix);
				return result;
			}
			catch(const error::IException&) { return std::nullopt; }
		}
	};

	template<typename T>
	requires format::detail::IsCharacterType<T>
	struct TScanner<T>
	{
		static constexpr bool Supports(const char32_t code) noexcept { return code == U'c'; }
		static constexpr bool Validate(const char32_t, const TDefaultFormatSpec& spec) noexcept { return !spec.has_pad && !spec.has_precision && (!spec.has_width || spec.width == 1); }
		static std::optional<T> Scan(detail::TScanContext& input, usys_t& pos, const char32_t, const TDefaultFormatSpec&)
		{
			char32_t chr;
			if(!detail::At(input, pos, chr))
				return std::nullopt;
			pos++;
			if constexpr(std::same_as<T, char32_t>)
				return chr;
			else if(chr <= (char32_t)std::numeric_limits<std::make_unsigned_t<T>>::max())
				return (T)chr;
			else
				return std::nullopt;
		}
	};

	template<>
	struct TScanner<string::TString>
	{
		static constexpr bool Supports(const char32_t code) noexcept { return code == U's' || code == U'q'; }
		static constexpr bool Validate(const char32_t, const TDefaultFormatSpec& spec) noexcept { return !spec.has_pad && !spec.has_precision; }

		static std::optional<string::TString> Scan(detail::TScanContext& input, usys_t& pos, const char32_t code, const TDefaultFormatSpec& spec)
		{
			usys_t p = pos;
			detail::SkipWhitespace(input, p);
			const usys_t limit = spec.has_width ? spec.width : NEG1;
			string::TString result;
			char32_t chr;
			if(code == U's')
			{
				while(result.Length() < limit && detail::At(input, p, chr) && !detail::IsWhitespace(chr))
				{
					result += chr;
					p++;
				}
				if(result.Length() == 0)
					return std::nullopt;
			}
			else
			{
				if(!detail::At(input, p, chr) || (chr != U'\'' && chr != U'"'))
					return std::nullopt;
				const char32_t quote = chr;
				p++;
				bool closed = false;
				while(result.Length() < limit && detail::At(input, p, chr))
				{
					p++;
					if(chr == quote)
					{
						closed = true;
						break;
					}
					if(chr == U'\\')
					{
						if(!detail::At(input, p, chr))
							return std::nullopt;
						p++;
					}
					result += chr;
				}
				if(!closed)
					return std::nullopt;
			}
			pos = p;
			return result;
		}
	};

	template<typename... TArgs>
	class TScanString
	{
		template<typename T>
		struct TVariable
		{
			usys_t begin = NEG1;
			usys_t end = NEG1;
			char32_t code = U'\0';
			detail::scanner_spec_t<T> spec = {};
			bool valid = false;
		};

		struct TUncheckedTag {};
		const char32_t* literal = nullptr;
		usys_t length = 0;
		std::tuple<TVariable<TArgs>...> variables = {};

		constexpr char32_t Character(const usys_t index) const noexcept { return literal[index]; }
		constexpr TFormatSpecView SpecView(const usys_t begin, const usys_t end, const bool braced) const noexcept { return {literal + begin, end - begin, braced}; }

		template<typename T>
		constexpr TVariable<T> Find(const usys_t start) const noexcept
		{
			TVariable<T> result;
			for(usys_t i = start; i < length; i++)
			{
				if(Character(i) != U'%') continue;
				if(i + 1 >= length) return result;
				if(Character(i + 1) == U'%') { i++; continue; }
				result.begin = i;
				usys_t p = i + 1, spec_begin = p, spec_end = p;
				bool braced = false;
				if(Character(p) == U'{')
				{
					braced = true;
					spec_begin = ++p;
					while(p < length && Character(p) != U'}') p++;
					if(p >= length) return result;
					spec_end = p++;
				}
				else
				{
					while(p < length && !format::detail::IsAsciiLetter(Character(p))) p++;
					spec_end = p;
				}
				if(p >= length || !format::detail::IsAsciiLetter(Character(p))) return result;
				result.code = Character(p);
				result.end = p + 1;
				if constexpr(detail::CScanner<T>)
				{
					if(!TScanner<T>::Supports(result.code)) return result;
					if(!detail::ParseScannerSpec<T>(result.spec, result.code, SpecView(spec_begin, spec_end, braced))) return result;
					if(!detail::ValidateScannerSpec<T>(result.code, result.spec)) return result;
					result.valid = true;
				}
				return result;
			}
			return result;
		}

		constexpr bool HasVariable(const usys_t start) const noexcept
		{
			for(usys_t i = start; i < length; i++)
				if(Character(i) == U'%')
				{
					if(i + 1 < length && Character(i + 1) == U'%') { i++; continue; }
					return true;
				}
			return false;
		}

		template<usys_t I>
		constexpr bool ValidateFrom(const usys_t pos) const noexcept
		{
			if constexpr(I == sizeof...(TArgs))
				return !HasVariable(pos);
			else
			{
				using T = std::tuple_element_t<I, std::tuple<TArgs...>>;
				if constexpr(!detail::CScanner<T>)
					return false;
				else
				{
					const auto variable = Find<T>(pos);
					return variable.valid && ValidateFrom<I + 1>(variable.end);
				}
			}
		}

		template<usys_t I>
		consteval bool InitializeFrom(const usys_t pos)
		{
			if constexpr(I == sizeof...(TArgs))
				return !HasVariable(pos);
			else
			{
				using T = std::tuple_element_t<I, std::tuple<TArgs...>>;
				if constexpr(!detail::CScanner<T>) return false;
				else
				{
					const auto variable = Find<T>(pos);
					if(!variable.valid) return false;
					std::get<I>(variables) = variable;
					return InitializeFrom<I + 1>(variable.end);
				}
			}
		}

		bool MatchLiteral(detail::TScanContext& input, usys_t& pos, const usys_t begin, const usys_t end) const
		{
			for(usys_t i = begin; i < end; i++)
			{
				char32_t expected = Character(i);
				if(expected == U'%' && i + 1 < end && Character(i + 1) == U'%')
				{
					expected = U'%';
					i++;
				}
				char32_t actual;
				if(!detail::At(input, pos, actual) || actual != expected)
					return false;
				pos++;
			}
			return true;
		}

		template<usys_t I = 0>
		bool ScanFrom(detail::TScanContext& input, usys_t& pos, usys_t literal_pos, std::tuple<std::optional<TArgs>...>& values) const
		{
			if constexpr(I == sizeof...(TArgs))
				return MatchLiteral(input, pos, literal_pos, length);
			else
			{
				const auto& variable = std::get<I>(variables);
				if(!MatchLiteral(input, pos, literal_pos, variable.begin)) return false;
				using T = std::tuple_element_t<I, std::tuple<TArgs...>>;
				auto value = detail::ScanValue<T>(input, pos, variable.code, variable.spec);
				if(!value) return false;
				std::get<I>(values) = std::move(value);
				return ScanFrom<I + 1>(input, pos, variable.end, values);
			}
		}

		consteval void RequireValid()
		{
			if(!InitializeFrom<0>(0))
				throw "invalid scan literal or scanner/argument mismatch";
		}

		template<std::size_t N>
		consteval TScanString(TUncheckedTag, const char32_t (&value)[N]) noexcept : literal(value), length(N - 1) {}

	public:
		template<std::size_t N>
		consteval TScanString(const char32_t (&value)[N]) : literal(value), length(N - 1) { RequireValid(); }

		constexpr bool IsValid() const noexcept { return ValidateFrom<0>(0); }

		bool TryScan(stream::IBufferedSource<char32_t>& input, std::tuple<std::optional<TArgs>...>& values, usys_t& consumed) const
		{
		detail::TScanContext context(input);
			consumed = 0;
			return ScanFrom(context, consumed, 0, values);
		}

		template<typename... A, std::size_t N>
		friend consteval bool IsValidScan(const char32_t (&value)[N]) noexcept;
	};

	template<typename... TArgs, std::size_t N>
	consteval bool IsValidScan(const char32_t (&value)[N]) noexcept
	{
		const TScanString<TArgs...> scan(typename TScanString<TArgs...>::TUncheckedTag{}, value);
		return scan.IsValid();
	}
}
