#pragma once

#include "io_collection_list.hpp"
#include "io_text_encoding.hpp"
#include "io_types.hpp"
#include "error.hpp"
#include "def.hpp"
#include "math.hpp"
#include "io_text_format.hpp"

#include <cstddef>
#include <type_traits>

namespace el1::io::bcd
{
	class TBCD;
	template<io::types::usys_t N_DIGITS, io::types::u16_t N_INTEGER, io::types::u16_t N_DECIMAL, io::types::u8_t BASE> class TFixedBCD;
}

namespace el1::io::text::string
{
	class TString;
	using namespace io::types;
	using namespace io::text::encoding;
	using namespace io::collection::list;
	using namespace io::collection::array;

	/**
	 * Immutable, non-owning UTF-32 string view backed directly by char32_t.
	 *
	 * TStringView never allocates and never owns character storage. Native
	 * `U"..."` literals bind directly through the array constructor below; the
	 * terminating U'\0' is intentionally not part of the view.
	 */
	class EL_LIFETIME_POINTER TStringView : public array_t<const char32_t>
	{
		public:
			using TBase = array_t<const char32_t>;

			constexpr TStringView() noexcept = default;
			constexpr TStringView(const TBase chars EL_LIFETIME_BOUND) noexcept : TBase(chars) {}
			TStringView(const TString& string EL_LIFETIME_BOUND) noexcept;

			// Explicit raw-pointer escape hatch; prefer U"...", TString or array_t views.
			static constexpr TStringView FromUnsafePointer(const char32_t* const chars EL_LIFETIME_BOUND, const usys_t n_chars) noexcept
			{
				return TStringView(TBase::FromUnsafePointer(chars, n_chars));
			}

			template<std::size_t N>
			constexpr TStringView(const char32_t (&chars EL_LIFETIME_BOUND)[N]) noexcept : TBase(chars, N > 0 ? N - 1 : 0) {}

			constexpr usys_t Length() const noexcept EL_GETTER { return Count(); }

			bool Contains(const TStringView needle) const EL_GETTER;
			bool Contains(const char32_t needle) const EL_GETTER { return TBase::Contains(needle); }
			usys_t Find(const TStringView needle, const ssys_t start = 0, const bool reverse = false) const EL_GETTER;
			usys_t Find(const char32_t needle, const ssys_t start = 0, const bool reverse = false) const EL_GETTER;
			usys_t FindFirst(const array_t<const char32_t>& charset, const ssys_t start = 0, const bool reverse = false) const EL_GETTER;
			bool BeginsWith(const TStringView txt) const EL_GETTER;
			bool EndsWith(const TStringView txt) const EL_GETTER;

			TStringView SliceSL(const ssys_t start, usys_t length = NEG1) const EL_LIFETIME_BOUND;
			TStringView SliceBE(const ssys_t begin, const ssys_t end) const EL_LIFETIME_BOUND;

			double ToDouble() const EL_GETTER;
			s64_t ToInteger() const EL_GETTER;

			bool operator==(const TStringView rhs) const EL_GETTER;
			bool operator!=(const TStringView rhs) const EL_GETTER { return !operator==(rhs); }
			bool operator>=(const TStringView rhs) const EL_GETTER;
			bool operator<=(const TStringView rhs) const EL_GETTER;
			bool operator> (const TStringView rhs) const EL_GETTER;
			bool operator< (const TStringView rhs) const EL_GETTER;
	};

	enum class EPlacement : u8_t
	{
		NONE,
		START,
		MID,
		END,
	};

	struct symbol_map_t
	{
		char32_t arr[2];
	};

	inline constexpr TStringView OCTAL_SYMBOLS = U"01234567";
	inline constexpr TStringView DECIMAL_SYMBOLS = U"0123456789";
	inline constexpr TStringView HEXADECIMAL_SYMBOLS_UC = U"0123456789ABCDEF";
	inline constexpr TStringView HEXADECIMAL_SYMBOLS_LC = U"0123456789abcdef";
	inline constexpr TStringView BINARY_SYMBOLS = U"01";
	inline constexpr TStringView ASCII_QUOTE_SYMBOLS = U"'\"";
	inline constexpr TStringView CONTROL_CHARS = U"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F";
	inline constexpr TStringView WHITESPACE_CHARS = U"\x09\x0A\x0B\x0C\x0D\x20\x85\xA0\x1680\x2000\x2001\x2002\x2003\x2004\x2005\x2006\x2007\x2008\x2009\x200A\x2028\x2029\x202F\x205F\x3000";
	array_t<const symbol_map_t> LetterCaseMap();	// [0] = lower; [1] = upper

	class EL_LIFETIME_OWNER TString
	{
		public:
			TList<char32_t> chars;

			template<typename ... A>
			static TString Format(const format::TFormatString<std::type_identity_t<std::decay_t<const A>>...>& format, A const& ...args);

			static TString Join(array_t<const TString> list, const TStringView delimiter);
			static TString Join(array_t<const TString> list, const TString& delimiter) { return Join(list, delimiter.View()); }

			bool Contains(const TStringView needle) const;
			bool Contains(const TString& needle) const { return Contains(needle.View()); }
			bool Contains(const char32_t needle) const;
			usys_t Find(const TStringView needle, const ssys_t start = 0, const bool reverse = false) const;
			usys_t Find(const TString& needle, const ssys_t start = 0, const bool reverse = false) const { return Find(needle.View(), start, reverse); }
			usys_t Find(const char32_t needle, const ssys_t start = 0, const bool reverse = false) const;
			usys_t FindFirst(const array_t<const char32_t>& charset, const ssys_t start = 0, const bool reverse = false) const;
			TString& Trim(const bool start = true, const bool end = true, const array_t<const char32_t> trim_chars = WHITESPACE_CHARS);
			void ReplaceAt(const ssys_t pos, const usys_t length, const TStringView substitute);
			void ReplaceAt(const ssys_t pos, const usys_t length, const TString& substitute) { ReplaceAt(pos, length, substitute.View()); }
			usys_t Replace(const TStringView needle, const TStringView substitute, const ssys_t start = 0, const bool reverse = false, const usys_t n_max_replacements = NEG1);
			usys_t Replace(const TString& needle, const TString& substitute, const ssys_t start = 0, const bool reverse = false, const usys_t n_max_replacements = NEG1) { return Replace(needle.View(), substitute.View(), start, reverse, n_max_replacements); }
			void Insert(const ssys_t pos, const TStringView str);
			void Insert(const ssys_t pos, const TString& str) { Insert(pos, str.View()); }
			void Append(const TStringView str);
			void Append(const TString& str) { Append(str.View()); }
			bool BeginsWith(const TStringView txt) const EL_GETTER;
			bool BeginsWith(const TString& txt) const EL_GETTER { return BeginsWith(txt.View()); }
			bool EndsWith(const TStringView txt) const EL_GETTER;
			bool EndsWith(const TString& txt) const EL_GETTER { return EndsWith(txt.View()); }

			TString SliceSL(const ssys_t start, usys_t length = NEG1) const;
			TString SliceBE(const ssys_t begin, const ssys_t end) const;

			TList<TString> Split(const TStringView delimiter, const usys_t n_max = NEG1, const bool skip_empty = false) const EL_GETTER;
			TList<TString> Split(const TString& delimiter, const usys_t n_max = NEG1, const bool skip_empty = false) const EL_GETTER { return Split(delimiter.View(), n_max, skip_empty); }
			TList<TString> Split(const char32_t delimiter, const usys_t n_max = NEG1, const bool skip_empty = false) const EL_GETTER;
			TList<TString> Split(const array_t<const char32_t> split_chars, const usys_t n_max = NEG1, const bool skip_empty = false) const EL_GETTER;
			kv_pair_tt<TString,TString> SplitKV(const TStringView delimiter) const;
			kv_pair_tt<TString,TString> SplitKV(const TString& delimiter) const { return SplitKV(delimiter.View()); }
			kv_pair_tt<TString,TString> SplitKV(const char32_t delimiter = '=') const;

			TList<TString> BlockFormat(const unsigned n_line_len) const EL_GETTER;

			TString& Pad(const char32_t pad_sign, const usys_t min_length, const EPlacement placement);
			TString& Reverse();
			void Escape(const array_t<const char32_t> special_chars, const char32_t escape_sign);
			void Unescape(const array_t<const char32_t> special_chars, const char32_t escape_char);
			void Quote(const char32_t quote_sign, const char32_t escape_sign);
			void Unquote(const char32_t quote_sign, const char32_t escape_sign);
			void Truncate(const usys_t n_max_length);
			void Cut(const usys_t n_begin, const usys_t n_end);
			void Translate(const array_t<const symbol_map_t> map, const bool reverse = false);

			/**
			* Replaces characters according to a whitelist or blacklist.
			*
			* @param list Characters used as the whitelist or blacklist.
			* @param replacement Character used to replace matching input characters.
			* @param whitelist If `false`, replace characters found in `list`. If `true`, replace characters not found in `list`.
			* @return Number of replaced characters.
			*/
			usys_t ReplaceChars(array_t<const char32_t> list, const char32_t replacement, const bool whitelist = false);

			TString& ToLower();
			TString& ToUpper();

			TString Lower() const EL_GETTER;
			TString Upper() const EL_GETTER;

			TString ExtractSequence(const array_t<const char32_t> charset, const ssys_t start = 0, usys_t max_length = NEG1) const EL_GETTER;

			static TString Padded(const char32_t pad_sign, const usys_t length);

			double ToDouble() const EL_GETTER;
			s64_t ToInteger() const EL_GETTER;

			TString& operator+=(const TStringView rhs);
			TString& operator+=(const TString& rhs) { return operator+=(rhs.View()); }
			TString  operator+ (const TStringView rhs) const;
			TString operator+ (const TString& rhs) const { return operator+(rhs.View()); }

			TString& operator+=(const char32_t rhs);
			TString  operator+ (const char32_t rhs) const;

			bool operator==(const TStringView rhs) const EL_GETTER;
			bool operator==(const TString& rhs) const EL_GETTER { return operator==(rhs.View()); }
			bool operator!=(const TStringView rhs) const EL_GETTER;
			bool operator!=(const TString& rhs) const EL_GETTER { return operator!=(rhs.View()); }
			bool operator>=(const TStringView rhs) const EL_GETTER;
			bool operator>=(const TString& rhs) const EL_GETTER { return operator>=(rhs.View()); }
			bool operator<=(const TStringView rhs) const EL_GETTER;
			bool operator<=(const TString& rhs) const EL_GETTER { return operator<=(rhs.View()); }
			bool operator> (const TStringView rhs) const EL_GETTER;
			bool operator> (const TString& rhs) const EL_GETTER { return operator>(rhs.View()); }
			bool operator< (const TStringView rhs) const EL_GETTER;
			bool operator< (const TString& rhs) const EL_GETTER { return operator<(rhs.View()); }

			inline usys_t Length() const noexcept EL_GETTER { return chars.Count(); }
			TStringView View() const & noexcept EL_LIFETIME_BOUND { return TStringView(chars.View()); }
			TStringView View() const && = delete;
			inline char32_t operator[](const ssys_t index) const EL_GETTER { return chars[index]; }
			inline char32_t& operator[](const ssys_t index) EL_GETTER { return chars[index]; }

			std::unique_ptr<char[]> MakeCStr() const;

			TString() = default;
			TString(TString&&) = default;
			TString(const TString&) = default;
			TString(const char* const str, const usys_t maxlen = NEG1);
			TString(const wchar_t* const str, const usys_t maxlen = NEG1);
			explicit TString(const char32_t* const str, const usys_t maxlen = NEG1);
			TString(TList<char32_t> chars) : chars(chars) {}
			TString(array_t<const char32_t> chars) : chars(chars) {}
			TString(const TStringView chars) : chars(static_cast<const array_t<const char32_t>&>(chars)) {}

			TString& operator=(const TString&) = default;
			TString& operator=(TString&&) = default;
	};

	inline TStringView::TStringView(const TString& string EL_LIFETIME_BOUND) noexcept : TBase(string.chars.View()) {}

	class TLineReader
	{
		public:
			TString buffer;
			const usys_t n_max_length;

			using TIn = char32_t;
			using TOut = TString;

			template<typename TSourceStream>
			TString* NextItem(TSourceStream* const source)
			{
				buffer.chars.Clear(NEG1);

				const char32_t* chr;
				while((chr = source->NextItem()) != nullptr && buffer.Length() < n_max_length)
				{
					if(*chr == 10U) // LF
					{
						if(buffer.Length() > 0 && buffer[-1] == 13U) // CR
							buffer.Cut(0,1); // remove CR
						return &buffer;
					}
					else
					{
						buffer += *chr;
					}
				}

				EL_ERROR(chr != nullptr && buffer.Length() >= n_max_length, TException, TString::Format(U"maximum line length of %d characters exceeded", n_max_length));

				if(buffer.Length() == 0)
					return nullptr;
				else
					return &buffer;
			}

			TLineReader(const usys_t n_max_length = NEG1) : n_max_length(n_max_length)
			{
				this->buffer.chars.Clear(n_max_length);
			}
	};

	template<typename T>
	struct strigify_t
	{
		template<typename U>
		static auto ToString(const U& v, const TString&) -> decltype((io::text::string::TString)v)
		{
			return (io::text::string::TString)v;
		}

		template<typename U>
		static const io::text::string::TString& ToString(U&&, const TString& alt)
		{
			return alt;
		}
	};

	struct TUndefineFormatException : error::IException
	{
		const char* const argument_type;
		const char* const format_name;

		TString Message() const final override;
		error::IException* Clone() const override;

		TUndefineFormatException(const char* const argument_type, const char* const format_name);
	};

	struct TMissingFormatVariableException : error::IException
	{
		const TString format;

		TString Message() const final override;
		error::IException* Clone() const override;

		TMissingFormatVariableException(const TString format) : format(format) {}
	};

	struct TTooManyFormatVariablesException : error::IException
	{
		const TString format;

		TString Message() const final override;
		error::IException* Clone() const override;

		TTooManyFormatVariablesException(const TString format) : format(format) {}
	};

	struct IFormatter
	{
		virtual const char* FormatName() const = 0;

		virtual TString Format(const char* const value) const;
		virtual TString Format(const wchar_t* const value) const;
		virtual TString Format(const TString& value) const;
		virtual TString Format(const TStringView value) const;
		virtual TString Format(const char value) const;
		virtual TString Format(const wchar_t value) const;
		virtual TString Format(const char32_t value) const;
		virtual TString Format(const s8_t value) const;
		virtual TString Format(const u8_t value) const;
		virtual TString Format(const s16_t value) const;
		virtual TString Format(const u16_t value) const;
		virtual TString Format(const s32_t value) const;
		virtual TString Format(const u32_t value) const;
		virtual TString Format(const s64_t value) const;
		virtual TString Format(const u64_t value) const;
		virtual TString Format(const double value) const;
		virtual TString Format(const bcd::TBCD& value) const;

		template<usys_t N_DIGITS, u16_t N_INTEGER, u16_t N_DECIMAL, u8_t BASE>
		TString Format(const bcd::TFixedBCD<N_DIGITS, N_INTEGER, N_DECIMAL, BASE>& value) const
		{
			return Format(value.ToBCD());
		}
		virtual TString Format(const void* const p_data, const usys_t n_bits) const;

		virtual ~IFormatter();
	};

	struct TStringFormatter : IFormatter
	{
		struct config_t
		{
			TString prefix;
			TString suffix;
			unsigned n_min_length;
			unsigned n_max_length;
			char32_t pad_sign;
			EPlacement align;
			const array_t<const char32_t>* quote_symbols;
			char32_t escape_symbol;
		};

		config_t config;
		TStringFormatter(config_t config) : config(std::move(config)) {}

		const char* FormatName() const final override;
		TString Format(const char* const value) const final override;
		TString Format(const wchar_t* const value) const final override;
		TString Format(const TString& value) const final override;
		TString Format(const TStringView value) const final override;
		TString Format(const char value) const final override;
		TString Format(const wchar_t value) const final override;
		TString Format(const char32_t value) const final override;

		char32_t DetectBestQuoteSymbol(const TString& str) const;

		static const TStringFormatter PLAIN;
		static const TStringFormatter ASCII_QUOTED;
	};

	struct TNumberFormatter : IFormatter
	{
		struct config_t
		{
			const array_t<const char32_t>* symbols;
			TString prefix;
			TString suffix;
			char32_t decimal_point_sign;
			char32_t grouping_sign;
			char32_t integer_pad_sign;
			char32_t decimal_pad_sign;
			char32_t negative_sign;
			char32_t positive_sign;
			unsigned n_digits_per_group;
			unsigned n_decimal_places;		// -1U => all significant decimal digits
			unsigned n_min_integer_places;
			math::ERoundingMode rounding;
		};

		config_t config;
		TNumberFormatter(config_t config) : config(std::move(config)) {}

		const char* FormatName() const final override;
		TString Format(const s8_t value) const final override;
		TString Format(const u8_t value) const final override;
		TString Format(const s16_t value) const final override;
		TString Format(const u16_t value) const final override;
		TString Format(const s32_t value) const final override;
		TString Format(const u32_t value) const final override;
		TString Format(const s64_t value) const final override;
		TString Format(const u64_t value) const final override;
		TString Format(const bcd::TBCD& value) const final override;
		TString Format(const double value) const final override;

		TString MakeIntegerPart(u64_t value, const bool is_negative) const;
		TString MakeDecimalPart(double& value) const;

		static const TNumberFormatter* DEFAULT_OCTAL;
		static const TNumberFormatter* DEFAULT_DECIMAL;
		static const TNumberFormatter* DEFAULT_HEXADECIMAL;
		static const TNumberFormatter* DEFAULT_BINARY;
		static const TNumberFormatter* DEFAULT_ADDRESS;

		static const TNumberFormatter PLAIN_OCTAL;
		static const TNumberFormatter PLAIN_DECIMAL_US_EN;
		static const TNumberFormatter PLAIN_HEXADECIMAL_UPPER_US_EN;
		static const TNumberFormatter PLAIN_HEXADECIMAL_LOWER_US_EN;
 		static const TNumberFormatter PLAIN_BINARY_US_EN;
	};

	struct TRawDataFormatter : IFormatter
	{
		const array_t<const char32_t>* symbols;

		const char* FormatName() const final override;
		TString Format(const char* const value) const final override;
		TString Format(const wchar_t* const value) const final override;
		TString Format(const TString& value) const final override;
		TString Format(const char value) const final override;
		TString Format(const wchar_t value) const final override;
		TString Format(const char32_t value) const final override;
		TString Format(const s8_t value) const final override;
		TString Format(const u8_t value) const final override;
		TString Format(const s16_t value) const final override;
		TString Format(const u16_t value) const final override;
		TString Format(const s32_t value) const final override;
		TString Format(const u32_t value) const final override;
		TString Format(const s64_t value) const final override;
		TString Format(const u64_t value) const final override;
		TString Format(const double value) const final override;
		TString Format(const void* const p_data, const usys_t n_bits) const final override;
	};


	/********************************************/

	static inline bool MatchStringList(const TString& needle, const char* const haystack)
	{
		return needle == haystack;
	}

	template<typename ... A>
	static inline bool MatchStringList(const TString& needle, const char* const haystack, const A ... haystacks)
	{
		if(MatchStringList(needle, haystack))
			return true;

		return MatchStringList(needle, haystacks ...);
	}
}

namespace el1::io::text::string
{
	class TString;
	template<typename ... A>
	TString TString::Format(const format::TFormatString<std::type_identity_t<std::decay_t<const A>>...>& format, A const& ...args)
	{
		TString out;
		format.RenderInto(out, args...);
		return out;
	}
}

namespace std
{
	std::ostream& operator<<(std::ostream& os, const el1::io::text::string::TString& str);
}
