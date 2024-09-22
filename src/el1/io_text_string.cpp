#include "io_text_string.hpp"
#include "io_text_encoding_utf8.hpp"
#include <string.h>
#include <iostream>
#include <math.h>

namespace el1::io::text::string
{
	extern const array_t<const TUTF32> WHITESPACE_CHARS;

	TString TUndefineFormatException::Message() const
	{
		return TString::Format("cannot convert from argument type %q to requested format %q", argument_type, format_name);
	}

	error::IException* TUndefineFormatException::Clone() const
	{
		return new TUndefineFormatException(*this);
	}

	TUndefineFormatException::TUndefineFormatException(const char* const argument_type, const char* const format_name) : argument_type(argument_type), format_name(format_name) {}

	TString TMissingFormatVariableException::Message() const
	{
		return TString::Format("unable to find a placeholder for a format variable in format string %q", format);
	}

	error::IException* TMissingFormatVariableException::Clone() const
	{
		return new TMissingFormatVariableException(*this);
	}

	TString TTooManyFormatVariablesException::Message() const
	{
		return TString::Format("format string %q contains more placeholders than variables were passed as function arguments", format);
	}

	error::IException* TTooManyFormatVariablesException::Clone() const
	{
		return new TTooManyFormatVariablesException(*this);
	}

	// LCOV_EXCL_START
	TString IFormatter::Format(const char* const) const    { EL_THROW(TUndefineFormatException, "char*",    FormatName()); }
	TString IFormatter::Format(const wchar_t* const) const { EL_THROW(TUndefineFormatException, "wchar_t*", FormatName()); }
	TString IFormatter::Format(const TString&) const       { EL_THROW(TUndefineFormatException, "TString",  FormatName()); }
	TString IFormatter::Format(const char) const           { EL_THROW(TUndefineFormatException, "char",     FormatName()); }
	TString IFormatter::Format(const wchar_t) const        { EL_THROW(TUndefineFormatException, "wchar_t",  FormatName()); }
	TString IFormatter::Format(const TUTF32) const         { EL_THROW(TUndefineFormatException, "TUTF32",   FormatName()); }
	TString IFormatter::Format(const s8_t) const           { EL_THROW(TUndefineFormatException, "s8_t",     FormatName()); }
	TString IFormatter::Format(const u8_t) const           { EL_THROW(TUndefineFormatException, "u8_t",     FormatName()); }
	TString IFormatter::Format(const s16_t) const          { EL_THROW(TUndefineFormatException, "s16_t",    FormatName()); }
	TString IFormatter::Format(const u16_t) const          { EL_THROW(TUndefineFormatException, "u16_t",    FormatName()); }
	TString IFormatter::Format(const s32_t) const          { EL_THROW(TUndefineFormatException, "s32_t",    FormatName()); }
	TString IFormatter::Format(const u32_t) const          { EL_THROW(TUndefineFormatException, "u32_t",    FormatName()); }
	TString IFormatter::Format(const s64_t) const          { EL_THROW(TUndefineFormatException, "s64_t",    FormatName()); }
	TString IFormatter::Format(const u64_t) const          { EL_THROW(TUndefineFormatException, "u64_t",    FormatName()); }
	TString IFormatter::Format(const double) const         { EL_THROW(TUndefineFormatException, "double",   FormatName()); }
	TString IFormatter::Format(const void* const, const usys_t) const { EL_THROW(TUndefineFormatException, "void*", FormatName()); }
	// LCOV_EXCL_STOP

	IFormatter::~IFormatter()
	{
	}

	/******************************************/

	const TNumberFormatter* TNumberFormatter::DEFAULT_OCTAL = &TNumberFormatter::PLAIN_OCTAL;
	const TNumberFormatter* TNumberFormatter::DEFAULT_DECIMAL = &TNumberFormatter::PLAIN_DECIMAL_US_EN;
	const TNumberFormatter* TNumberFormatter::DEFAULT_HEXADECIMAL = &TNumberFormatter::PLAIN_HEXADECIMAL_LOWER_US_EN;
	const TNumberFormatter* TNumberFormatter::DEFAULT_BINARY = &TNumberFormatter::PLAIN_BINARY_US_EN;
	const TNumberFormatter* TNumberFormatter::DEFAULT_ADDRESS = &TNumberFormatter::PLAIN_HEXADECIMAL_UPPER_US_EN;

		const TNumberFormatter  TNumberFormatter::PLAIN_OCTAL({
		.symbols = &OCTAL_SYMBOLS,
		.prefix = "",
		.suffix = "",
		.sign_placement = EPlacement::START,
		.force_sign = false,
		.decimal_point_sign = '.',
		.grouping_sign = TUTF32::TERMINATOR,
		.integer_pad_sign = ' ',
		.decimal_pad_sign = '0',
		.negative_sign = '-',
		.n_digits_per_group = 0,
		.n_decimal_places = -1U,
		.n_min_integer_places = -1U,
	});

	const TNumberFormatter  TNumberFormatter::PLAIN_DECIMAL_US_EN({
		.symbols = &DECIMAL_SYMBOLS,
		.prefix = "",
		.suffix = "",
		.sign_placement = EPlacement::START,
		.force_sign = false,
		.decimal_point_sign = '.',
		.grouping_sign = TUTF32::TERMINATOR,
		.integer_pad_sign = ' ',
		.decimal_pad_sign = '0',
		.negative_sign = '-',
		.n_digits_per_group = 0,
		.n_decimal_places = -1U,
		.n_min_integer_places = -1U,
	});

	const TNumberFormatter TNumberFormatter::PLAIN_HEXADECIMAL_UPPER_US_EN({
		.symbols = &HEXADECIMAL_SYMBOLS_UC,
		.prefix = "",
		.suffix = "",
		.sign_placement = EPlacement::START,
		.force_sign = false,
		.decimal_point_sign = '.',
		.grouping_sign = TUTF32::TERMINATOR,
		.integer_pad_sign = ' ',
		.decimal_pad_sign = '0',
		.negative_sign = '-',
		.n_digits_per_group = 0,
		.n_decimal_places = -1U,
		.n_min_integer_places = -1U,
	});

	const TNumberFormatter TNumberFormatter::PLAIN_HEXADECIMAL_LOWER_US_EN({
		.symbols = &HEXADECIMAL_SYMBOLS_LC,
		.prefix = "",
		.suffix = "",
		.sign_placement = EPlacement::START,
		.force_sign = false,
		.decimal_point_sign = '.',
		.grouping_sign = TUTF32::TERMINATOR,
		.integer_pad_sign = ' ',
		.decimal_pad_sign = '0',
		.negative_sign = '-',
		.n_digits_per_group = 0,
		.n_decimal_places = -1U,
		.n_min_integer_places = -1U,
	});

	const TNumberFormatter TNumberFormatter::PLAIN_BINARY_US_EN({
		.symbols = &BINARY_SYMBOLS,
		.prefix = "",
		.suffix = "",
		.sign_placement = EPlacement::START,
		.force_sign = false,
		.decimal_point_sign = '.',
		.grouping_sign = TUTF32::TERMINATOR,
		.integer_pad_sign = ' ',
		.decimal_pad_sign = '0',
		.negative_sign = '-',
		.n_digits_per_group = 0,
		.n_decimal_places = -1U,
		.n_min_integer_places = -1U,
	});

	const char* TNumberFormatter::FormatName() const
	{
		return "number";
	}

	TString TNumberFormatter::Format(const s8_t value) const
	{
		return Format((s64_t)value);
	}

	TString TNumberFormatter::Format(const u8_t value) const
	{
		return Format((u64_t)value);
	}

	TString TNumberFormatter::Format(const s16_t value) const
	{
		return Format((s64_t)value);
	}

	TString TNumberFormatter::Format(const u16_t value) const
	{
		return Format((u64_t)value);
	}

	TString TNumberFormatter::Format(const s32_t value) const
	{
		return Format((s64_t)value);
	}

	TString TNumberFormatter::Format(const u32_t value) const
	{
		return Format((u64_t)value);
	}

	TString TNumberFormatter::Format(const s64_t value) const
	{
		const bool is_negative = value < 0;

		TString out = MakeIntegerPart(value * (is_negative ? -1 : 1), is_negative);

		if(config.n_decimal_places != -1U && config.n_decimal_places != 0)
		{
			if(config.decimal_point_sign != TUTF32::TERMINATOR)
				out.chars.Append(config.decimal_point_sign);
			out.chars.Inflate(config.n_decimal_places, (*config.symbols)[0]);
		}

		if((is_negative || config.force_sign) && config.sign_placement == EPlacement::END)
			out += config.negative_sign;

		return out;
	}

	TString TNumberFormatter::Format(const u64_t value) const
	{
		TString out = MakeIntegerPart(value, false);

		if(config.n_decimal_places != -1U && config.n_decimal_places != 0)
		{
			if(config.decimal_point_sign != TUTF32::TERMINATOR)
				out.chars.Append(config.decimal_point_sign);
			out.chars.Inflate(config.n_decimal_places, (*config.symbols)[0]);
		}

		if(config.force_sign && config.sign_placement == EPlacement::END)
			out += config.negative_sign;

		return out;
	}

	TString TNumberFormatter::Format(const double _value) const
	{
		if(isnan(_value)) return "NAN";
		if(isinf(_value)) return "INF";
		EL_ERROR(!isfinite(_value), TLogicException);

		double value = _value;
		TString out = MakeDecimalPart(value);

		if(config.decimal_point_sign != TUTF32::TERMINATOR && out.Length() > 0)
			out.chars.Insert(0, config.decimal_point_sign);

		const bool is_negative = value < 0.0;
		const double abs_value = is_negative ? -value : value;
		out.Insert(0, MakeIntegerPart((u64_t)abs_value, is_negative));

		return out;
	}

	TString TNumberFormatter::MakeDecimalPart(double& value) const
	{
		EL_ERROR(!isfinite(value), TInvalidArgumentException, "value", "value must be a finite float number");
		const TList< TUTF32>& symbols = *config.symbols;
		const u64_t n_symbols = symbols.Count();
		EL_ERROR(n_symbols < 2, TInvalidArgumentException, "config.symbols", "we need at least two symbols to format a number");
		EL_ERROR(n_symbols >= -2U, TInvalidArgumentException, "config.symbols", "too many symbols");

		const bool is_negative = value < 0.0;
		const double abs_value = is_negative ? -value : value;
		double remainder = abs_value - (u64_t)abs_value;

		TList<TUTF32> digits;

		for(unsigned i = 0; (config.n_decimal_places != -1U && i < config.n_decimal_places + 1) || (config.n_decimal_places == -1U && remainder != 0); i++)
		{
			remainder *= (double)n_symbols;
			const u32_t n = (u32_t)remainder;
			remainder -= n;
			digits.Append(n);
		}

		if(config.n_decimal_places != -1U)
		{
			// stopped because i >= n_decimal_places+1
			// round to n_decimal_places
			if(digits[-1].code >= n_symbols / 2)
			{
				// round away from zero
				digits.Cut(0,1);

				ssys_t i;
				for(i = digits.Count() - 1; i >= 0; i--)
				{
					digits[i].code++;
					if(digits[i].code >= n_symbols)
						digits[i].code = 0;
					else
						break;
				}

				if(i < 0)
				{
					if(is_negative)
						value -= 1.0;
					else
						value += 1.0;
				}
			}
			else
			{
				// round towards zero
				// just cutoff last
				digits.Cut(0,1);
			}
		}

		for(TUTF32& digit : digits)
			digit = symbols[digit.code];
		TString out;
		out.chars = std::move(digits);
		return out;
	}

	TString TNumberFormatter::MakeIntegerPart(u64_t value, const bool is_negative) const
	{
		const TList< TUTF32>& symbols = *config.symbols;
		const u64_t n_symbols = symbols.Count();
		EL_ERROR(n_symbols < 2, TInvalidArgumentException, "config.symbols", "we need at least two symbols to format a number");
		TString digits;

		unsigned n_group = config.n_digits_per_group;

		if(value == 0)
			digits += symbols[0];

		while(value > 0)
		{
			digits += symbols[value % n_symbols];
			value /= n_symbols;

			if(config.n_digits_per_group != -1U && config.grouping_sign != TUTF32::TERMINATOR)
			{
				if(--n_group == 0)
				{
					digits += config.grouping_sign;
					n_group = config.n_digits_per_group;
				}
			}
		}

		if((is_negative || config.force_sign) && config.sign_placement == EPlacement::START)
			digits += config.negative_sign;

		if(config.n_min_integer_places != -1U && config.n_min_integer_places != 0)
			digits.Pad(config.integer_pad_sign, config.n_min_integer_places, EPlacement::END);

		digits.Reverse();
		return digits;
	}

	/******************************************/

	const TStringFormatter TStringFormatter::PLAIN({
		.prefix = "",
		.suffix = "",
		.n_min_length = 0,
		.n_max_length = -1U,
		.pad_sign = ' ',
		.align = EPlacement::START,
		.quote_symbols = nullptr,
		.escape_symbol = TUTF32::TERMINATOR,
	});

	const TStringFormatter TStringFormatter::ASCII_QUOTED({
		.prefix = "",
		.suffix = "",
		.n_min_length = 0,
		.n_max_length = -1U,
		.pad_sign = ' ',
		.align = EPlacement::START,
		.quote_symbols = &ASCII_QUOTE_SYMBOLS,
		.escape_symbol = '\\',
	});

	const char* TStringFormatter::FormatName() const
	{
		return "string";
	}

	TString TStringFormatter::Format(const char* const value) const
	{
		return Format(TString(value));
	}

	TString TStringFormatter::Format(const wchar_t* const value) const
	{
		return Format(TString(value));
	}

	TString TStringFormatter::Format(const TString& value) const
	{
		TString s = config.prefix + value + config.suffix;

		if(config.quote_symbols != nullptr)
		{
			const TUTF32 quote_symbol = DetectBestQuoteSymbol(s);
			s.Quote(quote_symbol, config.escape_symbol == TUTF32::TERMINATOR ? quote_symbol : config.escape_symbol);
		}

		s.Pad(config.pad_sign, config.n_min_length, config.align);
		if(s.Length() > config.n_max_length)
			s.Truncate(config.n_max_length);

		return s;
	}

	TString TStringFormatter::Format(const char value) const
	{
		return Format(TString(&value, 1));
	}

	TString TStringFormatter::Format(const wchar_t value) const
	{
		return Format(TString(&value, 1));
	}

	TString TStringFormatter::Format(const TUTF32 value) const
	{
		return Format(TString(&value, 1));
	}

	TUTF32 TStringFormatter::DetectBestQuoteSymbol(const TString& str) const
	{
		auto symbols = *config.quote_symbols;
		usys_t counts[symbols.Count()];
		memset(&counts, 0, sizeof(counts));

		for(usys_t i = 0; i < str.Length(); i++)
			for(usys_t j = 0; j < symbols.Count(); j++)
				if(str[i] == symbols[j])
				{
					counts[j]++;
					break;
				}

		usys_t idx_smallest = 0;
		for(usys_t i = 1; i < symbols.Count(); i++)
			if(counts[i] < counts[idx_smallest])
				idx_smallest = i;

		return symbols[idx_smallest];
	}

	/******************************************/

	TFormatVariable TFormatVariable::Find(const TString& str, const usys_t start_pos)
	{
		for(usys_t i = start_pos + 1; i < str.Length(); i++)
		{
			if(str[i-1] == L'%')
			{
				if(str[i] != L'%')
				{
					usys_t j = i;
					for(; j < str.Length() && !str[j].IsAsciiAlpha(); j++);

					EL_ERROR(j >= str.Length(), TInvalidArgumentException,"format", "invalid format string; format variable type missing or unescaped '%' encoutered");

					if(str[j] == 'b' || str[j] == 'd' || str[j] == 'x' || str[j] == 'p' || str[j] == 's' || str[j] == 'q' || str[j] == 'c')
					{
						const char format_char = (char)str[j].code;
						const TString arg_str = str.SliceBE(i, j);
						TFormatVariable v = TFormatVariable::Parse(format_char, arg_str);
						v.pos = i - 1;
						v.len = j - i + 2;
						return v;
					}
					else
					{
						EL_THROW(TInvalidArgumentException, "format", "format code must be one of: 'b', 'd', 'x', 'p', 's', 'q' or 'c'");
					}
				}
				else
					i++;
			}
		}

		return TFormatVariable(false, nullptr);
	}

	TFormatVariable TFormatVariable::Parse(const TUTF32 format_char, const TString& arg_str)
	{
		switch(format_char.code)
		{
			case 'b': return ParseNumberFormatterArguments(arg_str, TNumberFormatter::DEFAULT_BINARY);
			case 'd': return ParseNumberFormatterArguments(arg_str, TNumberFormatter::DEFAULT_DECIMAL);
			case 'x': return ParseNumberFormatterArguments(arg_str, TNumberFormatter::DEFAULT_HEXADECIMAL);
			case 'p': return ParseNumberFormatterArguments(arg_str, TNumberFormatter::DEFAULT_ADDRESS);
			case 's': return ParseStringFormatterArguments(arg_str);
			case 'q': return ParseQuotedFormatterArguments(arg_str);
			case 'c': return ParseCharacterFormatterArguments(arg_str);
			default: EL_THROW(TInvalidArgumentException, "format_char", "valid values are: b, d, x, p, s, q, c");
		}
	}

	TFormatVariable TFormatVariable::ParseNumberFormatterArguments(const TString& arg_str, const TNumberFormatter* const default_format)
	{
		// %[<PAD>][<int part len>[.<dec part len>]](b|d|x|p)
		// first non-numeric character is interpreted as pad character
		// following numbers are interpreted as minimum integer part length
		// if a '.' and another number follows, this is interpreted as exact decimal part length

		if(arg_str.Length() > 0)
		{
			// std::cerr<<"DEBUG: arg_str = '"<<arg_str.MakeCStr().get()<<"'\n";
			std::unique_ptr<TNumberFormatter> custom_formatter = std::unique_ptr<TNumberFormatter>(new TNumberFormatter(default_format->config));

			usys_t pos = 0;
			const bool has_padding_chr = !(arg_str[0].code >= '1' && arg_str[0].code <= '9');

			if(has_padding_chr)
			{
				// padding character
				custom_formatter->config.integer_pad_sign = arg_str[0];
				custom_formatter->config.decimal_pad_sign = arg_str[0];
				pos++;

				// std::cerr<<"DEBUG: has_padding_chr\n";
			}

			if(arg_str.Length() > pos)
			{
				const usys_t pos_dot = arg_str.Find('.', pos);

				// std::cerr<<"DEBUG: pos_dot = "<<pos_dot<<std::endl;

				if(pos_dot != NEG1)
				{
					// decimal part length
					custom_formatter->config.n_decimal_places = arg_str.SliceSL(pos_dot + 1, arg_str.Length() - pos_dot - 1).ToInteger();

					// integer part length
					custom_formatter->config.n_min_integer_places = arg_str.SliceBE(pos, pos_dot).ToInteger();
				}
				else
				{
					// integer part length
					custom_formatter->config.n_min_integer_places = arg_str.SliceSL(pos, arg_str.Length() - pos).ToInteger();
				}
			}

			// std::cerr<<"DEBUG: n_min_integer_places = "<<custom_formatter->config.n_min_integer_places<<std::endl;
			// std::cerr<<"DEBUG: n_decimal_places = "<<custom_formatter->config.n_decimal_places<<std::endl;

			return TFormatVariable(false, custom_formatter.release());
		}
		else
		{
			return TFormatVariable(true, default_format);
		}
	}

	TFormatVariable TFormatVariable::ParseStringFormatterArguments(const TString& arg_str)
	{
		EL_ERROR(arg_str.Length() != 0, TNotImplementedException);

		return TFormatVariable(true, &TStringFormatter::PLAIN);
	}

	TFormatVariable TFormatVariable::ParseQuotedFormatterArguments(const TString& arg_str)
	{
		EL_ERROR(arg_str.Length() != 0, TNotImplementedException);

		return TFormatVariable(true, &TStringFormatter::ASCII_QUOTED);
	}

	TFormatVariable TFormatVariable::ParseCharacterFormatterArguments(const TString& arg_str)
	{
		EL_ERROR(arg_str.Length() != 0, TNotImplementedException);

		return TFormatVariable(true, &TStringFormatter::PLAIN);
	}

	/************************************************************/

	double TString::ToDouble() const
	{
		EL_ERROR(this->Length() == 0, TInvalidArgumentException, "str", "empty string cannot be parsed as double");

		double number = 0.0;
		double divider = 10.0;

		u8_t integer_part[22];
		bool parse_int = true;
		bool negative = false;
		unsigned ii = 0;

		for(const TUTF32& chr : chars)
		{
			if(chr.code == '-' && parse_int && ii == 0 && !negative)
			{
				negative = true;
			}
			else if(chr.code == '.' && parse_int)
			{
				parse_int = false;
			}
			else if(chr.code >= '0' && chr.code <= '9')
			{
				if(parse_int)
				{
					EL_ERROR(ii >= sizeof(integer_part), TException, "number integer-part is too big");
					integer_part[ii++] = ((u8_t)(chr.code - '0'));
				}
				else
				{
					number += (chr.code - '0') / divider;
					divider *= 10.0;
				}
			}
			else
			{
				EL_THROW(TException, TString::Format("encountered non-numeric character '%c' at index %d", chr, (usys_t)(&chr - &chars[0])));
			}
		}

		u64_t m = 1;
		for(usys_t i = ii; i > 0; i--)
		{
			number += m * integer_part[i-1];
			m *= 10;
		}

		if(negative)
			number *= -1.0;

		return number;
	}

	s64_t TString::ToInteger() const
	{
		EL_ERROR(this->Length() == 0, TInvalidArgumentException, "str", "empty string cannot be parsed as integer");

		u8_t integer_part[22];
		bool negative = false;
		unsigned ii = 0;

		for(const TUTF32& chr : chars)
		{
			if(chr.code == '-' && ii == 0 && !negative)
			{
				negative = true;
			}
			else if(chr.code >= '0' && chr.code <= '9') // TODO: support more than just ASCII decimal - there are other unicode digits and number-systems
			{
				EL_ERROR(ii >= sizeof(integer_part), TException, "number integer-part is too big");
				integer_part[ii++] = ((u8_t)(chr.code - '0'));
			}
			else
			{
				EL_THROW(TException, TString::Format("encountered non-numeric character '%c' at index %d", chr, (usys_t)(&chr - &chars[0])));
			}
		}

		s64_t number = 0;
		u64_t m = 1;
		for(usys_t i = ii; i > 0; i--)
		{
			number += m * integer_part[i-1];
			m *= 10;
		}

		if(negative)
			number *= -1;

		// std::cerr<<"DEBUG: ToInteger() = "<<number<<std::endl;
		return number;
	}

	TString::TString(const char* const str, const usys_t maxlen)
	{
		const size_t len = str != nullptr ? (maxlen == NEG1 ? strlen(str) : strnlen(str, maxlen)) : 0;
		array_t<const byte_t> array((const byte_t*)str, len);
		chars = array.Pipe().Transform(TCharDecoder()).Collect();
	}

	#ifdef EL_WCHAR_IS_UTF32
		TString::TString(const wchar_t* const str, const usys_t maxlen)
		{
			const size_t len = maxlen == NEG1 ? wcslen(str) : wcsnlen(str, maxlen);
			chars.Append((const TUTF32*)str, len);
		}
	#endif

	TString::TString(const TUTF32* const str, const usys_t maxlen)
	{
		const usys_t len = TUTF32::StringLength(str, maxlen);
		chars.Append(str, len);
	}

	TString& TString::operator+=(const TString& rhs)
	{
		chars.Append(rhs.chars);
		return *this;
	}

	TString  TString::operator+ (const TString& rhs) const
	{
		TString tmp = *this;
		tmp += rhs;
		return tmp;
	}

	TString& TString::operator+=(const TUTF32 rhs)
	{
		chars.Append(rhs);
		return *this;
	}

	TString  TString::operator+ (const TUTF32 rhs) const
	{
		TString tmp = *this;
		tmp += rhs;
		return tmp;
	}

	bool TString::BeginsWith(const TString& str) const
	{
		if(this->Length() < str.Length())
			return false;

		for(usys_t i = 0; i < str.Length(); i++)
			if(str[i] != this->chars[i])
				return false;

		return true;
	}

	void TString::Insert(const ssys_t pos, const TString& str)
	{
		chars.Insert(pos, str.chars);
	}

	void TString::Append(const TString& str)
	{
		chars.Append(str.chars);
	}

	bool TString::EndsWith(const TString& txt) const
	{
		const usys_t my_len = this->Length();
		const usys_t txt_len = txt.Length();
		const usys_t l = util::Min(my_len, txt_len);
		for(usys_t i = 1; i <= l; i++)
			if(this->chars[my_len - i] != txt.chars[txt_len - i])
				return false;
		return true;
	}

	TString TString::ExtractSequence(const array_t<const TUTF32> charset, const ssys_t _start, usys_t max_length) const
	{
		TString seq;
		const usys_t start = chars.AbsoluteIndex(_start, false);
		const usys_t end = max_length == NEG1 ? chars.Count() : util::Min(chars.Count(), start + max_length);

		for(usys_t i = start; i < end; i++)
		{
			const TUTF32 chr = chars[i];
			if(!charset.Contains(chr))
				break;
			seq += chr;
		}

		return seq;
	}

	bool TString::operator==(const TString& rhs) const
	{
		if(this == &rhs) return true;
		if(this->chars.Count() != rhs.chars.Count())
			return false;

		for(usys_t i = 0; i < this->chars.Count(); i++)
			if(this->chars[i] != rhs.chars[i])
				return false;

		return true;
	}

	bool TString::operator!=(const TString& rhs) const
	{
		return !operator==(rhs);
	}

	bool TString::operator>=(const TString& rhs) const
	{
		if(this == &rhs) return true;
		const usys_t n = util::Min(this->chars.Count(), rhs.chars.Count());

		for(usys_t i = 0; i < n; i++)
		{
			if(this->chars[i] > rhs.chars[i])
				return true;

			if(this->chars[i] < rhs.chars[i])
				return false;
		}

		if(this->chars.Count() >= rhs.chars.Count())
			return true;

		return false;
	}

	bool TString::operator<=(const TString& rhs) const
	{
		if(this == &rhs) return true;
		const usys_t n = util::Min(this->chars.Count(), rhs.chars.Count());

		for(usys_t i = 0; i < n; i++)
		{
			if(this->chars[i] < rhs.chars[i])
				return true;

			if(this->chars[i] > rhs.chars[i])
				return false;
		}

		if(this->chars.Count() <= rhs.chars.Count())
			return true;

		return false;
	}

	bool TString::operator> (const TString& rhs) const
	{
		if(this == &rhs) return false;
		const usys_t n = util::Min(this->chars.Count(), rhs.chars.Count());

		for(usys_t i = 0; i < n; i++)
		{
			if(this->chars[i] > rhs.chars[i])
				return true;

			if(this->chars[i] < rhs.chars[i])
				return false;
		}

		if(this->chars.Count() > rhs.chars.Count())
			return true;

		return false;
	}

	bool TString::operator< (const TString& rhs) const
	{
		if(this == &rhs) return false;
		const usys_t n = util::Min(this->chars.Count(), rhs.chars.Count());

		for(usys_t i = 0; i < n; i++)
		{
			if(this->chars[i] > rhs.chars[i])
				return false;

			if(this->chars[i] < rhs.chars[i])
				return true;
		}

		if(this->chars.Count() < rhs.chars.Count())
			return true;

		return false;
	}

	usys_t TString::Find(const TString& needle, const ssys_t start, const bool reverse) const
	{
		EL_ERROR(needle.Length() == 0, TInvalidArgumentException, "needle", "needle must not be empty");

		if(this->Length() == 0)
			return NEG1;

		if(reverse)
		{
			for(ssys_t i = chars.AbsoluteIndex(start, false) - needle.Length() + 1; i >= 0; i--)
			{
				usys_t j = 0;
				for(; j < needle.Length() && chars[i + j] == needle[j]; j++);
				if(j == needle.Length())
					return i;
			}
		}
		else
		{
			const usys_t end = chars.Count() - needle.Length() + 1;
			for(usys_t i = chars.AbsoluteIndex(start, false); i < end; i++)
			{
				usys_t j = 0;
				for(; j < needle.Length() && chars[i + j] == needle[j]; j++);
				if(j == needle.Length())
					return i;
			}
		}

		return NEG1;
	}

	usys_t TString::Find(const TUTF32 needle, const ssys_t start, const bool reverse) const
	{
		if(Length() == 0)
			return NEG1;

		if(reverse)
		{
			for(ssys_t i = chars.AbsoluteIndex(start, false); i >= 0; i--)
				if(chars[i] == needle)
					return i;
		}
		else
		{
			for(usys_t i = chars.AbsoluteIndex(start, false); i < chars.Count(); i++)
				if(chars[i] == needle)
					return i;
		}

		return NEG1;
	}

	usys_t TString::FindFirst(const array_t<const TUTF32>& charset, const ssys_t _start, const bool reverse) const
	{
		const usys_t start = chars.AbsoluteIndex(_start, false);
		if(reverse)
		{
			for(ssys_t i = start; i >= 0; i--)
				if(charset.Contains(chars[i]))
					return i;
		}
		else
		{
			for(usys_t i = start; i < chars.Count(); i++)
				if(charset.Contains(chars[i]))
					return i;
		}
		return NEG1;
	}

	TString& TString::Trim(const bool start, const bool end, const array_t<const TUTF32> trim_chars)
	{
		usys_t s = 0;
		usys_t e = 0;

		if(start)
			for(; s < Length() && trim_chars.Contains(chars[s]); s++);

		if(end && s < chars.Count())
			for(; e < Length() && trim_chars.Contains(chars[Length() - e - 1]); e++);

		chars.Cut(s, e);
		return *this;
	}

	void TString::ReplaceAt(const ssys_t pos, const usys_t length, const TString& substitute)
	{
		const usys_t n_common = util::Min(length, substitute.Length());

		for(usys_t i = 0; i < n_common; i++)
			chars[pos + i] = substitute[i];

		if(length > substitute.Length())
			chars.Remove(pos + n_common, length - n_common);
		else if(length < substitute.Length())
			chars.Insert(pos + n_common, &substitute.chars[n_common], substitute.Length() - n_common);
	}

	usys_t TString::Replace(const TString& needle, const TString& substitute, const ssys_t start, const bool reverse, const usys_t n_max_replacements)
	{
		EL_ERROR(needle.Length() == 0, TInvalidArgumentException, "needle", "needle must not be empty");

		if(Length() == 0)
			return 0;

		usys_t n_replace = 0;
		usys_t pos_current = chars.AbsoluteIndex(start, false);

		const ssys_t displacement = ((ssys_t)substitute.Length() - (ssys_t)needle.Length()) * (reverse ? -1 : 1);

		while(n_replace < n_max_replacements)
		{
			const usys_t pos_found = Find(needle, pos_current, reverse);
			if(pos_found == NEG1)
				break;

			ReplaceAt(pos_found, needle.Length(), substitute);
			pos_current = pos_found + displacement;
			n_replace++;
		}

		return n_replace;
	}

	bool TString::Contains(const TString& needle) const
	{
		if(this->Length() < needle.Length())
			return false;

		for(usys_t i = 0; i <= this->chars.Count() - needle.chars.Count(); i++)
			if(memcmp(this->chars.ItemPtr(i), needle.chars.ItemPtr(0), needle.chars.Count() * sizeof(TUTF32)) == 0)
				return true;

		return false;
	}

	bool TString::Contains(const TUTF32 needle) const
	{
		return this->chars.Contains(needle);
	}

	TString TString::Join(array_t<const TString> list, const TString& delimiter)
	{
		return list.Pipe().Aggregate([&delimiter](TString& result, const TString& append){
			if(result.Length() > 0)
				result.Append(delimiter);
			result.Append(append);
		}, TString());
	}

	TList<TString> TString::Split(const TString& delimiter, const usys_t n_max, const bool skip_empty) const
	{
		usys_t start = 0;
		TList<TString> list;

		while(start < this->Length() && list.Count() + 1 < n_max)
		{
			const usys_t pos = this->Find(delimiter, start, false);
			if(pos == NEG1)
			{
				break;
			}
			else
			{
				if(!(skip_empty && start == pos))
					list.Append(this->SliceBE(start, pos));
				start = pos + delimiter.Length();
			}
		}

		if(!(skip_empty && start == this->Length()))
			list.Append(this->SliceBE(start, this->Length()));

		return list;
	}

	TList<TString> TString::Split(const TUTF32 delimiter, const usys_t n_max, const bool skip_empty) const
	{
		usys_t start = 0;
		TList<TString> list;

		for(usys_t i = 0; i < this->Length() && list.Count() + 1 < n_max; i++)
		{
			if(this->chars[i] == delimiter)
			{
				if(!(skip_empty && start == i))
					list.Append(this->SliceBE(start, i));

				start = i + 1;
			}
		}

		if(!(skip_empty && start == this->Length()))
			list.Append(this->SliceBE(start, this->Length()));

		return list;
	}

	TList<TString> TString::Split(const array_t<const TUTF32> split_chars, const usys_t n_max, const bool skip_empty) const
	{
		usys_t start = 0;
		TList<TString> list;

		for(usys_t i = 0; i < this->Length() && list.Count() + 1 < n_max; i++)
		{
			if(split_chars.Contains(this->chars[i]))
			{
				if(!(skip_empty && start == i))
					list.Append(this->SliceBE(start, i));

				start = i + 1;
			}
		}

		if(!(skip_empty && start == this->Length()))
			list.Append(this->SliceBE(start, this->Length()));

		return list;
	}

	TList<TString> TString::BlockFormat(const unsigned n_line_len) const
	{
		TList<TString> lines;
		auto words = Split(WHITESPACE_CHARS, NEG1, true);
		TString current_line;
		for(auto word : words)
		{
			if(current_line.Length() + word.Length() <= n_line_len)
			{
				if(current_line.Length() > 0)
					current_line += " ";
				current_line += word;
				word.chars.Clear();
			}
			else
			{
				lines.MoveAppend(std::move(current_line));
				current_line = std::move(word);
			}
		}
		if(current_line.Length() > 0)
			lines.MoveAppend(std::move(current_line));
		return lines;
	}

	kv_pair_tt<TString,TString> TString::SplitKV(const TString& delimiter) const
	{
		const usys_t idx = Find(delimiter);
		EL_ERROR(idx == NEG1, TException, TString::Format("unable to find key/value delimiter %q", delimiter));
		return { SliceBE(0, idx), SliceBE(idx + delimiter.Length(), Length()) };
	}

	kv_pair_tt<TString,TString> TString::SplitKV(const TUTF32 delimiter) const
	{
		const usys_t idx = Find(delimiter);
		EL_ERROR(idx == NEG1, TException, TString::Format("unable to find key/value delimiter %q", delimiter));
		return { SliceBE(0, idx), SliceBE(idx + 1, Length()) };
	}

	TString TString::SliceSL(const ssys_t start, usys_t length) const
	{
		if(length == 0)
			return "";

		const usys_t idx_start = chars.AbsoluteIndex(start, true);
		if(length == NEG1)
			length = chars.Count() - start;

		EL_ERROR(idx_start >= chars.Count(), TInvalidArgumentException, "start", "start is after the end of the string");
		EL_ERROR(idx_start + length > chars.Count(), TInvalidArgumentException, "length", "start + length is after the end of the string");
		return TString(&chars[idx_start], util::Min(this->Length() - idx_start, length));
	}

	TString TString::SliceBE(const ssys_t begin, const ssys_t end) const
	{
		if(begin == end)
			return "";
		const usys_t start = chars.AbsoluteIndex(begin, false);
		const usys_t end_abs = chars.AbsoluteIndex(end, true);
		EL_ERROR(end_abs < start, TInvalidArgumentException, "end", "end is before start");
		return SliceSL(start, end_abs - start);
	}

	TString& TString::Pad(const TUTF32 pad_sign, const usys_t min_length, const EPlacement placement)
	{
		if(chars.Count() >= min_length || placement == EPlacement::NONE || pad_sign == TUTF32::TERMINATOR)
			return *this;

		const usys_t index = 	placement == EPlacement::START ? 0 :
								placement == EPlacement::END ? chars.Count() :
								chars.Count() / 2;

		chars.FillInsert(index, pad_sign, min_length - chars.Count());
		return *this;
	}

	TString TString::Padded(const TUTF32 pad_sign, const usys_t length)
	{
		TString str;
		str.Pad(pad_sign, length, EPlacement::END);
		return str;
	}

	TString& TString::Reverse()
	{
		chars.Reverse();
		return *this;
	}

	void TString::Escape(const array_t<const TUTF32> special_chars, const TUTF32 escape_sign)
	{
		for(usys_t i = 0; i < chars.Count(); i++)
			if(special_chars.Contains(chars[i]) || chars[i] == escape_sign)
			{
				chars.Insert(i, escape_sign);
				i++;
			}
	}

	void TString::Unescape(const array_t<const TUTF32> special_chars, const TUTF32 escape_char)
	{
		for(usys_t i = 0; i < chars.Count(); i++)
		{
			if(chars[i] == escape_char)
			{
				EL_ERROR(i + 1 == chars.Count(), TException, TString::Format("found escape character at last position of input string %q", *this));
				chars.Remove(i, 1);
			}
			else
			{
				EL_ERROR(special_chars.Contains(chars[i]), TException, TString::Format("found unescaped special character %c at position %d in input string %q", chars[i], i, *this));
			}
		}
	}

	void TString::Quote(const TUTF32 quote_sign, const TUTF32 escape_sign)
	{
		Escape(array_t<const TUTF32>(&quote_sign, 1), escape_sign);
		chars.Insert(0, quote_sign);
		chars.Append(quote_sign);
	}

	void TString::Unquote(const TUTF32 quote_sign, const TUTF32 escape_sign)
	{
		EL_ERROR(chars[0] != quote_sign || chars[-1] != quote_sign, TInvalidArgumentException, "this", "the input string does not start and end with a quoute character");
		chars.Cut(1, 1);
		Unescape(array_t<const TUTF32>(&quote_sign, 1), escape_sign);
	}

	void TString::Truncate(const usys_t n_max_length)
	{
		if(chars.Count() > n_max_length)
		{
			const usys_t n_remove = chars.Count() - n_max_length;
			chars.Remove(-n_remove, n_remove);
		}
	}

	void TString::Cut(const usys_t n_begin, const usys_t n_end)
	{
		chars.Cut(n_begin, n_end);
	}

	void TString::Translate(const array_t<const symbol_map_t> map, const bool reverse)
	{
		chars.Apply([&](TUTF32& current_char) {
			for(usys_t i = 0; i < map.Count(); i++)
				if(map[i].arr[reverse ? 1 : 0] == current_char)
				{
					current_char = map[i].arr[reverse ? 0 : 1];
					break;
				}
		});
	}

	TString& TString::ToLower()
	{
		Translate(MAP_LETTER_CASE, true);
		return *this;
	}

	TString& TString::ToUpper()
	{
		Translate(MAP_LETTER_CASE, false);
		return *this;
	}

	TString TString::Lower() const
	{
		TString str = *this;
		str.ToLower();
		return str;
	}

	TString TString::Upper() const
	{
		TString str = *this;
		str.ToUpper();
		return str;
	}

	std::unique_ptr<char[]> TString::MakeCStr() const
	{
		const usys_t n_bytes = chars.Pipe().Transform(TCharEncoder()).Count();
		auto p = std::unique_ptr<char[]>(new char[n_bytes + 1]);
		chars.Pipe().Transform(TCharEncoder()).ReadAll((byte_t*)p.get(), n_bytes);
		p.get()[n_bytes] = 0;
		return p;
	}

	void TString::_Format(TString& out, const TString& format, usys_t& pos)
	{
		const TFormatVariable var = TFormatVariable::Find(format, pos);
		EL_ERROR(var.len != 0, TTooManyFormatVariablesException, format);
		TString slice = format.SliceBE(pos, format.Length());
		slice.Replace("%%", "%");
		out += slice;
		pos = format.Length();
	}
}

namespace std
{
	std::ostream& operator<<(std::ostream& os, const el1::io::text::string::TString& str)
	{
		os<<str.MakeCStr().get();
		return os;
	}
}
