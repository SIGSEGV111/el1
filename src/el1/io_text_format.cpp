#include "io_text_format.hpp"
#include "io_text_string.hpp"
#include "io_bcd.hpp"

namespace el1::io::text::format::detail
{
	using namespace io::text::string;


	static char32_t NumericSymbol(const unsigned digit, const bool upper_case)
	{
		static constexpr char LOWER[] = "0123456789abcdef";
		static constexpr char UPPER[] = "0123456789ABCDEF";
		return char32_t((upper_case ? UPPER : LOWER)[digit]);
	}

	static void AppendPadding(TString& out, const char32_t sign, const usys_t count)
	{
		for(usys_t i = 0; i < count; i++)
			out += sign;
	}

	static bool IsNumericPad(const unsigned radix, const bool upper_case, const char32_t pad)
	{
		for(unsigned i = 0; i < radix; i++)
			if(NumericSymbol(i, upper_case) == pad)
				return true;
		return false;
	}

	static void FormatMagnitude(TString& out, u64_t magnitude, const bool negative, const TDefaultFormatSpec& spec, const unsigned radix, const bool upper_case)
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
			out += '-';
		AppendPadding(out, pad, n_padding);
		if(negative && !IsNumericPad(radix, upper_case, pad))
			out += '-';

		for(usys_t i = n_digits; i > 0; i--)
			out += digits[i - 1];

		if(spec.has_precision && spec.precision != 0)
		{
			out += '.';
			AppendPadding(out, NumericSymbol(0, upper_case), spec.precision);
		}
	}

	template<typename T>
	static bool FormatSpecialBCD(TString& out, const T& value)
	{
		if constexpr(requires { value.IsNaN(); value.IsInfinity(); })
		{
			if(value.IsNaN())
			{
				out += "NAN";
				return true;
			}
			if(value.IsInfinity())
			{
				if(value.IsNegative())
					out += '-';
				out += "INF";
				return true;
			}
		}
		return false;
	}
	static const TNumberFormatter& NumberFormatter(const unsigned radix)
	{
		switch(radix)
		{
			case 2: return *TNumberFormatter::DEFAULT_BINARY;
			case 8: return *TNumberFormatter::DEFAULT_OCTAL;
			case 10: return *TNumberFormatter::DEFAULT_DECIMAL;
			case 16: return *TNumberFormatter::DEFAULT_HEXADECIMAL;
			default: EL_THROW(error::TLogicException);
		}
	}

	static TNumberFormatter MakeNumberFormatter(const unsigned radix, const TDefaultFormatSpec& spec)
	{
		TNumberFormatter formatter(NumberFormatter(radix));
		if(spec.has_pad)
		{
			formatter.config.integer_pad_sign = spec.pad_sign;
			formatter.config.decimal_pad_sign = spec.pad_sign;
		}
		if(spec.has_width)
			formatter.config.n_min_integer_places = spec.width;
		if(spec.has_precision)
			formatter.config.n_decimal_places = spec.precision;
		return formatter;
	}

	static TStringFormatter MakeStringFormatter(const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter(code == 'q' ? TStringFormatter::ASCII_QUOTED : TStringFormatter::PLAIN);
		if(spec.has_pad)
			formatter.config.pad_sign = spec.pad_sign;
		if(spec.has_width)
			formatter.config.n_min_length = spec.width;
		if(spec.has_precision)
			formatter.config.n_max_length = spec.precision;
		return formatter;
	}

	void AppendLiteral(TString& out, const char32_t* const data, const usys_t begin, const usys_t end)
	{
		for(usys_t i = begin; i < end; i++)
			out += char32_t((u32_t)data[i]);
	}

	void FormatSigned(TString& out, const s64_t value, const TDefaultFormatSpec& spec, const unsigned radix, const bool upper_case)
	{
		const bool negative = value < 0;
		const u64_t magnitude = negative ? (0U - (u64_t)value) : (u64_t)value;
		FormatMagnitude(out, magnitude, negative, spec, radix, upper_case);
	}

	void FormatUnsigned(TString& out, const u64_t value, const TDefaultFormatSpec& spec, const unsigned radix, const bool upper_case)
	{
		FormatMagnitude(out, value, false, spec, radix, upper_case);
	}

	void FormatFloat(TString& out, const double value, const TDefaultFormatSpec& spec, const unsigned radix)
	{
		TNumberFormatter formatter = MakeNumberFormatter(radix, spec);
		out += formatter.Format(value);
	}

	void FormatBCD(TString& out, const bcd::TBCD& input, const TDefaultFormatSpec& spec, const unsigned radix)
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
			out += '-';
		AppendPadding(out, pad, n_padding);
		if(value.IsNegative() && !IsNumericPad(radix, false, pad))
			out += '-';

		for(usys_t i = n_integer; i > 0; i--)
			out += NumericSymbol(value.Digit((ssys_t)i - 1), false);

		const usys_t n_significant_decimal = value.CountSignificantDecimalDigits();
		const usys_t n_decimal = spec.has_precision ? spec.precision : n_significant_decimal;
		if(n_decimal != 0)
		{
			out += '.';
			for(usys_t i = 0; i < n_decimal; i++)
				out += i < n_significant_decimal ? NumericSymbol(value.Digit(-(ssys_t)i - 1), false) : NumericSymbol(0, false);
		}
	}

	void FormatString(TString& out, const TString& value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter = MakeStringFormatter(code, spec);
		out += formatter.Format(value);
	}

	void FormatStringView(TString& out, const TStringView& value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter = MakeStringFormatter(code, spec);
		out += formatter.Format(value);
	}

	void FormatCString(TString& out, const char* const value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter = MakeStringFormatter(code, spec);
		out += formatter.Format(value);
	}

	void FormatWideString(TString& out, const wchar_t* const value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter = MakeStringFormatter(code, spec);
		out += formatter.Format(value);
	}

	void FormatUTF32String(TString& out, const char32_t* const value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter = MakeStringFormatter(code, spec);
		out += formatter.Format(TStringView::FromUnsafePointer(value, UTF32StringLength(value)));
	}

	void FormatCharacter(TString& out, const char value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter = MakeStringFormatter(code, spec);
		out += formatter.Format(value);
	}

	void FormatCharacter(TString& out, const wchar_t value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter = MakeStringFormatter(code, spec);
		out += formatter.Format(value);
	}

	void FormatCharacter(TString& out, const char32_t value, const char32_t code, const TDefaultFormatSpec& spec)
	{
		TStringFormatter formatter = MakeStringFormatter(code, spec);
		out += formatter.Format(value);
	}
}
