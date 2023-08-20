#include "io_text_encoding.hpp"

namespace el1::io::text::encoding
{
	const TUTF32 TUTF32::TERMINATOR(0U);

	usys_t TUTF32::StringLength(const TUTF32* const str, const usys_t maxlen)
	{
		usys_t i = 0;
		for(; i < maxlen && str[i] != TUTF32::TERMINATOR; i++);
		return i;
	}

	bool TUTF32::IsAscii() const
	{
		return code <= 127;
	}

	bool TUTF32::IsAsciiDecimal() const
	{
		return code >= '0' && code <= '9';
	}

	bool TUTF32::IsAsciiAlpha() const
	{
		return (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z');
	}

	bool TUTF32::IsControl() const
	{
		return code <= 31;
	}
}
