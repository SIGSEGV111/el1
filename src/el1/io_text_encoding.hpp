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

	struct TUTF32
	{
		u32_t code;

		constexpr ~TUTF32() = default;
		constexpr TUTF32() = default;
		constexpr TUTF32(TUTF32&&) = default;
		constexpr TUTF32(const TUTF32&) = default;
		constexpr TUTF32(const u32_t code) : code(code) {}

		#ifdef EL_CHAR_IS_UTF8
			// an individual UTF8 char can only be 7bit ASCII which translates 1:1 to UTF32
			constexpr TUTF32(const char chr) : code((u32_t)chr) {}
		#else
			TUTF32(const char chr);
		#endif

		#ifdef EL_WCHAR_IS_UTF32
			constexpr TUTF32(const wchar_t chr) : code((u32_t)chr) {}
		#else
			TUTF32(const wchar_t chr);
		#endif

		inline bool operator!=(const TUTF32& rhs) const EL_GETTER { return code != rhs.code; }
		inline bool operator==(const TUTF32& rhs) const EL_GETTER { return code == rhs.code; }
		inline bool operator>=(const TUTF32& rhs) const EL_GETTER { return code >= rhs.code; }
		inline bool operator<=(const TUTF32& rhs) const EL_GETTER { return code <= rhs.code; }
		inline bool operator> (const TUTF32& rhs) const EL_GETTER { return code >  rhs.code; }
		inline bool operator< (const TUTF32& rhs) const EL_GETTER { return code <  rhs.code; }

		TUTF32& operator=(const TUTF32&) = default;
		TUTF32& operator=(TUTF32&&) = default;

		bool IsAscii() const EL_GETTER;
		bool IsAsciiDecimal() const EL_GETTER;
		bool IsAsciiAlpha() const EL_GETTER;
		bool IsControl() const EL_GETTER;
		bool IsWhitespace() const EL_GETTER;

		static const TUTF32 TERMINATOR;
		static usys_t StringLength(const TUTF32* const str, const usys_t maxlen = NEG1);
	};

	#ifdef EL_WCHAR_IS_UTF32
		using TWideCharDecoder = io::stream::TReinterpretCastTransformer<TUTF32>;
		using TWideCharEncoder = io::stream::TReinterpretCastTransformer<wchar_t>;
	#endif
}

#ifdef EL_CHAR_IS_UTF8
	#include "io_text_encoding_utf8.hpp"
#endif
