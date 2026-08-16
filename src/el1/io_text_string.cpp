#include "io_text_string.hpp"
#include "io_text_encoding_utf8.hpp"
#include "io_bcd.hpp"
#include <string.h>
#include <iostream>
#include <math.h>

namespace el1::io::text::string
{
	bool TStringView::Contains(const TStringView needle) const
	{
		if(needle.Length() == 0)
			return true;
		if(Length() < needle.Length())
			return false;

		for(usys_t i = 0; i <= Count() - needle.Count(); i++)
			if(memcmp(ItemPtr(i), needle.ItemPtr(0), needle.Count() * sizeof(char32_t)) == 0)
				return true;

		return false;
	}

	usys_t TStringView::Find(const TStringView needle, const ssys_t start, const bool reverse) const
	{
		EL_ERROR(needle.Length() == 0, TInvalidArgumentException, "needle", "needle must not be empty");

		if(Length() == 0 || needle.Length() > Length())
			return NEG1;

		if(reverse)
		{
			for(ssys_t i = static_cast<ssys_t>(AbsoluteIndex(start, false)) - static_cast<ssys_t>(needle.Length()) + 1; i >= 0; i--)
			{
				usys_t j = 0;
				for(; j < needle.Length() && (*this)[i + j] == needle[j]; j++);
				if(j == needle.Length())
					return static_cast<usys_t>(i);
			}
		}
		else
		{
			const usys_t end = Count() - needle.Length() + 1;
			for(usys_t i = AbsoluteIndex(start, false); i < end; i++)
			{
				usys_t j = 0;
				for(; j < needle.Length() && (*this)[i + j] == needle[j]; j++);
				if(j == needle.Length())
					return i;
			}
		}

		return NEG1;
	}

	usys_t TStringView::Find(const char32_t needle, const ssys_t start, const bool reverse) const
	{
		if(Length() == 0)
			return NEG1;

		if(reverse)
		{
			for(ssys_t i = static_cast<ssys_t>(AbsoluteIndex(start, false)); i >= 0; i--)
				if((*this)[i] == needle)
					return static_cast<usys_t>(i);
		}
		else
		{
			for(usys_t i = AbsoluteIndex(start, false); i < Count(); i++)
				if((*this)[i] == needle)
					return i;
		}

		return NEG1;
	}

	usys_t TStringView::FindFirst(const array_t<const char32_t>& charset, const ssys_t start, const bool reverse) const
	{
		if(Length() == 0)
			return NEG1;

		const usys_t index = AbsoluteIndex(start, false);
		if(reverse)
		{
			for(ssys_t i = static_cast<ssys_t>(index); i >= 0; i--)
				if(charset.Contains((*this)[i]))
					return static_cast<usys_t>(i);
		}
		else
		{
			for(usys_t i = index; i < Count(); i++)
				if(charset.Contains((*this)[i]))
					return i;
		}

		return NEG1;
	}

	bool TStringView::BeginsWith(const TStringView txt) const
	{
		if(Length() < txt.Length())
			return false;

		for(usys_t i = 0; i < txt.Length(); i++)
			if((*this)[i] != txt[i])
				return false;
		return true;
	}

	bool TStringView::EndsWith(const TStringView txt) const
	{
		if(Length() < txt.Length())
			return false;

		const usys_t offset = Length() - txt.Length();
		for(usys_t i = 0; i < txt.Length(); i++)
			if((*this)[offset + i] != txt[i])
				return false;
		return true;
	}

	TStringView TStringView::SliceSL(const ssys_t start, usys_t length) const EL_LIFETIME_BOUND
	{
		const usys_t idx_start = AbsoluteIndex(start, true);
		if(length == NEG1)
			length = Count() - idx_start;

		EL_ERROR(length > Count() - idx_start, TInvalidArgumentException, "length", "start + length is after the end of the string");
		return TStringView::FromUnsafePointer(idx_start == 0 ? Data() : Data() + idx_start, length);
	}

	TStringView TStringView::SliceBE(const ssys_t begin, const ssys_t end) const EL_LIFETIME_BOUND
	{
		const usys_t idx_begin = AbsoluteIndex(begin, true);
		const usys_t idx_end = AbsoluteIndex(end, true);
		EL_ERROR(idx_end < idx_begin, TInvalidArgumentException, "end", "end is before start");
		return TStringView::FromUnsafePointer(idx_begin == 0 ? Data() : Data() + idx_begin, idx_end - idx_begin);
	}

	double TStringView::ToDouble() const
	{
		EL_ERROR(Length() == 0, TInvalidArgumentException, "str", "empty string cannot be parsed as double");

		double number = 0.0;
		double divider = 10.0;
		u8_t integer_part[22];
		bool parse_int = true;
		bool negative = false;
		unsigned ii = 0;

		for(usys_t i = 0; i < Length(); i++)
		{
			const char32_t chr = (*this)[i];
			if(chr == '-' && parse_int && ii == 0 && !negative)
			{
				negative = true;
			}
			else if(chr == '.' && parse_int)
			{
				parse_int = false;
			}
			else if(chr >= '0' && chr <= '9')
			{
				if(parse_int)
				{
					EL_ERROR(ii >= sizeof(integer_part), TException, U"number integer-part is too big");
					integer_part[ii++] = static_cast<u8_t>(chr - '0');
				}
				else
				{
					number += (chr - '0') / divider;
					divider *= 10.0;
				}
			}
			else
			{
				EL_THROW(TException, TString::Format(U"encountered non-numeric character '%c' at index %d", chr, i));
			}
		}

		u64_t m = 1;
		for(usys_t i = ii; i > 0; i--)
		{
			number += m * integer_part[i - 1];
			m *= 10;
		}

		return negative ? -number : number;
	}

	s64_t TStringView::ToInteger() const
	{
		EL_ERROR(Length() == 0, TInvalidArgumentException, "str", "empty string cannot be parsed as integer");

		u8_t integer_part[22];
		bool negative = false;
		unsigned ii = 0;

		for(usys_t i = 0; i < Length(); i++)
		{
			const char32_t chr = (*this)[i];
			if(chr == '-' && ii == 0 && !negative)
			{
				negative = true;
			}
			else if(chr >= '0' && chr <= '9')
			{
				EL_ERROR(ii >= sizeof(integer_part), TException, U"number integer-part is too big");
				integer_part[ii++] = static_cast<u8_t>(chr - '0');
			}
			else
			{
				EL_THROW(TException, TString::Format(U"encountered non-numeric character '%c' at index %d", chr, i));
			}
		}

		s64_t number = 0;
		u64_t m = 1;
		for(usys_t i = ii; i > 0; i--)
		{
			number += m * integer_part[i - 1];
			m *= 10;
		}

		return negative ? -number : number;
	}

	bool TStringView::operator==(const TStringView rhs) const
	{
		return Count() == rhs.Count() && (Count() == 0 || memcmp(Data(), rhs.Data(), Count() * sizeof(char32_t)) == 0);
	}

	bool TStringView::operator>=(const TStringView rhs) const
	{
		return !operator<(rhs);
	}

	bool TStringView::operator<=(const TStringView rhs) const
	{
		return !operator>(rhs);
	}

	bool TStringView::operator>(const TStringView rhs) const
	{
		const usys_t n = util::Min(Count(), rhs.Count());
		for(usys_t i = 0; i < n; i++)
		{
			if((*this)[i] > rhs[i])
				return true;
			if((*this)[i] < rhs[i])
				return false;
		}
		return Count() > rhs.Count();
	}

	bool TStringView::operator<(const TStringView rhs) const
	{
		const usys_t n = util::Min(Count(), rhs.Count());
		for(usys_t i = 0; i < n; i++)
		{
			if((*this)[i] < rhs[i])
				return true;
			if((*this)[i] > rhs[i])
				return false;
		}
		return Count() < rhs.Count();
	}



	double TString::ToDouble() const
	{
		return View().ToDouble();
	}

	s64_t TString::ToInteger() const
	{
		return View().ToInteger();
	}

	TString::TString(const char* const str, const usys_t maxlen)
	{
		const size_t len = str != nullptr ? (maxlen == NEG1 ? strlen(str) : strnlen(str, maxlen)) : 0;
		const array_t<const byte_t> array = array_t<const byte_t>::FromUnsafePointer((const byte_t*)str, len);
		chars = array.Pipe().Transform(TCharDecoder()).Collect();
	}

	#ifdef EL_WCHAR_IS_UTF32
		TString::TString(const wchar_t* const str, const usys_t maxlen)
		{
			const size_t len = maxlen == NEG1 ? wcslen(str) : wcsnlen(str, maxlen);
			chars.Append((const char32_t*)str, len);
		}
	#endif

	TString::TString(const char32_t* const str, const usys_t maxlen)
	{
		const usys_t len = UTF32StringLength(str, maxlen);
		chars.Append(str, len);
	}

	TString& TString::operator+=(const TStringView rhs)
	{
		chars.Append(rhs);
		return *this;
	}

	TString TString::operator+ (const TStringView rhs) const
	{
		TString tmp = *this;
		tmp += rhs;
		return tmp;
	}

	TString& TString::operator+=(const char32_t rhs)
	{
		chars.Append(rhs);
		return *this;
	}

	TString  TString::operator+ (const char32_t rhs) const
	{
		TString tmp = *this;
		tmp += rhs;
		return tmp;
	}

	bool TString::BeginsWith(const TStringView str) const
	{
		return View().BeginsWith(str);
	}

	void TString::Insert(const ssys_t pos, const TStringView str)
	{
		chars.Insert(pos, str);
	}

	void TString::Append(const TStringView str)
	{
		chars.Append(str);
	}

	bool TString::EndsWith(const TStringView txt) const
	{
		return View().EndsWith(txt);
	}

	TString TString::ExtractSequence(const array_t<const char32_t> charset, const ssys_t _start, usys_t max_length) const
	{
		TString seq;
		const usys_t start = chars.AbsoluteIndex(_start, false);
		const usys_t end = max_length == NEG1 ? chars.Count() : util::Min(chars.Count(), start + max_length);

		for(usys_t i = start; i < end; i++)
		{
			const char32_t chr = chars[i];
			if(!charset.Contains(chr))
				break;
			seq += chr;
		}

		return seq;
	}

	bool TString::operator==(const TStringView rhs) const
	{
		return View() == rhs;
	}

	bool TString::operator!=(const TStringView rhs) const
	{
		return View() != rhs;
	}

	bool TString::operator>=(const TStringView rhs) const
	{
		return View() >= rhs;
	}

	bool TString::operator<=(const TStringView rhs) const
	{
		return View() <= rhs;
	}

	bool TString::operator>(const TStringView rhs) const
	{
		return View() > rhs;
	}

	bool TString::operator<(const TStringView rhs) const
	{
		return View() < rhs;
	}

	usys_t TString::Find(const TStringView needle, const ssys_t start, const bool reverse) const
	{
		return View().Find(needle, start, reverse);
	}

	usys_t TString::Find(const char32_t needle, const ssys_t start, const bool reverse) const
	{
		return View().Find(needle, start, reverse);
	}

	usys_t TString::FindFirst(const array_t<const char32_t>& charset, const ssys_t start, const bool reverse) const
	{
		return View().FindFirst(charset, start, reverse);
	}

	TString& TString::Trim(const bool start, const bool end, const array_t<const char32_t> trim_chars)
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

	void TString::ReplaceAt(const ssys_t pos, const usys_t length, const TStringView substitute)
	{
		const usys_t n_common = util::Min(length, substitute.Length());

		for(usys_t i = 0; i < n_common; i++)
			chars[pos + i] = substitute[i];

		if(length > substitute.Length())
			chars.Remove(pos + n_common, length - n_common);
		else if(length < substitute.Length())
			chars.Insert(pos + n_common, substitute.Data() + n_common, substitute.Length() - n_common);
	}

	usys_t TString::Replace(const TStringView needle, const TStringView substitute, const ssys_t start, const bool reverse, const usys_t n_max_replacements)
	{
		EL_ERROR(needle.Length() == 0, TInvalidArgumentException, "needle", "needle must not be empty");

		if(Length() == 0)
			return 0;

		usys_t n_replace = 0;
		usys_t pos_current = chars.AbsoluteIndex(start, false);

		const ssys_t displacement = reverse ? -1 : ((ssys_t)substitute.Length() - (ssys_t)needle.Length() + 1);

		while(n_replace < n_max_replacements && pos_current < Length())
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

	bool TString::Contains(const TStringView needle) const
	{
		return View().Contains(needle);
	}

	bool TString::Contains(const char32_t needle) const
	{
		return View().Contains(needle);
	}

	TString TString::Join(array_t<const TString> list, const TStringView delimiter)
	{
		return list.Pipe().Aggregate([&delimiter](TString& result, const TString& append){
			if(result.Length() > 0)
				result.Append(delimiter);
			result.Append(append);
		}, TString());
	}

	TList<TString> TString::Split(const TStringView delimiter, const usys_t n_max, const bool skip_empty) const
	{
		usys_t start = 0;
		TList<TString> list(n_max < 256 ? n_max : 8);

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

	TList<TString> TString::Split(const char32_t delimiter, const usys_t n_max, const bool skip_empty) const
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

	TList<TString> TString::Split(const array_t<const char32_t> split_chars, const usys_t n_max, const bool skip_empty) const
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
					current_line += TStringView(U" ");
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

	kv_pair_tt<TString,TString> TString::SplitKV(const TStringView delimiter) const
	{
		const usys_t idx = Find(delimiter);
		EL_ERROR(idx == NEG1, TException, TString::Format(U"unable to find key/value delimiter %q", delimiter));
		return { SliceBE(0, idx), SliceBE(idx + delimiter.Length(), Length()) };
	}

	kv_pair_tt<TString,TString> TString::SplitKV(const char32_t delimiter) const
	{
		const usys_t idx = Find(delimiter);
		EL_ERROR(idx == NEG1, TException, TString::Format(U"unable to find key/value delimiter %q", delimiter));
		return { SliceBE(0, idx), SliceBE(idx + 1, Length()) };
	}

	TString TString::SliceSL(const ssys_t start, const usys_t length) const
	{
		return TString(View().SliceSL(start, length));
	}

	TString TString::SliceBE(const ssys_t begin, const ssys_t end) const
	{
		return TString(View().SliceBE(begin, end));
	}

	TString& TString::Pad(const char32_t pad_sign, const usys_t min_length, const EPlacement placement)
	{
		if(chars.Count() >= min_length || placement == EPlacement::NONE || pad_sign == U'\0')
			return *this;

		const usys_t index = 	placement == EPlacement::START ? 0 :
								placement == EPlacement::END ? chars.Count() :
								chars.Count() / 2;

		chars.FillInsert(index, pad_sign, min_length - chars.Count());
		return *this;
	}

	TString TString::Padded(const char32_t pad_sign, const usys_t length)
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

	void TString::Escape(const array_t<const char32_t> special_chars, const char32_t escape_sign)
	{
		for(usys_t i = 0; i < chars.Count(); i++)
			if(special_chars.Contains(chars[i]) || chars[i] == escape_sign)
			{
				chars.Insert(i, escape_sign);
				i++;
			}
	}

	void TString::Unescape(const array_t<const char32_t> special_chars, const char32_t escape_char)
	{
		for(usys_t i = 0; i < chars.Count(); i++)
		{
			if(chars[i] == escape_char)
			{
				EL_ERROR(i + 1 == chars.Count(), TException, TString::Format(U"found escape character at last position of input string %q", *this));
				chars.Remove(i, 1);
			}
			else
			{
				EL_ERROR(special_chars.Contains(chars[i]), TException, TString::Format(U"found unescaped special character %c at position %d in input string %q", chars[i], i, *this));
			}
		}
	}

	void TString::Quote(const char32_t quote_sign, const char32_t escape_sign)
	{
		Escape(array_t<const char32_t>::FromUnsafePointer(&quote_sign, 1), escape_sign);
		chars.Insert(0, quote_sign);
		chars.Append(quote_sign);
	}

	void TString::Unquote(const char32_t quote_sign, const char32_t escape_sign)
	{
		EL_ERROR(chars[0] != quote_sign || chars[-1] != quote_sign, TInvalidArgumentException, "this", "the input string does not start and end with a quoute character");
		chars.Cut(1, 1);
		Unescape(array_t<const char32_t>::FromUnsafePointer(&quote_sign, 1), escape_sign);
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
		chars.Apply([&](char32_t& current_char) {
			for(usys_t i = 0; i < map.Count(); i++)
				if(map[i].arr[reverse ? 1 : 0] == current_char)
				{
					current_char = map[i].arr[reverse ? 0 : 1];
					break;
				}
		});
	}

	usys_t TString::ReplaceChars(array_t<const char32_t> list, const char32_t replacement, const bool whitelist)
	{
		usys_t n = 0;
		for(char32_t& chr : chars)
			if(list.Contains(chr) != whitelist)
			{
				chr = replacement;
				n++;
			}
		return n;
	}

	TString& TString::ToLower()
	{
		Translate(LetterCaseMap(), true);
		return *this;
	}

	TString& TString::ToUpper()
	{
		Translate(LetterCaseMap(), false);
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


}

namespace std
{
	std::ostream& operator<<(std::ostream& os, const el1::io::text::string::TString& str)
	{
		os<<str.MakeCStr().get();
		return os;
	}
}
