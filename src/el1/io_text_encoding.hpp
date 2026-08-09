#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"

namespace el1::io::text::encoding
{
	using namespace io::types;

	enum class EDirection : u8_t
	{
		ENCODING,
		DECODING
	};

	constexpr usys_t UTF32StringLength(const char32_t* const str, const usys_t maxlen = NEG1) noexcept
	{
		usys_t i = 0;
		for(; i < maxlen && str[i] != U'\0'; i++);
		return i;
	}

	#ifdef EL_WCHAR_IS_UTF32
		using TWideCharDecoder = io::stream::TReinterpretCastTransformer<char32_t>;
		using TWideCharEncoder = io::stream::TReinterpretCastTransformer<wchar_t>;
	#endif
}

#ifdef EL_CHAR_IS_UTF8
	#include "io_text_encoding_utf8.hpp"
#endif
