#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_text_encoding.hpp"
#include "io_bcd.hpp"

#include <concepts>
#include <cstddef>
#include <tuple>
#include <type_traits>

namespace el1::io::text::string
{
	class TString;
	class TStringView;
}

namespace el1::io::text::format
{
	using namespace io::types;
	using namespace io::text::encoding;

	struct TDefaultFormatSpec
	{
		char32_t pad_sign = U'\0';
		usys_t width = 0;
		usys_t precision = NEG1;
		bool has_pad = false;
		bool has_width = false;
		bool has_precision = false;
	};

	struct TFormatSpecView
	{
		const char32_t* data = nullptr;
		usys_t count = 0;
		bool braced = false;

		constexpr usys_t Length() const noexcept { return count; }
		constexpr bool IsBraced() const noexcept { return braced; }
		constexpr char32_t operator[](const usys_t index) const noexcept { return data[index]; }
	};

	constexpr bool ParseDefaultFormatSpec(TDefaultFormatSpec& out, const TFormatSpecView& text) noexcept;

	/**
	 * Compile-time formatter registry.
	 *
	 * A formatter registers one or more format codes using Supports(). The
	 * formatter may optionally define its own `TSpec` and `ParseSpec()`:
	 *
	 *   using TSpec = TMySpec;
	 *   static constexpr bool ParseSpec(TSpec&, char32_t code, TFormatSpecView);
	 *
	 * Without those members the formatter automatically uses
	 * TDefaultFormatSpec and ParseDefaultFormatSpec(). A braced spec (`%{...}m`)
	 * is deliberately only accepted by a formatter with ParseSpec(); its content
	 * is opaque to the generic parser.
	 *
	 * Format() receives the already parsed spec; no format-spec parsing happens
	 * at runtime.
	 */
	template<typename T>
	struct TFormatter;

	namespace detail
	{
		constexpr bool IsDigit(const char32_t chr) noexcept
		{
			return chr >= '0' && chr <= '9';
		}

		constexpr bool IsAsciiLetter(const char32_t chr) noexcept
		{
			return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z');
		}

		constexpr bool IsNumericCode(const char32_t code) noexcept
		{
			return code == 'b' || code == 'o' || code == 'd' || code == 'u' || code == 'f' || code == 'x';
		}

		template<typename T>
		constexpr bool IsCharacterType = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char32_t>;

		template<typename T>
		constexpr bool IsStringPointer =
			std::is_same_v<T, const char*> || std::is_same_v<T, char*> ||
			std::is_same_v<T, const wchar_t*> || std::is_same_v<T, wchar_t*> ||
			std::is_same_v<T, const char32_t*> || std::is_same_v<T, char32_t*>;

		template<typename T, typename = void>
		struct TFormatterSpec
		{
			using type = TDefaultFormatSpec;
		};

		template<typename T>
		struct TFormatterSpec<T, std::void_t<typename TFormatter<T>::TSpec>>
		{
			using type = typename TFormatter<T>::TSpec;
		};

		template<typename T>
		using formatter_spec_t = typename TFormatterSpec<T>::type;

		template<typename T>
		constexpr bool ParseFormatterSpec(formatter_spec_t<T>& out, const char32_t code, const TFormatSpecView& text) noexcept
		{
			if constexpr(requires { { TFormatter<T>::ParseSpec(out, code, text) } -> std::convertible_to<bool>; })
				return TFormatter<T>::ParseSpec(out, code, text);
			else
			{
				if(text.IsBraced())
					return false;
				if constexpr(std::same_as<formatter_spec_t<T>, TDefaultFormatSpec>)
					return ParseDefaultFormatSpec(out, text);
				else
					return false;
			}
		}

		template<typename T>
		constexpr bool ValidateFormatterSpec(const char32_t code, const formatter_spec_t<T>& spec) noexcept
		{
			if constexpr(requires { { TFormatter<T>::Validate(code, spec) } -> std::convertible_to<bool>; })
				return TFormatter<T>::Validate(code, spec);
			else
				return true;
		}

		template<typename T>
		concept CFormatter = requires(const T& value, const char32_t code, const formatter_spec_t<T>& spec, string::TString& out)
		{
			{ TFormatter<T>::Supports(code) } -> std::convertible_to<bool>;
			TFormatter<T>::Format(out, value, code, spec);
		};

		struct TNumericFormatterBase
		{
			static constexpr bool Supports(const char32_t code) noexcept
			{
				return IsNumericCode(code);
			}

			static constexpr unsigned Radix(const char32_t code) noexcept
			{
				switch(code)
				{
					case 'b': return 2;
					case 'o': return 8;
					case 'd':
					case 'u':
					case 'f': return 10;
					case 'x': return 16;
					default: return 0;
				}
			}
		};

		void AppendLiteral(string::TString& out, const char32_t* data, usys_t begin, usys_t end);

		void FormatSigned(string::TString& out, s64_t value, const TDefaultFormatSpec& spec, unsigned radix, bool upper_case = false);
		void FormatUnsigned(string::TString& out, u64_t value, const TDefaultFormatSpec& spec, unsigned radix, bool upper_case = false);
		void FormatFloat(string::TString& out, double value, const TDefaultFormatSpec& spec, unsigned radix);
		void FormatBCD(string::TString& out, const bcd::TBCD& value, const TDefaultFormatSpec& spec, unsigned radix);
		void FormatString(string::TString& out, const string::TString& value, char32_t code, const TDefaultFormatSpec& spec);
		void FormatStringView(string::TString& out, const string::TStringView& value, char32_t code, const TDefaultFormatSpec& spec);
		void FormatCString(string::TString& out, const char* value, char32_t code, const TDefaultFormatSpec& spec);
		void FormatWideString(string::TString& out, const wchar_t* value, char32_t code, const TDefaultFormatSpec& spec);
		void FormatUTF32String(string::TString& out, const char32_t* value, char32_t code, const TDefaultFormatSpec& spec);
		void FormatCharacter(string::TString& out, char value, char32_t code, const TDefaultFormatSpec& spec);
		void FormatCharacter(string::TString& out, wchar_t value, char32_t code, const TDefaultFormatSpec& spec);
		void FormatCharacter(string::TString& out, char32_t value, char32_t code, const TDefaultFormatSpec& spec);
	}

	constexpr bool ParseDefaultFormatSpec(TDefaultFormatSpec& out, const TFormatSpecView& text) noexcept
	{
		usys_t p = 0;
		if(p < text.Length() && (text[p] == '0' || (!detail::IsDigit(text[p]) && text[p] != '.')))
		{
			out.has_pad = true;
			out.pad_sign = text[p++];
		}

		while(p < text.Length() && detail::IsDigit(text[p]))
		{
			out.has_width = true;
			out.width = out.width * 10U + text[p] - '0';
			p++;
		}

		if(p < text.Length() && text[p] == '.')
		{
			out.has_precision = true;
			out.precision = 0;
			p++;
			if(p >= text.Length() || !detail::IsDigit(text[p]))
				return false;
			while(p < text.Length() && detail::IsDigit(text[p]))
			{
				out.precision = out.precision * 10U + text[p] - '0';
				p++;
			}
		}
		return p == text.Length();
	}

	template<typename T>
	requires ((std::is_integral_v<T> && !detail::IsCharacterType<T>) || std::is_enum_v<T>)
	struct TFormatter<T> : detail::TNumericFormatterBase
	{
		static void Format(string::TString& out, const T value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			const unsigned radix = Radix(code);
			if constexpr(std::is_enum_v<T>)
			{
				using value_t = std::underlying_type_t<T>;
				if constexpr(std::is_signed_v<value_t>)
					detail::FormatSigned(out, (s64_t)(value_t)value, spec, radix);
				else
					detail::FormatUnsigned(out, (u64_t)(value_t)value, spec, radix);
			}
			else if constexpr(std::is_signed_v<T>)
				detail::FormatSigned(out, (s64_t)value, spec, radix);
			else
				detail::FormatUnsigned(out, (u64_t)value, spec, radix);
		}
	};

	template<typename T>
	requires std::is_floating_point_v<T>
	struct TFormatter<T> : detail::TNumericFormatterBase
	{
		static void Format(string::TString& out, const T value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			detail::FormatFloat(out, (double)value, spec, Radix(code));
		}
	};

	template<>
	struct TFormatter<bcd::TBCD> : detail::TNumericFormatterBase
	{
		static void Format(string::TString& out, const bcd::TBCD& value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			detail::FormatBCD(out, value, spec, Radix(code));
		}
	};

	template<typename T>
	requires (std::derived_from<T, bcd::TBCD> && !std::same_as<T, bcd::TBCD>)
	struct TFormatter<T> : detail::TNumericFormatterBase
	{
		static void Format(string::TString& out, const T& value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			detail::FormatBCD(out, static_cast<const bcd::TBCD&>(value), spec, Radix(code));
		}
	};

	template<typename T>
	requires (!std::derived_from<T, bcd::TBCD> && requires(const T& value) { { value.ToBCD() } -> std::convertible_to<bcd::TBCD>; })
	struct TFormatter<T> : detail::TNumericFormatterBase
	{
		static void Format(string::TString& out, const T& value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			detail::FormatBCD(out, value.ToBCD(), spec, Radix(code));
		}
	};

	template<typename T>
	requires detail::IsCharacterType<T>
	struct TFormatter<T> : detail::TNumericFormatterBase
	{
		static constexpr bool Supports(const char32_t code) noexcept
		{
			return code == 's' || code == 'q' || code == 'c' || detail::TNumericFormatterBase::Supports(code);
		}

		static constexpr unsigned Radix(const char32_t code) noexcept
		{
			return detail::TNumericFormatterBase::Radix(code);
		}

		static constexpr bool Validate(const char32_t code, const TDefaultFormatSpec& spec) noexcept
		{
			return !spec.has_precision || code != 'c';
		}

		static void Format(string::TString& out, const T value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			if(detail::IsNumericCode(code))
			{
				if constexpr(std::same_as<T, char32_t>)
					detail::FormatUnsigned(out, value, spec, Radix(code));
				else
					detail::FormatUnsigned(out, (std::make_unsigned_t<T>)value, spec, Radix(code));
			}
			else
				detail::FormatCharacter(out, value, code, spec);
		}
	};

	template<>
	struct TFormatter<string::TString>
	{
		static constexpr bool Supports(const char32_t code) noexcept { return code == 's' || code == 'q'; }
		static void Format(string::TString& out, const string::TString& value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			detail::FormatString(out, value, code, spec);
		}
	};

	template<>
	struct TFormatter<string::TStringView>
	{
		static constexpr bool Supports(const char32_t code) noexcept { return code == 's' || code == 'q'; }
		static void Format(string::TString& out, const string::TStringView& value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			detail::FormatStringView(out, value, code, spec);
		}
	};


	template<typename T>
	requires detail::IsStringPointer<T>
	struct TFormatter<T>
	{
		static constexpr bool Supports(const char32_t code) noexcept { return code == 's' || code == 'q'; }

		static void Format(string::TString& out, const T value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			if constexpr(std::is_same_v<T, const char*> || std::is_same_v<T, char*>)
				detail::FormatCString(out, value, code, spec);
			else if constexpr(std::is_same_v<T, const wchar_t*> || std::is_same_v<T, wchar_t*>)
				detail::FormatWideString(out, value, code, spec);
			else
				detail::FormatUTF32String(out, value, code, spec);
		}
	};

	template<>
	struct TFormatter<std::nullptr_t>
	{
		static constexpr bool Supports(const char32_t code) noexcept { return code == 'p'; }
		static constexpr unsigned Radix(const char32_t code) noexcept { return code == 'p' ? 16U : 0U; }
		static constexpr bool Validate(const char32_t, const TDefaultFormatSpec& spec) noexcept { return !spec.has_precision; }
		static void Format(string::TString& out, const std::nullptr_t, const char32_t code, const TDefaultFormatSpec& spec)
		{
			detail::FormatUnsigned(out, 0, spec, Radix(code), true);
		}
	};

	template<typename T>
	requires (std::is_pointer_v<T> && !detail::IsStringPointer<T>)
	struct TFormatter<T>
	{
		static constexpr bool Supports(const char32_t code) noexcept { return code == 'p'; }
		static constexpr unsigned Radix(const char32_t code) noexcept { return code == 'p' ? 16U : 0U; }
		static constexpr bool Validate(const char32_t, const TDefaultFormatSpec& spec) noexcept { return !spec.has_precision; }
		static void Format(string::TString& out, const T value, const char32_t code, const TDefaultFormatSpec& spec)
		{
			detail::FormatUnsigned(out, (u64_t)(usys_t)value, spec, Radix(code), true);
		}
	};

	/**
	 * Validated native UTF-32 format literal. TString::Format() accepts only
	 * `U"..."` literals. Source-code UTF-8 is therefore converted to char32_t by
	 * the compiler before this parser runs; every index is one Unicode code point.
	 *
	 * The conversion code is the first ASCII letter after `%`. Therefore a
	 * normal spec cannot contain letters:
	 *
	 *   %10.6d   -> default spec "10.6", code 'd'
	 *   %hm      -> empty spec, code 'h', followed by literal "m"
	 *   %{h}m    -> custom spec "h", code 'm'
	 *
	 * Braces make the enclosed text opaque to the generic parser and require a
	 * formatter-provided ParseSpec(). `%%` emits one percent sign.
	 */
	template<typename... TArgs>
	class TFormatString
	{
		private:
			template<typename T>
			struct TVariable
			{
				usys_t begin = NEG1;
				usys_t end = NEG1;
				char32_t code = U'\0';
				detail::formatter_spec_t<T> spec = {};
				bool valid = false;
			};

			struct TUncheckedTag {};

			const char32_t* literal = nullptr;
			usys_t length = 0;
			std::tuple<TVariable<TArgs>...> variables = {};

			constexpr char32_t Character(const usys_t index) const noexcept { return literal[index]; }

			constexpr TFormatSpecView SpecView(const usys_t begin, const usys_t end, const bool braced) const noexcept
			{
				return TFormatSpecView{literal + begin, end - begin, braced};
			}

			template<typename T>
			constexpr TVariable<T> Find(const usys_t start) const noexcept
			{
				TVariable<T> result;
				for(usys_t i = start; i < length; i++)
				{
					if(Character(i) != '%')
						continue;
					if(i + 1 >= length)
						return result;
					if(Character(i + 1) == '%')
					{
						i++;
						continue;
					}

					result.begin = i;
					usys_t p = i + 1;
					usys_t spec_begin = p;
					usys_t spec_end = p;
					bool braced = false;

					if(Character(p) == '{')
					{
						braced = true;
						spec_begin = ++p;
						while(p < length && Character(p) != '}')
							p++;
						if(p >= length)
							return result;
						spec_end = p++;
					}
					else
					{
						while(p < length && !detail::IsAsciiLetter(Character(p)))
							p++;
						spec_end = p;
					}

					if(p >= length || !detail::IsAsciiLetter(Character(p)))
						return result;

					result.code = Character(p);
					result.end = p + 1;
					if constexpr(detail::CFormatter<T>)
					{
						if(!TFormatter<T>::Supports(result.code))
							return result;
						if(!detail::ParseFormatterSpec<T>(result.spec, result.code, SpecView(spec_begin, spec_end, braced)))
							return result;
						if(!detail::ValidateFormatterSpec<T>(result.code, result.spec))
							return result;
						result.valid = true;
					}
					return result;
				}
				return result;
			}

			constexpr bool HasUnescapedVariable(const usys_t start) const noexcept
			{
				for(usys_t i = start; i < length; i++)
				{
					if(Character(i) != '%')
						continue;
					if(i + 1 < length && Character(i + 1) == '%')
					{
						i++;
						continue;
					}
					return true;
				}
				return false;
			}

			template<usys_t I>
			constexpr bool ValidateFrom(const usys_t pos) const noexcept
			{
				if constexpr(I == sizeof...(TArgs))
					return !HasUnescapedVariable(pos);
				else
				{
					using T = std::tuple_element_t<I, std::tuple<TArgs...>>;
					if constexpr(!detail::CFormatter<T>)
						return false;
					else
					{
						const TVariable<T> variable = Find<T>(pos);
						return variable.valid && ValidateFrom<I + 1>(variable.end);
					}
				}
			}

			template<usys_t I>
			consteval bool InitializeFrom(const usys_t pos)
			{
				if constexpr(I == sizeof...(TArgs))
					return !HasUnescapedVariable(pos);
				else
				{
					using T = std::tuple_element_t<I, std::tuple<TArgs...>>;
					if constexpr(!detail::CFormatter<T>)
						return false;
					else
					{
						const TVariable<T> variable = Find<T>(pos);
						if(!variable.valid)
							return false;
						std::get<I>(variables) = variable;
						return InitializeFrom<I + 1>(variable.end);
					}
				}
			}

			void AppendRaw(string::TString& out, const usys_t begin, const usys_t end) const
			{
				detail::AppendLiteral(out, literal, begin, end);
			}

			void AppendLiteral(string::TString& out, const usys_t begin, const usys_t end) const
			{
				usys_t chunk = begin;
				for(usys_t i = begin; i + 1 < end; i++)
				{
					if(Character(i) == '%' && Character(i + 1) == '%')
					{
						AppendRaw(out, chunk, i);
						detail::FormatCharacter(out, U'%', U'c', TDefaultFormatSpec{});
						i++;
						chunk = i + 1;
					}
				}
				AppendRaw(out, chunk, end);
			}

			template<usys_t I, typename T, typename... TRest>
			void RenderFrom(string::TString& out, usys_t& pos, const T& value, const TRest&... rest) const
			{
				using TArg = std::decay_t<const T>;
				const auto& variable = std::get<I>(variables);
				AppendLiteral(out, pos, variable.begin);
				TFormatter<TArg>::Format(out, value, variable.code, variable.spec);
				pos = variable.end;
				if constexpr(sizeof...(TRest) != 0)
					RenderFrom<I + 1>(out, pos, rest...);
			}

			consteval void RequireValid()
			{
				if(!InitializeFrom<0>(0))
					throw "invalid format literal or formatter/argument mismatch";
			}

			template<std::size_t N>
			consteval TFormatString(TUncheckedTag, const char32_t (&value)[N]) noexcept : literal(value), length(N - 1) {}

		public:
			template<std::size_t N>
			consteval TFormatString(const char32_t (&value)[N]) : literal(value), length(N - 1)
			{
				RequireValid();
			}

			constexpr bool IsValid() const noexcept
			{
				return ValidateFrom<0>(0);
			}

			template<typename... A>
			void RenderInto(string::TString& out, const A&... args) const
			{
				static_assert(sizeof...(A) == sizeof...(TArgs));
				usys_t pos = 0;
				if constexpr(sizeof...(A) != 0)
					RenderFrom<0>(out, pos, args...);
				AppendLiteral(out, pos, length);
			}

			template<typename... A, std::size_t N>
			friend consteval bool IsValidFormat(const char32_t (&value)[N]) noexcept;
	};

	template<typename... TArgs, std::size_t N>
	consteval bool IsValidFormat(const char32_t (&value)[N]) noexcept
	{
		const TFormatString<TArgs...> format(typename TFormatString<TArgs...>::TUncheckedTag{}, value);
		return format.IsValid();
	}
}
