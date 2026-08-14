#pragma once

#include "io_types.hpp"
#include "io_text.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_stream_producer.hpp"
#include "io_text_parser.hpp"
#include <concepts>
#include <limits>
#include <utility>

namespace el1::io::file
{
	class TFile;
}

namespace el1::io::format::json
{
	using namespace io::types;
	using namespace io::text;
	using namespace io::text::string;
	using namespace io::collection::list;
	using namespace io::collection::map;

	class TJsonValue;
	using TJsonMap = TSortedMap<TString, TJsonValue>;
	using TConstJsonMap = TSortedMap<TString, const TJsonValue>;
	using TJsonArray = TList<TJsonValue>;
	using TConstJsonArray = const array_t<const TJsonValue>;


	struct TInvalidJsonException : IException
	{
		const iosize_t pos;
		const iosize_t line;
		const char32_t chr;

		TString Message() const final override;
		IException* Clone() const override;

		TInvalidJsonException(const iosize_t pos, const iosize_t line, const char32_t chr) : pos(pos), line(line), chr(chr) {}
	};

	enum class EType : usys_t // usys_t required for alignment of TJsonValue::__placeholder
	{
		NULLVALUE = 0,	// => IsNull() / SetNull()
		BOOLEAN = 1,	// => bool
		NUMBER = 2,		// => double
		STRING = 3,		// => TString
		ARRAY = 4,		// => TJsonArray
		MAP = 5,			// => TJsonMap
		INTEGER = 6		// => s64_t
	};

	const char* JsonTypeToString(const EType type);

	// escapes and quotes the input string
	TString JsonQuote(const TString& input);
	TString JsonUnquote(const TString& input);

	class TJsonValue
	{
		protected:
			EType type;
			union
			{
				bool boolean;
				double number;
				s64_t integer;

				struct
				{
					byte_t __placeholder[util::Max(sizeof(TString), sizeof(TList<void*>), sizeof(TSortedMap<void*,void*>))];
				};
			};

			void Destruct() noexcept;

		public:
			EType Type() const EL_GETTER
			{
				return type;
			}

			bool operator==(const TJsonValue& rhs) const EL_GETTER;
			bool operator!=(const TJsonValue& rhs) const EL_GETTER;

			TJsonValue& operator[](const TString& key) EL_GETTER  { return Map()[key]; }
			const TJsonValue& operator[](const TString& key) const EL_GETTER { return Map()[key]; }
			TJsonValue& operator[](const char* const key) EL_GETTER  { return Map()[key]; }
			const TJsonValue& operator[](const char* const key) const EL_GETTER { return Map()[key]; }

			TJsonValue& operator[](const ssys_t index) EL_LIFETIME_BOUND { return Array()[index]; }
			const TJsonValue& operator[](const ssys_t index) const EL_LIFETIME_BOUND EL_GETTER { return const_cast<TJsonValue*>(this)->Array()[index]; }

			const TJsonValue& operator()(const TString& key) const EL_GETTER;

			#if (__SIZEOF_SIZE_T__ != __SIZEOF_INT__)	// ssys_t vs. int
				TJsonValue& operator[](const int index) EL_LIFETIME_BOUND { return Array()[index]; }
				const TJsonValue& operator[](const int index) const EL_LIFETIME_BOUND EL_GETTER { return const_cast<TJsonValue*>(this)->Array()[index]; }
			#endif

			bool IsNull() const EL_GETTER;
			void SetNull() EL_SETTER;

			bool IsBoolean() const { return Type() == EType::BOOLEAN; }
			bool& Boolean() EL_GETTER;
			const bool& Boolean() const EL_GETTER;
			const bool& Boolean(const bool& _default) const EL_GETTER;
			explicit operator bool&() { return Boolean(); }
			explicit operator const bool&() const { return Boolean(); }

			bool IsInteger() const { return Type() == EType::INTEGER; }
			s64_t& Integer() EL_GETTER;
			const s64_t& Integer() const EL_GETTER;
			const s64_t& Integer(const s64_t& _default) const EL_GETTER;
			explicit operator s64_t&() { return Integer(); }
			explicit operator const s64_t&() const { return Integer(); }

			bool IsNumber() const { return Type() == EType::NUMBER; }
			bool IsNumeric() const { return IsInteger() || IsNumber(); }
			double ToDouble() const EL_GETTER;
			double& Number() EL_GETTER;
			const double& Number() const EL_GETTER;
			const double& Number(const double& _default) const EL_GETTER;
			explicit operator double&() { return Number(); }
			explicit operator const double&() const { return Number(); }

			bool IsString() const { return Type() == EType::STRING; }
			TString& String();
			const TString& String() const;
			const TString& String(const TString& _default) const;
			explicit operator TString&() { return String(); }
			explicit operator const TString&() const { return String(); }

			bool IsArray() const { return Type() == EType::ARRAY; }
			TJsonArray& Array() EL_LIFETIME_BOUND EL_GETTER;
			array_t<const TJsonValue> Array() const EL_LIFETIME_BOUND EL_GETTER;
			array_t<const TJsonValue> Array(const array_t<const TJsonValue>& _default) const EL_GETTER;
			explicit operator TJsonArray&() { return Array(); }
			explicit operator array_t<const TJsonValue>() const { return Array(); }

			bool IsMap() const { return Type() == EType::MAP; }
			TJsonMap& Map() EL_GETTER;
			const TConstJsonMap& Map() const EL_GETTER;
			const TConstJsonMap& Map(const TConstJsonMap& _default) const EL_GETTER;
			explicit operator TJsonMap&() { return Map(); }
			explicit operator const TConstJsonMap&() const { return Map(); }

			TJsonValue& operator=(const TJsonValue& rhs);
			TJsonValue& operator=(TJsonValue&& rhs);

			TJsonValue(const bool boolean);
			TJsonValue(const s64_t integer);

			template<std::integral T>
			requires (!std::same_as<std::remove_cv_t<T>, bool> && !std::same_as<std::remove_cv_t<T>, s64_t>)
			TJsonValue(const T integer)
			{
				type = EType::INTEGER;
				if constexpr(std::is_unsigned_v<T>)
					EL_ERROR((u64_t)integer > (u64_t)std::numeric_limits<s64_t>::max(), TInvalidArgumentException, "integer", "JSON integer exceeds s64_t range");
				this->integer = (s64_t)integer;
			}

			TJsonValue(const double number);

			TJsonValue(const TString& string);
			TJsonValue(TString&& string);

			TJsonValue(const char* const string);

			TJsonValue(const array_t<const TJsonValue> array);
			TJsonValue(const TJsonArray& array);
			TJsonValue(TJsonArray&& array);

			TJsonValue(const TConstJsonMap& map);
			TJsonValue(TJsonMap&& map);

			TJsonValue(const TJsonValue&);
			TJsonValue(TJsonValue&&);

			TJsonValue();
			~TJsonValue();

			TString ToString() const;
			void ToStream(stream::ISink<char32_t>&) const;
			stream::producer::TProducerPipe<char32_t> Pipe() const;

			static const TJsonValue NULLVALUE;
			static const TJsonValue TRUE;
			static const TJsonValue FALSE;
			static const TJsonValue ZERO;
			static const TJsonValue EMPTY_STRING;
			static const TJsonValue EMPTY_ARRAY;
			static const TJsonValue EMPTY_MAP;

			static TJsonValue Parse(const TString& str, const bool tolerant = false);
			static TJsonValue Parse(ITextReader& reader, const bool tolerant = false);
			static TJsonValue Parse(const file::TFile& file, const bool tolerant = false);
	};

	class TJsonParser
	{
		static u16_t ConvertCodeUnit(TStringView token);
		static std::optional<TJsonValue> ConvertInteger(TStringView token);
		static std::optional<TJsonValue> ConvertNumber(TStringView token);
		static std::optional<TJsonValue> ConvertNumeric(TStringView token);

	public:
		static constexpr auto Parser(const bool tolerant = false)
		{
			using namespace text::parser;

			return Recursive<TJsonValue>([tolerant](auto self)
			{
				auto whitespace = Discard(Repeat(CharList(U' ', U'\t', U'\n', U'\r'), 0, NEG1));
				auto token = [whitespace](auto parser)
				{
					return whitespace + std::move(parser) + whitespace;
				};

				auto boundary = LookAhead(Discard(CharList(U',', U']', U'}'))) || End();
				auto value = [boundary](auto parser)
				{
					return std::move(parser) + boundary;
				};

				// JSON string grammar. Escapes dispatch on the character following '\\'
				// so common escapes and Unicode escapes do not re-run unrelated branches.
				auto hex = CharRange(U'0', U'9') || CharRange(U'A', U'F') || CharRange(U'a', U'f');
				auto code_unit = Translate(ConvertCodeUnit, Capture(Repeat(hex, 4, 4)));
				auto first_unicode_unit = Discard(U'u'_P) + Expect(code_unit);
				auto unicode_unit = Discard(U"\\u"_P) + Expect(code_unit);
				auto unicode_scalar = Translate(
					[](const u16_t v) { return (char32_t)v; },
					Where(
						[](const u16_t v) { return v < 0xd800 || v > 0xdfff; },
						first_unicode_unit
					)
				);
				auto high_surrogate = Where(
					[](const u16_t v) { return v >= 0xd800 && v <= 0xdbff; },
					first_unicode_unit
				);
				auto low_surrogate = Expect(Validate(
					[](const u16_t v) { return v >= 0xdc00 && v <= 0xdfff; },
					unicode_unit
				));
				auto surrogate_pair = Translate(
					[](const u16_t high, const u16_t low) -> char32_t
					{
						return (char32_t)(0x10000u + (((u32_t)high - 0xd800u) << 10) + ((u32_t)low - 0xdc00u));
					},
					high_surrogate, low_surrogate
				);
				auto invalid_surrogate = Translate(
					[](const u16_t) -> char32_t { EL_THROW(TLogicException); },
					Validate([](const u16_t) { return false; }, first_unicode_unit)
				);
				auto unicode_escape = unicode_scalar || surrogate_pair || invalid_surrogate;

				auto simple_escape_code = CharList(U'"', U'\\', U'/', U'b', U'f', U'n', U'r', U't');
				auto simple_escape = Translate(
					[](const char32_t code)
					{
						switch(code)
						{
							case U'"': return U'"';
							case U'\\': return U'\\';
							case U'/': return U'/';
							case U'b': return U'\b';
							case U'f': return U'\f';
							case U'n': return U'\n';
							case U'r': return U'\r';
							case U't': return U'\t';
							default: EL_THROW(TLogicException);
						}
					},
					simple_escape_code
				);
				constexpr char32_t unicode_max = (char32_t)0x10ffff;
				auto non_unicode_escape = CharRange((char32_t)0, U't') || CharRange(U'v', unicode_max);
				auto escaped = Discard(U'\\'_P) + Dispatch(
					Case(simple_escape_code, simple_escape),
					Case(U'u'_P, unicode_escape),
					Case(non_unicode_escape, If(tolerant, non_unicode_escape))
				);
				auto plain = ~(
					CharList(U'"', U'\\') ||
					If(!tolerant, CharRange((char32_t)0, (char32_t)0x1f))
				);
				auto raw_string = Translate(
					[](TList<char32_t> chars) { return TString(std::move(chars)); },
					Between(U'"'_P, Repeat(plain || escaped, 0, NEG1), U'"'_P)
				);
				auto string = token(raw_string);
				auto string_value = Translate([](TString text) { return TJsonValue(std::move(text)); }, raw_string);

				// RFC 8259 number grammar:
				// -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
				auto digit = CharRange(U'0', U'9');
				auto zero = Repeat(U'0'_P, 1, 1);
				auto nonzero_integer = CharRange(U'1', U'9') + Repeat(digit, 0, NEG1);
				auto integer_part = zero || nonzero_integer;
				auto sign = Optional(U'-'_P);
				auto fraction = Repeat(U'.'_P, 1, 1) + OneOrMore(digit);
				auto exponent = Repeat(CharList(U'e', U'E'), 1, 1) + Optional(CharList(U'+', U'-')) + OneOrMore(digit);
				auto number_token = Capture(sign + integer_part + Optional(fraction) + Optional(exponent));
				auto number = TryTranslate(ConvertNumeric, number_token);

				auto true_value = Translate([](TStringView) { return TJsonValue(true); }, Capture(U"true"_P));
				auto false_value = Translate([](TStringView) { return TJsonValue(false); }, Capture(U"false"_P));
				auto null_value = Translate([](TStringView) { return TJsonValue(); }, Capture(U"null"_P));

				auto array = Translate(
					[](TJsonArray values) { return TJsonValue(std::move(values)); },
					Between(token(U'['_P), SeparatedBy(self, token(U','_P)), token(U']'_P))
				);

				auto member = Translate(
					[](TString key, TJsonValue member_value)
					{
						return TJsonMap::kv_pair_t{std::move(key), std::move(member_value)};
					},
					string + Discard(token(U':'_P)), self
				);
				auto map = Translate(
					[](TList<TJsonMap::kv_pair_t> members)
					{
						return TJsonValue(TJsonMap(std::move(members)));
					},
					Between(token(U'{'_P), SeparatedBy(member, token(U','_P)), token(U'}'_P))
				);

				auto raw_value = Dispatch(
					Case(U'n'_P, null_value),
					Case(U't'_P, true_value),
					Case(U'f'_P, false_value),
					Case(U'-'_P || CharRange(U'0', U'9'), number),
					Case(U'"'_P, string_value),
					Case(U'['_P, array),
					Case(U'{'_P, map)
				);
				return value(token(raw_value));
			});
		}
	};
}
