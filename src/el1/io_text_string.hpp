#pragma once

#include "io_collection_list.hpp"
#include "io_text_encoding.hpp"
#include "io_types.hpp"
#include "error.hpp"
#include "def.hpp"

namespace el1::io::text::string
{
	using namespace io::types;
	using namespace io::text::encoding;
	using namespace io::collection::list;

	enum class EPlacement : u8_t
	{
		NONE,
		START,
		MID,
		END,
	};

	struct symbol_map_t
	{
		TUTF32 arr[2];
	};

	extern const array_t<const TUTF32> OCTAL_SYMBOLS;
	extern const array_t<const TUTF32> DECIMAL_SYMBOLS;
	extern const array_t<const TUTF32> HEXADECIMAL_SYMBOLS_UC;
	extern const array_t<const TUTF32> HEXADECIMAL_SYMBOLS_LC;
	extern const array_t<const TUTF32> BINARY_SYMBOLS;
	extern const array_t<const TUTF32> CONTROL_CHARS;
	extern const array_t<const TUTF32> WHITESPACE_CHARS;
	extern const array_t<const TUTF32> ASCII_QUOTE_SYMBOLS;
	extern const array_t<const symbol_map_t> MAP_LETTER_CASE;	// [0] = lower; [1] = upper

	class TString
	{
		protected:
			static void _Format(TString& out, const TString& format, usys_t& pos);

			template<typename T>
			static void _Format(TString& out, const TString& format, usys_t& pos, const T& value);

			template<typename T, typename ... R>
			static void _Format(TString& out, const TString& format, usys_t& pos, const T& value, R const& ... rest);

		public:
			TList<TUTF32> chars;

			template<typename ... A>
			static TString Format(const TString& format, A const& ...args)
			{
				TString out;
				usys_t pos = 0;
				_Format(out, format, pos, args ...);
				_Format(out, format, pos);
				EL_ERROR(pos != format.Length(), TLogicException);
				return out;
			}

			/**
			* Parses the string using the specified format and variable arguments.
			*
			* @param format The format string specifying how to parse the input.
			* @return The number of characters consumed during parsing.
			* @throws IException if any of the placeholders cannot be parsed.
			*
			* Supported parsers:
			* - %c : single character
			* - %s : string (non-greedy)
			* - %d : decimal integer or float (depending on variable type)
			* - %x : hexadecimal integer or float
			* - %o : octal integer or float
			* - %b : boolean (supports multiple representations like "yes", "no", "true", "false", etc., case insensitive)
			*/
			template<typename ... A>
			usys_t Parse(const TString& format, A& ... a) const;

			static TString Join(array_t<const TString> list, const TString& delimiter);

			bool Contains(const TString& needle) const;
			bool Contains(const TUTF32 needle) const;
			usys_t Find(const TString& needle, const ssys_t start = 0, const bool reverse = false) const;
			usys_t Find(const TUTF32 needle, const ssys_t start = 0, const bool reverse = false) const;
			usys_t FindFirst(const array_t<const TUTF32>& charset, const ssys_t start = 0, const bool reverse = false) const;
			TString& Trim(const bool start = true, const bool end = true, const array_t<const TUTF32> trim_chars = WHITESPACE_CHARS);
			void ReplaceAt(const ssys_t pos, const usys_t length, const TString& substitute);
			usys_t Replace(const TString& needle, const TString& substitute, const ssys_t start = 0, const bool reverse = false, const usys_t n_max_replacements = NEG1);
			void Insert(const ssys_t pos, const TString& str);
			void Append(const TString& str);
			bool BeginsWith(const TString& txt) const EL_GETTER;
			bool EndsWith(const TString& txt) const EL_GETTER;

			TString SliceSL(const ssys_t start, usys_t length = NEG1) const;
			TString SliceBE(const ssys_t begin, const ssys_t end) const;

			TList<TString> Split(const TString& delimiter, const usys_t n_max = NEG1, const bool skip_empty = false) const EL_GETTER;
			TList<TString> Split(const TUTF32 delimiter, const usys_t n_max = NEG1, const bool skip_empty = false) const EL_GETTER;
			TList<TString> Split(const array_t<const TUTF32> split_chars, const usys_t n_max = NEG1, const bool skip_empty = false) const EL_GETTER;
			kv_pair_tt<TString,TString> SplitKV(const TString& delimiter) const;
			kv_pair_tt<TString,TString> SplitKV(const TUTF32 delimiter = '=') const;

			TList<TString> BlockFormat(const unsigned n_line_len) const EL_GETTER;

			TString& Pad(const TUTF32 pad_sign, const usys_t min_length, const EPlacement placement);
			TString& Reverse();
			void Escape(const array_t<const TUTF32> special_chars, const TUTF32 escape_sign);
			void Unescape(const array_t<const TUTF32> special_chars, const TUTF32 escape_char);
			void Quote(const TUTF32 quote_sign, const TUTF32 escape_sign);
			void Unquote(const TUTF32 quote_sign, const TUTF32 escape_sign);
			void Truncate(const usys_t n_max_length);
			void Cut(const usys_t n_begin, const usys_t n_end);
			void Translate(const array_t<const symbol_map_t> map, const bool reverse = false);
			TString& ToLower();
			TString& ToUpper();

			TString Lower() const EL_GETTER;
			TString Upper() const EL_GETTER;

			TString ExtractSequence(const array_t<const TUTF32> charset, const ssys_t start = 0, usys_t max_length = NEG1) const EL_GETTER;

			static TString Padded(const TUTF32 pad_sign, const usys_t length);

			double ToDouble() const EL_GETTER;
			s64_t ToInteger() const EL_GETTER;

			TString& operator+=(const TString& rhs);
			TString  operator+ (const TString& rhs) const;

			TString& operator+=(const TUTF32 rhs);
			TString  operator+ (const TUTF32 rhs) const;

			bool operator==(const TString& rhs) const EL_GETTER;
			bool operator!=(const TString& rhs) const EL_GETTER;
			bool operator>=(const TString& rhs) const EL_GETTER;
			bool operator<=(const TString& rhs) const EL_GETTER;
			bool operator> (const TString& rhs) const EL_GETTER;
			bool operator< (const TString& rhs) const EL_GETTER;

			inline usys_t Length() const noexcept EL_GETTER { return chars.Count(); }
			inline TUTF32 operator[](const ssys_t index) const EL_GETTER { return chars[index]; }
			inline TUTF32& operator[](const ssys_t index) EL_GETTER { return chars[index]; }

			std::unique_ptr<char[]> MakeCStr() const;

			TString() = default;
			TString(TString&&) = default;
			TString(const TString&) = default;
			TString(const char* const str, const usys_t maxlen = NEG1);
			TString(const wchar_t* const str, const usys_t maxlen = NEG1);
			TString(const TUTF32* const str, const usys_t maxlen = NEG1);
			TString(TList<TUTF32> chars) : chars(chars) {}

			TString& operator=(const TString&) = default;
			TString& operator=(TString&&) = default;
	};

	class TLineReader
	{
		public:
			TString buffer;
			const usys_t n_max_length;

			using TIn = TUTF32;
			using TOut = TString;

			template<typename TSourceStream>
			TString* NextItem(TSourceStream* const source)
			{
				buffer.chars.Clear(NEG1);

				const TUTF32* chr;
				while((chr = source->NextItem()) != nullptr && buffer.Length() < n_max_length)
				{
					if(chr->code == 10U) // LF
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

				EL_ERROR(chr != nullptr && buffer.Length() >= n_max_length, TException, TString::Format("maximum line length of %d characters exceeded", n_max_length));

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
		virtual TString Format(const char value) const;
		virtual TString Format(const wchar_t value) const;
		virtual TString Format(const TUTF32 value) const;
		virtual TString Format(const s8_t value) const;
		virtual TString Format(const u8_t value) const;
		virtual TString Format(const s16_t value) const;
		virtual TString Format(const u16_t value) const;
		virtual TString Format(const s32_t value) const;
		virtual TString Format(const u32_t value) const;
		virtual TString Format(const s64_t value) const;
		virtual TString Format(const u64_t value) const;
		virtual TString Format(const double value) const;
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
			TUTF32 pad_sign;
			EPlacement align;
			const array_t<const TUTF32>* quote_symbols;
			TUTF32 escape_symbol;
		};

		config_t config;
		TStringFormatter(config_t config) : config(std::move(config)) {}

		const char* FormatName() const final override;
		TString Format(const char* const value) const final override;
		TString Format(const wchar_t* const value) const final override;
		TString Format(const TString& value) const final override;
		TString Format(const char value) const final override;
		TString Format(const wchar_t value) const final override;
		TString Format(const TUTF32 value) const final override;

		TUTF32 DetectBestQuoteSymbol(const TString& str) const;

		static const TStringFormatter PLAIN;
		static const TStringFormatter ASCII_QUOTED;
	};

	struct TNumberFormatter : IFormatter
	{
		struct config_t
		{
			const array_t<const TUTF32>* symbols;
			TString prefix;
			TString suffix;
			EPlacement sign_placement;
			bool force_sign;
			TUTF32 decimal_point_sign;
			TUTF32 grouping_sign;
			TUTF32 integer_pad_sign;
			TUTF32 decimal_pad_sign;
			TUTF32 negative_sign;
			unsigned n_digits_per_group;
			unsigned n_decimal_places;
			unsigned n_min_integer_places;
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
		const array_t<const TUTF32>* symbols;

		const char* FormatName() const final override;
		TString Format(const char* const value) const final override;
		TString Format(const wchar_t* const value) const final override;
		TString Format(const TString& value) const final override;
		TString Format(const char value) const final override;
		TString Format(const wchar_t value) const final override;
		TString Format(const TUTF32 value) const final override;
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

	struct TFormatVariable
	{
// 		enum class EType
// 		{
// 			STRING,
// 			NUMBER,
// 			RAWDATA
// 		};

		usys_t pos;
		unsigned len;
		bool shared_formatter;
		const IFormatter* formatter;

		template<typename T>
		TString Evaluate(const T& value) const
		{
			return formatter->Format(value);
		}

		static TFormatVariable Find(const TString& str, const usys_t start_pos);
		static TFormatVariable Parse(const TUTF32 format_char, const TString& arg_str);

		static TFormatVariable ParseNumberFormatterArguments(const TString& arg_str, const TNumberFormatter* const default_format);
		static TFormatVariable ParseStringFormatterArguments(const TString& arg_str);
		static TFormatVariable ParseCharacterFormatterArguments(const TString& arg_str);
		static TFormatVariable ParseQuotedFormatterArguments(const TString& arg_str);

		// valid format variable definitions (extended regex):
		// number: %[ 0]?([1-9][0-9]*(\.(0|[1-9][0-9]*))?)?n
		// string with optional padding: %[0-9]*s
		// quoted string: %[^q]?q

		TFormatVariable(bool shared_formatter, const IFormatter* formatter) : pos(0), len(0), shared_formatter(shared_formatter), formatter(formatter)
		{
		}

		TFormatVariable(const TFormatVariable&) = delete;

		TFormatVariable(TFormatVariable&& other) : pos(other.pos), len(other.len), shared_formatter(other.shared_formatter), formatter(other.formatter)
		{
			other.formatter = nullptr;
		}

		~TFormatVariable()
		{
			if(!shared_formatter)
				delete formatter;
		}
	};

	/********************************************/

	template<typename T>
	void TString::_Format(TString& out, const TString& format, usys_t& pos, const T& value)
	{
		bool forwarded = false;
		try
		{
			const TFormatVariable var = TFormatVariable::Find(format, pos);
			try
			{
				EL_ERROR(var.formatter == nullptr, TMissingFormatVariableException, format);
				TString slice = format.SliceBE(pos, var.pos);
				slice.Replace("%%", "%");
				out += slice;
				out += var.Evaluate(value);
				pos = var.pos + var.len;
			}
			catch(const error::IException& e)
			{
				forwarded = true;
				EL_FORWARD(e, TException, TString::Format("error while processing format string %q near pos %d", format, var.pos + 1));
			}
		}
		catch(const error::IException& e)
		{
			if(forwarded)
				throw;
			else
				EL_FORWARD(e, TException, TString::Format("error while processing format string %q", format));
		}
	}

	template<typename T, typename ... R>
	void TString::_Format(TString& out, const TString& format, usys_t& pos, const T& value, R const& ... rest)
	{
		_Format(out, format, pos, value);
		_Format(out, format, pos, rest ...);
	}
}

namespace std
{
	std::ostream& operator<<(std::ostream& os, const el1::io::text::string::TString& str);
}
