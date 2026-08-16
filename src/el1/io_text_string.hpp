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

			static constexpr TStringView FromUnsafePointerStrlen(const char32_t* const chars EL_LIFETIME_BOUND) noexcept
			{
				usys_t n_chars = 0;
				if(chars != nullptr)
					for(; chars[n_chars] != 0; n_chars++);
				return TStringView::FromUnsafePointer(chars, n_chars);
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

			template<typename TLeft, typename TRight, typename ... A>
			requires std::is_same_v<std::tuple<std::type_identity_t<std::decay_t<const A>>...>, typename format::TConcatenatedFormatString<TLeft, TRight>::TArguments>
			static TString Format(const format::TConcatenatedFormatString<TLeft, TRight>& format, A const& ...args);

			template<typename F, typename ... A>
			requires std::is_same_v<std::remove_cvref_t<F>, TStringView>
			static TString Format(F&& format, A const& ...args);

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
			// bool operator==(const TString& rhs) const EL_GETTER { return operator==(rhs.View()); }
			bool operator!=(const TStringView rhs) const EL_GETTER;
			// bool operator!=(const TString& rhs) const EL_GETTER { return operator!=(rhs.View()); }
			bool operator>=(const TStringView rhs) const EL_GETTER;
			// bool operator>=(const TString& rhs) const EL_GETTER { return operator>=(rhs.View()); }
			bool operator<=(const TStringView rhs) const EL_GETTER;
			// bool operator<=(const TString& rhs) const EL_GETTER { return operator<=(rhs.View()); }
			bool operator> (const TStringView rhs) const EL_GETTER;
			// bool operator> (const TString& rhs) const EL_GETTER { return operator>(rhs.View()); }
			bool operator< (const TStringView rhs) const EL_GETTER;
			// bool operator< (const TString& rhs) const EL_GETTER { return operator<(rhs.View()); }

			inline usys_t Length() const noexcept EL_GETTER { return chars.Count(); }
			TStringView View() const & noexcept EL_LIFETIME_BOUND { return TStringView(chars.View()); }
			TStringView View() const && = delete;
			inline char32_t operator[](const ssys_t index) const EL_GETTER { return chars[index]; }
			inline char32_t& operator[](const ssys_t index) EL_GETTER { return chars[index]; }

			// inline operator TStringView() const & noexcept EL_LIFETIME_BOUND { return TStringView(chars.View()); }

			std::unique_ptr<char[]> MakeCStr() const;

			TString() = default;
			TString(TString&&) = default;
			TString(const TString&) = default;
			TString(const char* const str, const usys_t maxlen = NEG1);
			TString(const wchar_t* const str, const usys_t maxlen = NEG1);
			TString(const char32_t* const str, const usys_t maxlen = NEG1);
			TString(TList<char32_t> chars) : chars(std::move(chars)) {}
			TString(array_t<const char32_t> chars) : chars(chars) {}
			TString(const TStringView chars) : chars(static_cast<const array_t<const char32_t>&>(chars)) {}

			TString& operator=(const TString&) = default;
			TString& operator=(TString&&) = default;
	};

	inline TStringView::TStringView(const TString& string EL_LIFETIME_BOUND) noexcept : TBase(string.chars.View()) {}

	/** Owning buffered source for text that must outlive its original TString. */
	class TStringSource final : public stream::IBufferedSource<char32_t>
	{
		TString string;
		usys_t pos = 0;

	public:
		explicit TStringSource(TString string) : string(std::move(string)) {}
		explicit TStringSource(const TStringView string) : string(string) {}

		usys_t Count() const noexcept final override { return string.Length() - pos; }
		bool Ensure(const usys_t count) final override { return count <= Count(); }

		const char32_t& operator[](const usys_t index) const final override
		{
			EL_ERROR(index >= Count(), stream::TStreamDryException);
			return string.chars[pos + index];
		}

		array_t<const char32_t> Head() const noexcept EL_LIFETIME_BOUND final override
		{
			const usys_t count = Count();
			return array_t<const char32_t>::FromUnsafePointer(count == 0 ? nullptr : string.chars.ItemPtr(pos), count);
		}

		void Shift(const usys_t count) final override
		{
			EL_ERROR(count > Count(), stream::TStreamDryException);
			pos += count;
		}
	};

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



	/********************************************/

	static inline bool MatchStringList(const TString& needle, const TStringView haystack)
	{
		return needle == haystack;
	}

	template<typename ... A>
	static inline bool MatchStringList(const TString& needle, const TStringView haystack, const A& ... haystacks)
	{
		if(MatchStringList(needle, haystack))
			return true;

		return MatchStringList(needle, haystacks ...);
	}
}

namespace el1::io::text::string
{
	namespace detail
	{
		class TStringFormatSink final : public format::IFormatSink
		{
			TString& out;
		public:
			explicit TStringFormatSink(TString& out) : out(out) {}
			void Append(const char32_t* const data, const usys_t length) final
			{
				if(length != 0)
					out.chars.Append(array_t<const char32_t>::FromUnsafePointer(data, length));
			}
		};
	}

	class TString;
	template<typename ... A>
	TString TString::Format(const format::TFormatString<std::type_identity_t<std::decay_t<const A>>...>& format, A const& ...args)
	{
		TString out;
		detail::TStringFormatSink sink(out);
		format.RenderInto(sink, args...);
		return out;
	}

	template<typename TLeft, typename TRight, typename ... A>
	requires std::is_same_v<std::tuple<std::type_identity_t<std::decay_t<const A>>...>, typename format::TConcatenatedFormatString<TLeft, TRight>::TArguments>
	TString TString::Format(const format::TConcatenatedFormatString<TLeft, TRight>& format, A const& ...args)
	{
		TString out;
		detail::TStringFormatSink sink(out);
		format.RenderInto(sink, args...);
		return out;
	}

	template<typename F, typename ... A>
	requires std::is_same_v<std::remove_cvref_t<F>, TStringView>
	TString TString::Format(F&& runtime_format, A const& ...args)
	{
		const format::TFormatString<std::type_identity_t<std::decay_t<const A>>...> parsed_format(runtime_format.Data(), runtime_format.Length());
		EL_ERROR(!parsed_format.IsValid(), TException, U"invalid format string or formatter/argument mismatch");
		return Format(parsed_format, args...);
	}
}

namespace std
{
	std::ostream& operator<<(std::ostream& os, const el1::io::text::string::TString& str);
}
