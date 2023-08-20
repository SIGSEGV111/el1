#pragma once

#include "error.hpp"
#include "util.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_text_encoding.hpp"

namespace el1::io::text
{
	using namespace io::types;

	namespace string
	{
		class TString;
	}

	class ITextWriter;
	class IFormatter;

	template<typename T>
	class TCStrPipe : public stream::IPipe<TCStrPipe<T>, T>
	{
		protected:
			const T* cstr;
			const T* const end;

		public:
			using TOut = T;
			using TIn = void;

			const TOut* NextItem() final override
			{
				if(cstr < end && *cstr != '\0')
					return cstr++;
				else
					return nullptr;
			}

			TCStrPipe(const T* const cstr, const usys_t n_max_chars = NEG1) : cstr(cstr), end(n_max_chars == NEG1 ? (const T*)NEG1 : cstr + n_max_chars) {}

			TCStrPipe(const io::collection::list::array_t<const T> array) : cstr(array.ItemPtr(0)), end(array.ItemPtr(0) + array.Count()) {}
	};

	using TFormatterMap = collection::map::TSortedMap<encoding::TUTF32, const IFormatter*>;

	enum class EPlacement : u8_t
	{
		NONE,
		START,
		MID,
		END,
	};

	enum class ERounding : u8_t
	{
		TO_NEAREST,
		TOWARDS_ZERO,
		AWAY_FROM_ZERO,
		DOWNWARD,
		UPWARD
	};

	enum class ESignWhen : u8_t
	{
		NEGATIVE,
		NEVER,
		ALWAYS,
		POSITIVE
	};

	struct TUnsupportedDatatypeException : error::IException
	{
		const char* const input_type;
		const char* const* const supported_types;

		string::TString Message() const final override;
		IException* Clone() const  final override;

		TUnsupportedDatatypeException(const char* const input_type, const char* const* const supported_types);
	};

	class IFormatter
	{
		private:
			// this stores the previous formatter in case we replaced one during registration
			TFormatterMap* const map;
			const IFormatter* const previous;

		public:
			static TFormatterMap FORMATTERS;

			// returns the size in bytes of the derived formatter by the means of sizeof(TDerived)
			// keep in mind that this memory is then later usually allocated on the stack
			virtual usys_t Size() const = 0;

			// clones the current formatter along with its settings
			// p_mem will point to a memory location where the formatter shall be instantiated
			// returns a pointer to the cloned formatter (located somewhere in p_mem)
			virtual IFormatter& Clone(void* const p_mem) const = 0;

			// parses the given parameter specification from the format string and updates the settings of the formatter
			// the text representation of the format spec is limited to 32 characters
			virtual void ParseFormatParameters(collection::list::array_t<const encoding::TUTF32> spec) = 0;

			// performs the actuall formatting of the input data with the given parameters
			// any Format() function that is not implemented by the formatter will raise an TUnsupportedDatatypeException
			virtual void Format(stream::ISink<encoding::TUTF32>& out, u64_t v) const;
			virtual void Format(stream::ISink<encoding::TUTF32>& out, s64_t v) const;
			virtual void Format(stream::ISink<encoding::TUTF32>& out, double v) const;
			virtual void Format(stream::ISink<encoding::TUTF32>& out, const string::TString& v) const;

			// constructor/destructor handle automatic registration (e.g. with IFormatter::FORMATTERS)
			// if you do not specify a key then the formatter will NOT be registered and can only be used explicitly
			// there is no mutex to protect IFormatter::FORMATTERS - thus registration must happen before any threads are started
			virtual ~IFormatter();
			constexpr IFormatter() : map(nullptr), previous(nullptr) {}
			IFormatter(const encoding::TUTF32 key, TFormatterMap* const map);
	};

	class ITextWriter : public stream::ISink<encoding::TUTF32>
	{
		public:
			static const unsigned N_MAX_SPEC_LEN = 32;

		private:
			template<typename T>
			void PrintVariableRecurse(collection::list::array_t<const encoding::TUTF32> spec, const unsigned idx_var, const T& v);

			template<typename T, typename ... A>
			void PrintVariableRecurse(collection::list::array_t<const encoding::TUTF32> spec, const unsigned idx_var, const T& v, const A& ... a);

			const IFormatter* active_formatter;
			const void* active_parameters;

		public:
			// queried before global IFormatter::FORMATTERS
			TFormatterMap extra_formatters;

			const IFormatter& LookupFormatter(const encoding::TUTF32 key);

			template<typename T>
			void PrintVariable(collection::list::array_t<const encoding::TUTF32> spec, const T& v);

			template<typename ... A>
			void PrintF(const char* const format, const A& ... a);

			ITextWriter() : active_formatter(nullptr), active_parameters(nullptr) {}
	};

	template<typename T>
	void ITextWriter::PrintVariable(collection::list::array_t<const encoding::TUTF32> spec, const T& v)
	{
		const IFormatter& reference_formatter = this->LookupFormatter(spec[-1]);
		alignas(util::Max<unsigned>(16U, alignof(void*), alignof(u64_t))) byte_t _formatter_mem[reference_formatter.Size()];
		IFormatter& formatter = reference_formatter.Clone(_formatter_mem);

		// cut away the format key to not confuse the parser
		spec = collection::list::array_t<const encoding::TUTF32>(spec.ItemPtr(0), spec.Count() - 1);
		formatter.ParseFormatParameters(spec);

		try
		{
			formatter.Format(*this, v);
		}
		catch(...)
		{
			formatter.~IFormatter();
			throw;
		}

		formatter.~IFormatter();
	}

	template<typename T>
	void ITextWriter::PrintVariableRecurse(collection::list::array_t<const encoding::TUTF32> spec, const unsigned idx_var, const T& v)
	{
		EL_THROW(TException, "format-string contains more placeholders than variables were passed as arguments");
	}

	template<typename T, typename ... A>
	void ITextWriter::PrintVariableRecurse(collection::list::array_t<const encoding::TUTF32> spec, const unsigned idx_var, const T& v, const A& ... a)
	{
		if(idx_var > 0)
		{
			this->PrintVariableRecurse(spec, idx_var - 1, a ...);
		}
		else
		{
			this->PrintVariable(spec, v);
		}
	}

	template<typename ... A>
	void ITextWriter::PrintF(const char* const format, const A& ... a)
	{
		using namespace io::collection::list;
		encoding::TUTF32 _format_spec[ITextWriter::N_MAX_SPEC_LEN];
		array_t<encoding::TUTF32> arr_format_spec(_format_spec, ITextWriter::N_MAX_SPEC_LEN);
		unsigned idx_var = 0;
		unsigned idx_spec = 0;
		bool variable = false;
		bool guard = false;

		TCStrPipe(format).Transform(encoding::TCharDecoder()).ForEach(
			[&](const encoding::TUTF32& chr)
			{
				if(chr == '%' && !guard)
				{
					if(variable)
					{
						variable = false;
						this->WriteAll(&chr, 1);
					}
					else
					{
						variable = true;
						idx_spec = 0;
					}
				}
				else if(variable)
				{
					if(chr == '{' && !guard)
					{
						guard = true;
					}
					else if(chr == '}' && guard)
					{
						guard = false;
					}
					else
					{
						arr_format_spec[idx_spec++] = chr;
						if(!guard && (chr.IsAsciiAlpha() || !chr.IsAscii()))
						{
							this->PrintVariableRecurse(array_t<const encoding::TUTF32>(arr_format_spec.ItemPtr(0), idx_spec), idx_var, a ...);
							variable = false;
							idx_var++;
						}
					}
				}
				else
				{
					this->WriteAll(&chr, 1);
				}
			}
		);
	}

	/*
	 * step 1: Limit the amount of possible input datatypes to an (extensible) set of well-known datatypes.
	 * 		This is achieved by using operator<<() and thus forcing the compiler to perform type casts.
	 * 		Then the casted value along with its typeid() is passed to the formatter.
	 *
	 * step 2: Call the formatter for the format-key in the format string.
	 * 		The formatter only needs to be able to handle a small set of known datatypes and can have its own set of parameters.
	 * 		The available formatters can be extended by implementing derived classes from IFormatter and instantiating them before use.
	 * 		The format-key can be any ASCII letter (A-Za-z) or any non-ASCII character.
	 *
	 * If you just want to convert a custom datatype to text and do NOT need any custom parameters then all you have to do is implement
	 * a ITextWriter& operator<<(ITextWriter&, your_datatype) which in turn can use PrintF() to do all the formatting.
	 */

	ITextWriter& operator<<(ITextWriter& w, const u8_t v);
	ITextWriter& operator<<(ITextWriter& w, const s8_t v);
	ITextWriter& operator<<(ITextWriter& w, const u16_t v);
	ITextWriter& operator<<(ITextWriter& w, const s16_t v);
	ITextWriter& operator<<(ITextWriter& w, const u32_t v);
	ITextWriter& operator<<(ITextWriter& w, const s32_t v);
	ITextWriter& operator<<(ITextWriter& w, const u64_t v);
	ITextWriter& operator<<(ITextWriter& w, const s64_t v);
	ITextWriter& operator<<(ITextWriter& w, const float v);
	ITextWriter& operator<<(ITextWriter& w, const double v);
	ITextWriter& operator<<(ITextWriter& w, const void* const v);
	ITextWriter& operator<<(ITextWriter& w, const char* const v);
	ITextWriter& operator<<(ITextWriter& w, const wchar_t* const v);
	ITextWriter& operator<<(ITextWriter& w, const io::collection::list::array_t<const encoding::TUTF32> v);
	ITextWriter& operator<<(ITextWriter& w, const encoding::TUTF32 v);

	struct TGenericNumberFormatter : IFormatter
	{
		struct TParameters
		{
			collection::list::array_t<const encoding::TUTF32> digits;

			encoding::TUTF32 decimal_point_sign;
			encoding::TUTF32 grouping_sign;
			encoding::TUTF32 integer_pad_sign;
			encoding::TUTF32 decimal_pad_sign;
			encoding::TUTF32 negative_sign;

			u8_t n_digits_per_group;
			u8_t n_min_decimal_places;
			u8_t n_max_decimal_places;
			u8_t n_min_integer_places;

			ESignWhen sign_when;
			EPlacement sign_placement;
			ERounding rounding;
		};

		TParameters parameters;

		constexpr TGenericNumberFormatter(TParameters parameters) : parameters(parameters) {}
		TGenericNumberFormatter(const encoding::TUTF32 key, TParameters parameters, TFormatterMap* const map = &IFormatter::FORMATTERS);
		TGenericNumberFormatter(ITextWriter* const w, const encoding::TUTF32 key, TParameters parameters);

		usys_t Size() const final override;
		void ParseFormatParameters(collection::list::array_t<const encoding::TUTF32> spec) final override;
		void Format(stream::ISink<encoding::TUTF32>& out, u64_t v) const final override;
		void Format(stream::ISink<encoding::TUTF32>& out, s64_t v) const final override;
		void Format(stream::ISink<encoding::TUTF32>& out, double v) const final override;

		// builtin hardcoded formats
		static const TGenericNumberFormatter PLAIN_BINARY_US_EN;			// IFormatter::FORMATTERS, key: 'b'
		static const TGenericNumberFormatter PLAIN_OCTAL_US_EN;				// IFormatter::FORMATTERS, key: 'o'
		static const TGenericNumberFormatter PLAIN_DECIMAL_US_EN;			// IFormatter::FORMATTERS, key: 'd'
		static const TGenericNumberFormatter PLAIN_HEXADECIMAL_LOWER_US_EN;	// IFormatter::FORMATTERS, key: 'x'
		static const TGenericNumberFormatter PLAIN_HEXADECIMAL_UPPER_US_EN;	// IFormatter::FORMATTERS, key: 'X'
	};

	struct TBasicStringFormatter : IFormatter
	{
		struct TParameters
		{
			encoding::TUTF32 pad_sign;
			u16_t n_min_length;
			EPlacement alignment;
		};

		TParameters parameters;

		constexpr TBasicStringFormatter(TParameters parameters) : parameters(parameters) {}
		TBasicStringFormatter(const encoding::TUTF32 key, TParameters parameters, TFormatterMap* const map = &IFormatter::FORMATTERS);
		TBasicStringFormatter(ITextWriter* const w, const encoding::TUTF32 key, TParameters parameters);

		usys_t Size() const final override;
		void ParseFormatParameters(collection::list::array_t<const encoding::TUTF32> spec) final override;
		void Format(stream::ISink<encoding::TUTF32>& out, const string::TString& v) const final override;

		static const TBasicStringFormatter PLAIN;		// IFormatter::FORMATTERS, key: 's'
	};

	struct TQuotedStringFormatter : IFormatter
	{
		struct TParameters
		{
			// if both opening_quotation_sign and closing_quotation_sign are TUTF32::TERMINATOR then the symbol is auto-detected based on the input string
			io::collection::list::array_t<const encoding::TUTF32> special_chars;
			encoding::TUTF32 special_chars_escape_sign; // if TUTF32::TERMINATOR than every special character will be escaped by repeating itself
			encoding::TUTF32 opening_quotation_sign;
			encoding::TUTF32 closing_quotation_sign;
			encoding::TUTF32 opening_quotation_escape_sign;
			encoding::TUTF32 closing_quotation_escape_sign;
		};

		TParameters parameters;

		constexpr TQuotedStringFormatter(TParameters parameters) : parameters(parameters) {}
		TQuotedStringFormatter(const encoding::TUTF32 key, TParameters parameters, TFormatterMap* const map = &IFormatter::FORMATTERS);
		TQuotedStringFormatter(ITextWriter* const w, const encoding::TUTF32 key, TParameters parameters);

		usys_t Size() const final override;
		void ParseFormatParameters(collection::list::array_t<const encoding::TUTF32> spec) final override;
		void Format(stream::ISink<encoding::TUTF32>& out, const string::TString& v) const final override;

		static const TQuotedStringFormatter QUOTED_AUTO;	// IFormatter::FORMATTERS, key: 'q'
		static const TQuotedStringFormatter SHELL_SAFE;		// IFormatter::FORMATTERS, key: 'z'
	};
}
