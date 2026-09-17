#pragma once

#include "io_bcd.hpp"
#include "io_text_string.hpp"

#include <cmath>
#include <optional>
#include <type_traits>

namespace el1::io::text::number
{
	using namespace io::types;
	using string::TStringView;

	constexpr int DigitValue(const char32_t chr) noexcept
	{
		const unsigned value = bcd::DigitValue(chr);
		return value < 36 ? static_cast<int>(value) : -1;
	}

	constexpr int DigitValue(const char32_t chr, const TStringView digits) noexcept
	{
		const unsigned value = bcd::DigitValue(chr, digits);
		return value < digits.Length() ? static_cast<int>(value) : -1;
	}

	constexpr bool IsDigit(const char32_t chr, const unsigned radix) noexcept
	{
		const int value = DigitValue(chr);
		return value >= 0 && static_cast<unsigned>(value) < radix;
	}

	inline bool IsDigit(const char32_t chr, const TStringView digits) noexcept
	{
		return DigitValue(chr, digits) >= 0;
	}

	namespace detail
	{
		inline bool IsValidDigitSet(const TStringView digits) noexcept
		{
			if(digits.Length() < 2 || digits.Length() > 256)
				return false;

			for(usys_t i = 0; i < digits.Length(); i++)
				for(usys_t j = 0; j < i; j++)
					if(digits[i] == digits[j])
						return false;
			return true;
		}

		template<typename T, bool IS_ENUM = std::is_enum_v<T>>
		struct TIntegerType { using type = T; };

		template<typename T>
		struct TIntegerType<T, true> { using type = std::underlying_type_t<T>; };
	}

	template<typename T>
	requires (std::is_integral_v<T> || std::is_enum_v<T>)
	std::optional<T> TryParseInteger(const TStringView text, const unsigned radix = 10) noexcept
	{
		if(radix != 2 && radix != 8 && radix != 10 && radix != 16)
			return std::nullopt;

		T value;
		return bcd::TBCD::ParseIntegerMSD(text, value, static_cast<bcd::digit_t>(radix))
			? std::optional<T>(value)
			: std::nullopt;
	}

	template<typename T>
	requires (std::is_integral_v<T> || std::is_enum_v<T>)
	std::optional<T> TryParseInteger(const TStringView text, const TStringView digits)
	{
		if(!detail::IsValidDigitSet(digits) || text.Length() == 0)
			return std::nullopt;

		usys_t begin = 0;
		if(text[0] == U'+' || text[0] == U'-')
			begin = 1;
		if(begin == text.Length())
			return std::nullopt;
		for(usys_t i = begin; i < text.Length(); i++)
			if(!IsDigit(text[i], digits))
				return std::nullopt;

		const bcd::TBCD value = bcd::TBCD::FromStringMSD(text, digits);
		using value_t = typename detail::TIntegerType<T>::type;
		value_t converted;
		if(!value.TryToInteger(converted))
			return std::nullopt;
		return static_cast<T>(converted);
	}

	template<typename T>
	requires (std::is_integral_v<T> || std::is_enum_v<T>)
	std::optional<T> TryParseHex(const TStringView text) noexcept
	{
		return TryParseInteger<T>(text, 16);
	}

	template<typename T>
	requires (std::is_integral_v<T> || std::is_enum_v<T>)
	std::optional<T> TryParseHex(const TStringView text, const TStringView digits)
	{
		return digits.Length() == 16 ? TryParseInteger<T>(text, digits) : std::nullopt;
	}

	template<typename T>
	requires std::is_floating_point_v<T>
	std::optional<T> TryParseFloating(const TStringView text, const unsigned radix = 10, const bool scientific_notation = false) noexcept
	{
		if(radix < 2 || radix > 36 || text.Length() == 0)
			return std::nullopt;

		usys_t pos = 0;
		if(text[pos] == U'+' || text[pos] == U'-')
			pos++;

		bool have_digit = false;
		bool have_decimal = false;
		for(; pos < text.Length(); pos++)
		{
			if(IsDigit(text[pos], radix))
			{
				have_digit = true;
				continue;
			}
			if(!have_decimal && text[pos] == U'.')
			{
				have_decimal = true;
				continue;
			}
			break;
		}
		if(!have_digit)
			return std::nullopt;

		if(scientific_notation && radix == 10 && pos < text.Length() && (text[pos] == U'e' || text[pos] == U'E'))
		{
			pos++;
			if(pos < text.Length() && (text[pos] == U'+' || text[pos] == U'-'))
				pos++;

			const usys_t exponent_begin = pos;
			while(pos < text.Length() && IsDigit(text[pos], 10))
				pos++;
			if(pos == exponent_begin)
				return std::nullopt;
		}
		if(pos != text.Length())
			return std::nullopt;

		const T value = static_cast<T>(bcd::TBCD::ParseDoubleMSD(text, static_cast<bcd::digit_t>(radix), 0, scientific_notation));
		return std::isfinite(value) ? std::optional<T>(value) : std::nullopt;
	}

	template<typename T>
	requires std::is_floating_point_v<T>
	std::optional<T> TryParseFloating(const TStringView text, const TStringView digits, const bool scientific_notation = false)
	{
		if(!detail::IsValidDigitSet(digits) || text.Length() == 0)
			return std::nullopt;

		usys_t pos = 0;
		if(text[pos] == U'+' || text[pos] == U'-')
			pos++;

		bool have_digit = false;
		bool have_decimal = false;
		for(; pos < text.Length(); pos++)
		{
			if(IsDigit(text[pos], digits))
			{
				have_digit = true;
				continue;
			}
			if(!have_decimal && text[pos] == U'.')
			{
				have_decimal = true;
				continue;
			}
			break;
		}
		if(!have_digit)
			return std::nullopt;

		const usys_t mantissa_end = pos;
		ssys_t exponent = 0;
		if(scientific_notation && digits.Length() == 10 && pos < text.Length() && (text[pos] == U'e' || text[pos] == U'E'))
		{
			const auto parsed_exponent = TryParseInteger<ssys_t>(text.SliceSL(pos + 1), digits);
			if(!parsed_exponent || *parsed_exponent > static_cast<ssys_t>(bcd::MAX_PRECISION) || *parsed_exponent < -static_cast<ssys_t>(bcd::MAX_PRECISION))
				return std::nullopt;
			exponent = *parsed_exponent;
			pos = text.Length();
		}
		if(pos != text.Length())
			return std::nullopt;

		const bcd::TBCD mantissa = bcd::TBCD::FromStringMSD(text.SliceBE(0, mantissa_end), digits);
		const long double scale = std::pow(static_cast<long double>(digits.Length()), static_cast<long double>(exponent));
		const T value = static_cast<T>(static_cast<long double>(mantissa.ToDouble()) * scale);
		return std::isfinite(value) ? std::optional<T>(value) : std::nullopt;
	}

	template<typename T>
	requires std::is_floating_point_v<T>
	std::optional<T> TryParseDecimal(const TStringView text, const bool scientific_notation = false) noexcept
	{
		return TryParseFloating<T>(text, 10, scientific_notation);
	}

	template<typename T>
	requires std::is_floating_point_v<T>
	std::optional<T> TryParseDecimal(const TStringView text, const TStringView digits, const bool scientific_notation = false)
	{
		return digits.Length() == 10 ? TryParseFloating<T>(text, digits, scientific_notation) : std::nullopt;
	}

	template<typename T>
	requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
	std::optional<T> TryParseNumber(const TStringView text, const unsigned radix = 10, const bool scientific_notation = false) noexcept
	{
		if constexpr(std::is_floating_point_v<T>)
			return TryParseFloating<T>(text, radix, scientific_notation);
		else
			return TryParseInteger<T>(text, radix);
	}

	template<typename T>
	requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
	std::optional<T> TryParseNumber(const TStringView text, const TStringView digits, const bool scientific_notation = false)
	{
		if constexpr(std::is_floating_point_v<T>)
			return TryParseFloating<T>(text, digits, scientific_notation);
		else
			return TryParseInteger<T>(text, digits);
	}
}
