#include "io_text_format.hpp"
#include "io_text_string.hpp"
#include "io_bcd.hpp"

#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>

namespace el1::io::text::format::detail
{
	using namespace io::text::string;

	static char32_t NumericSymbol(const unsigned digit, const bool upper_case)
	{
		static constexpr char LOWER[] = "0123456789abcdef";
		static constexpr char UPPER[] = "0123456789ABCDEF";
		return char32_t((upper_case ? UPPER : LOWER)[digit]);
	}

	static void AppendPadding(IFormatSink& out, const char32_t sign, const usys_t count)
	{
		char32_t buffer[64];
		for(char32_t& chr : buffer)
			chr = sign;
		for(usys_t left = count; left != 0;)
		{
			const usys_t n = util::Min<usys_t>(left, sizeof(buffer) / sizeof(buffer[0]));
			out.Append(buffer, n);
			left -= n;
		}
	}

	static bool IsNumericPad(const unsigned radix, const bool upper_case, const char32_t pad)
	{
		for(unsigned i = 0; i < radix; i++)
			if(NumericSymbol(i, upper_case) == pad)
				return true;
		return false;
	}

	static void FormatMagnitude(IFormatSink& out, u64_t magnitude, const bool negative, const TDefaultFormatSpec& spec, const unsigned radix, const bool upper_case)
	{
		char32_t digits[64] = {};
		usys_t n_digits = 0;

		do
		{
			digits[n_digits++] = NumericSymbol((unsigned)(magnitude % radix), upper_case);
			magnitude /= radix;
		}
		while(magnitude != 0);

		const usys_t width = spec.has_width ? spec.width : 0;
		const char32_t pad = spec.has_pad ? spec.pad_sign : U' ';
		const usys_t n_padding = width > n_digits ? width - n_digits : 0;
		if(negative && IsNumericPad(radix, upper_case, pad))
			out.Append(U'-');
		AppendPadding(out, pad, n_padding);
		if(negative && !IsNumericPad(radix, upper_case, pad))
			out.Append(U'-');

		char32_t forward[64];
		for(usys_t i = 0; i < n_digits; i++)
			forward[i] = digits[n_digits - i - 1];
		out.Append(forward, n_digits);

		if(spec.has_precision && spec.precision != 0)
		{
			out.Append(U'.');
			AppendPadding(out, NumericSymbol(0, upper_case), spec.precision);
		}
	}

	template<typename T>
	static bool FormatSpecialBCD(IFormatSink& out, const T& value)
	{
		if constexpr(requires { value.IsNaN(); value.IsInfinity(); })
		{
			if(value.IsNaN())
			{
				out.Append(U"NAN");
				return true;
			}
			if(value.IsInfinity())
			{
				if(value.IsNegative())
					out.Append(U'-');
				out.Append(U"INF");
				return true;
			}
		}
		return false;
	}

	void AppendLiteral(IFormatSink& out, const char32_t* const data, const usys_t begin, const usys_t end)
	{
		if(end > begin)
			out.Append(data + begin, end - begin);
	}

	void FormatSigned(IFormatSink& out, const s64_t value, const TDefaultFormatSpec& spec, const unsigned radix, const bool upper_case)
	{
		const bool negative = value < 0;
		const u64_t magnitude = negative ? (0U - (u64_t)value) : (u64_t)value;
		FormatMagnitude(out, magnitude, negative, spec, radix, upper_case);
	}

	void FormatUnsigned(IFormatSink& out, const u64_t value, const TDefaultFormatSpec& spec, const unsigned radix, const bool upper_case)
	{
		FormatMagnitude(out, value, false, spec, radix, upper_case);
	}

	static void AppendAsciiNumber(IFormatSink& out, const char* begin, const char* end, const TDefaultFormatSpec& spec)
	{
		const char* integer_begin = begin;
		const bool negative = integer_begin != end && *integer_begin == '-';
		if(negative)
			integer_begin++;

		const char* integer_end = integer_begin;
		while(integer_end != end && *integer_end != '.' && *integer_end != 'e' && *integer_end != 'E')
			integer_end++;

		const usys_t n_integer = (usys_t)(integer_end - integer_begin);
		const usys_t width = spec.has_width ? spec.width : 0;
		const char32_t pad = spec.has_pad ? spec.pad_sign : U' ';
		const usys_t n_padding = width > n_integer ? width - n_integer : 0;
		const bool numeric_pad = pad >= U'0' && pad <= U'9';
		if(negative && numeric_pad)
			out.Append(U'-');
		AppendPadding(out, pad, n_padding);
		if(negative && !numeric_pad)
			out.Append(U'-');

		char32_t converted[512];
		usys_t n = 0;
		for(const char* p = integer_begin; p != end; ++p)
			converted[n++] = (unsigned char)*p;
		out.Append(converted, n);
	}

	void FormatFloat(IFormatSink& out, const double value, const TDefaultFormatSpec& spec, const unsigned radix)
	{
		if(std::isnan(value))
		{
			out.Append(U"NAN");
			return;
		}
		if(std::isinf(value))
		{
			if(std::signbit(value))
				out.Append(U'-');
			out.Append(U"INF");
			return;
		}

		if(radix == 10)
		{
			char buffer[512];
			std::to_chars_result result;
			if(spec.has_precision)
				result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, (int)spec.precision);
			else
				result = std::to_chars(buffer, buffer + sizeof(buffer), value);
			EL_ERROR(result.ec != std::errc{}, error::TException, "floating point formatting failed");
			AppendAsciiNumber(out, buffer, result.ptr, spec);
			return;
		}

		// Non-decimal floating output uses the exact IEEE value through TBCD.
		const usys_t n_decimal = spec.has_precision ? spec.precision + 1U : 64U;
		bcd::TBCD converted(value, (bcd::digit_t)radix, 1100U, n_decimal);
		FormatBCD(out, converted, spec, radix);
	}

	void FormatBCD(IFormatSink& out, const bcd::TBCD& input, const TDefaultFormatSpec& spec, const unsigned radix)
	{
		if(FormatSpecialBCD(out, input))
			return;

		bcd::TBCD value = input.Base() == radix ? input : bcd::TBCD(input, (bcd::digit_t)radix);
		if(spec.has_precision && spec.precision < value.CountDecimal())
		{
			using precision_t = decltype(value.CountDecimal());
			value.Round((precision_t)spec.precision, math::ERoundingMode::TO_NEAREST);
		}

		const usys_t n_integer = util::Max<usys_t>(1, value.CountSignificantIntegerDigits());
		const usys_t width = spec.has_width ? spec.width : 0;
		const char32_t pad = spec.has_pad ? spec.pad_sign : U' ';
		const usys_t n_padding = width > n_integer ? width - n_integer : 0;
		if(value.IsNegative() && IsNumericPad(radix, false, pad))
			out.Append(U'-');
		AppendPadding(out, pad, n_padding);
		if(value.IsNegative() && !IsNumericPad(radix, false, pad))
			out.Append(U'-');

		char32_t integer[64];
		for(usys_t offset = 0; offset < n_integer;)
		{
			const usys_t n = util::Min<usys_t>(64, n_integer - offset);
			for(usys_t i = 0; i < n; i++)
				integer[i] = NumericSymbol(value.Digit((ssys_t)(n_integer - offset - i - 1)), false);
			out.Append(integer, n);
			offset += n;
		}

		const usys_t n_significant_decimal = value.CountSignificantDecimalDigits();
		const usys_t n_decimal = spec.has_precision ? spec.precision : n_significant_decimal;
		if(n_decimal != 0)
		{
			out.Append(U'.');
			char32_t decimal[64];
			for(usys_t offset = 0; offset < n_decimal;)
			{
				const usys_t n = util::Min<usys_t>(64, n_decimal - offset);
				for(usys_t i = 0; i < n; i++)
				{
					const usys_t index = offset + i;
					decimal[i] = index < n_significant_decimal ? NumericSymbol(value.Digit(-(ssys_t)index - 1), false) : U'0';
				}
				out.Append(decimal, n);
				offset += n;
			}
		}
	}

	static void FormatPlainString(IFormatSink& out, const TStringView value, const TDefaultFormatSpec& spec)
	{
		const usys_t n_text = spec.has_precision ? util::Min(value.Length(), spec.precision) : value.Length();
		const usys_t width = spec.has_width ? spec.width : 0;
		const char32_t pad = spec.has_pad ? spec.pad_sign : U' ';
		if(width > n_text)
			AppendPadding(out, pad, width - n_text);
		if(n_text != 0)
			out.Append(value.Data(), n_text);
	}

	static char32_t BestQuote(const TStringView value)
	{
		usys_t single = 0;
		usys_t dbl = 0;
		for(const char32_t chr : value)
		{
			single += chr == U'\'';
			dbl += chr == U'"';
		}
		return single <= dbl ? U'\'' : U'"';
	}

	static void FormatQuotedString(IFormatSink& out, const TStringView value, const TDefaultFormatSpec& spec)
	{
		const char32_t quote = BestQuote(value);
		usys_t escaped_length = 2;
		for(const char32_t chr : value)
			escaped_length += (chr == quote || chr == U'\\') ? 2U : 1U;
		const usys_t width = spec.has_width ? spec.width : 0;
		const char32_t pad = spec.has_pad ? spec.pad_sign : U' ';
		if(width > escaped_length)
			AppendPadding(out, pad, width - escaped_length);
		out.Append(quote);
		for(const char32_t chr : value)
		{
			if(chr == quote || chr == U'\\')
				out.Append(U'\\');
			out.Append(chr);
		}
		out.Append(quote);
	}

	void FormatStringView(IFormatSink& out, const TStringView& value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		if(code == U'q')
			FormatQuotedString(out, value, spec);
		else
			FormatPlainString(out, value, spec);
	}

	void FormatString(IFormatSink& out, const TString& value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		FormatStringView(out, value.View(), code, spec);
	}

	void FormatCString(IFormatSink& out, const char* const value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		const TString converted(value);
		FormatStringView(out, converted.View(), code, spec);
	}

	void FormatWideString(IFormatSink& out, const wchar_t* const value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		const TString converted(value);
		FormatStringView(out, converted.View(), code, spec);
	}

	void FormatUTF32String(IFormatSink& out, const char32_t* const value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		FormatStringView(out, TStringView::FromUnsafePointer(value, encoding::UTF32StringLength(value)), code, spec);
	}

	void FormatCharacter(IFormatSink& out, const char value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		const TString converted(&value, 1);
		FormatStringView(out, converted.View(), code, spec);
	}

	void FormatCharacter(IFormatSink& out, const wchar_t value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		const TString converted(&value, 1);
		FormatStringView(out, converted.View(), code, spec);
	}

	void FormatCharacter(IFormatSink& out, const char32_t value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		if(code == U'c')
		{
			const usys_t width = spec.has_width ? spec.width : 0;
			if(width > 1)
				AppendPadding(out, spec.has_pad ? spec.pad_sign : U' ', width - 1);
			out.Append(value);
		}
		else
			FormatStringView(out, TStringView::FromUnsafePointer(&value, 1), code, spec);
	}
}
