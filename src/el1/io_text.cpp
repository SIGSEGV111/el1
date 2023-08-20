#include "io_text.hpp"
#include "io_text_string.hpp"

namespace el1::io::text
{
	using namespace io::collection::list;

	TFormatterMap IFormatter::FORMATTERS;

	const IFormatter& ITextWriter::LookupFormatter(const encoding::TUTF32 key)
	{
		const IFormatter** formatter = this->extra_formatters.Get(key);
		if(formatter != nullptr)
			return **formatter;
		else
			return *IFormatter::FORMATTERS[key];
	}

	/************************************************************************/

	ITextWriter& operator<<(ITextWriter& w, const u8_t v)
	{
		return w<<(u64_t)v;
	}

	ITextWriter& operator<<(ITextWriter& w, const s8_t v)
	{
		return w<<(s64_t)v;
	}

	ITextWriter& operator<<(ITextWriter& w, const u16_t v)
	{
		return w<<(u64_t)v;
	}

	ITextWriter& operator<<(ITextWriter& w, const s16_t v)
	{
		return w<<(s64_t)v;
	}

	ITextWriter& operator<<(ITextWriter& w, const u32_t v)
	{
		return w<<(u64_t)v;
	}

	ITextWriter& operator<<(ITextWriter& w, const s32_t v)
	{
		return w<<(s64_t)v;
	}

	ITextWriter& operator<<(ITextWriter& w, const float v)
	{
		return w<<(double)v;
	}

	ITextWriter& operator<<(ITextWriter& w, const u64_t v)
	{
		EL_NOT_IMPLEMENTED;
		return w;
	}

	ITextWriter& operator<<(ITextWriter& w, const s64_t v)
	{
		EL_NOT_IMPLEMENTED;
		return w;
	}

	ITextWriter& operator<<(ITextWriter& w, const double v)
	{
		EL_NOT_IMPLEMENTED;
		return w;
	}

	ITextWriter& operator<<(ITextWriter& w, const void* const v)
	{
		EL_NOT_IMPLEMENTED;
		return w;
	}

	ITextWriter& operator<<(ITextWriter& w, const char* const v)
	{
		EL_NOT_IMPLEMENTED;
		return w;
	}

	ITextWriter& operator<<(ITextWriter& w, const wchar_t* const v)
	{
		EL_NOT_IMPLEMENTED;
		return w;
	}

	ITextWriter& operator<<(ITextWriter& w, const array_t<const encoding::TUTF32> v)
	{
		EL_NOT_IMPLEMENTED;
		return w;
	}

	ITextWriter& operator<<(ITextWriter& w, const encoding::TUTF32 v)
	{
		EL_NOT_IMPLEMENTED;
		return w;
	}

	/************************************************************************/

	usys_t TGenericNumberFormatter::Size() const
	{
		return sizeof(TGenericNumberFormatter);
	}

	void TGenericNumberFormatter::ParseFormatParameters(collection::list::array_t<const encoding::TUTF32> spec)
	{
		EL_NOT_IMPLEMENTED;
	}

	/************************************************************************/

	usys_t TBasicStringFormatter::Size() const
	{
		return sizeof(TBasicStringFormatter);
	}

	void TBasicStringFormatter::ParseFormatParameters(array_t<const encoding::TUTF32> spec)
	{
		// the spec has the format
		// [[<padding sign (any non-ascii-numeric)>] <min length (ascii-numbers)> [<alignment (r|l|c)>]]

		if(spec.Count() > 0 && !spec[0].IsAsciiDecimal())
		{
			// padding sign
			parameters.pad_sign = spec[0];
			spec = array_t<const encoding::TUTF32>(spec.ItemPtr(1), spec.Count() - 1);
		}

		if(spec.Count() > 0)
		{
			usys_t l = 0;
			for(; l < spec.Count() && spec[l].IsAsciiDecimal(); l++);
			EL_ERROR(l == 0, TInvalidArgumentException, "spec", "spec is missing the numeric minimum-length field");
			EL_ERROR(l > 3, TInvalidArgumentException, "spec", "the numeric minimum-length field cannot exceed 3 digits (allowed numeric range 0-255)");
			const s64_t n_min_length = string::TString(spec.ItemPtr(0), l).ToInteger(); // TODO ugly
			EL_ERROR(n_min_length < 0 || n_min_length > 255, TInvalidArgumentException, "spec", "the minimum-length cannot exeed 255");
			parameters.n_min_length = (u8_t)n_min_length;
			spec = array_t<const encoding::TUTF32>(spec.ItemPtr(l), spec.Count() - l);
		}

		if(spec.Count() > 0)
		{
			EL_ERROR(spec.Count() > 1, TInvalidArgumentException, "spec", "after the minimum-length field only a single further character (l|r|c) is allowed that specified the alignment");

			switch(spec[0].code)
			{
				case 'l':
					parameters.alignment = EPlacement::START;
					break;

				case 'r':
					parameters.alignment = EPlacement::END;
					break;

				case 'c':
					parameters.alignment = EPlacement::MID;
					break;

				default:
					EL_THROW(TInvalidArgumentException, "spec", "after the minimum-length field only a single further character (l|r|c) is allowed that specified the alignment");
			}
		}
	}

	/************************************************************************/

	usys_t TQuotedStringFormatter::Size() const
	{
		return sizeof(TQuotedStringFormatter);
	}

	void TQuotedStringFormatter::ParseFormatParameters(array_t<const encoding::TUTF32> spec)
	{
		// up to 3 characters are allowed in the spec in this order:
		// <opening quotation mark> <closing quotation mark> <escape sign>

		EL_ERROR(spec.Count() > 3, TInvalidArgumentException, "spec", "invalid spec");

		if(spec.Count() == 1)
		{
			// just the quotation mark (used for both opening and closing)
			parameters.opening_quotation_sign = spec[0];
			parameters.closing_quotation_sign = spec[0];
		}
		else if(spec.Count() == 2)
		{
			// opening and closing mark
			parameters.opening_quotation_sign = spec[0];
			parameters.closing_quotation_sign = spec[1];
		}

		if(spec.Count() == 3)
		{
			parameters.opening_quotation_escape_sign = spec[2];
			parameters.closing_quotation_escape_sign = spec[2];
		}
	}
}
