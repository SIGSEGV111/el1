#pragma once

#include "io_types.hpp"
#include "io_text.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_stream_producer.hpp"
#include "io_text_parser.hpp"
#include <concepts>
#include <cmath>
#include <limits>
#include <type_traits>
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
		MAP = 5			// => TJsonMap
	};

	const char* JsonTypeToString(const EType type);

	// escapes and quotes the input string
	TString JsonQuote(const TStringView input);
	TString JsonUnquote(const TStringView input);

	class TJsonValue
	{
		protected:
			enum class ENumberRepresentation : u8_t
			{
				FLOATING,
				SIGNED_INTEGER,
				UNSIGNED_INTEGER
			};

			union number_t
			{
				double floating;
				s64_t signed_integer;
				u64_t unsigned_integer;
			};

			enum class EStorageType : usys_t
			{
				NULLVALUE = static_cast<usys_t>(EType::NULLVALUE),
				BOOLEAN = static_cast<usys_t>(EType::BOOLEAN),
				NUMBER_FLOATING = static_cast<usys_t>(EType::NUMBER),
				STRING = static_cast<usys_t>(EType::STRING),
				ARRAY = static_cast<usys_t>(EType::ARRAY),
				MAP = static_cast<usys_t>(EType::MAP),
				NUMBER_SIGNED_INTEGER,
				NUMBER_UNSIGNED_INTEGER
			};

			static_assert(sizeof(number_t) == sizeof(u64_t));

			EStorageType type;
			union
			{
				bool boolean;
				number_t number;

				struct
				{
					byte_t __placeholder[util::Max(sizeof(TString), sizeof(TList<void*>), sizeof(TSortedMap<void*,void*>))];
				};
			};

			void Destruct() noexcept;

			ENumberRepresentation NumberRepresentation() const noexcept
			{
				if(type == EStorageType::NUMBER_SIGNED_INTEGER)
					return ENumberRepresentation::SIGNED_INTEGER;
				if(type == EStorageType::NUMBER_UNSIGNED_INTEGER)
					return ENumberRepresentation::UNSIGNED_INTEGER;
				return ENumberRepresentation::FLOATING;
			}

		public:
			EType Type() const EL_GETTER
			{
				if(type == EStorageType::NUMBER_SIGNED_INTEGER || type == EStorageType::NUMBER_UNSIGNED_INTEGER)
					return EType::NUMBER;
				return static_cast<EType>(type);
			}

			bool operator==(const TJsonValue& rhs) const EL_GETTER;
			bool operator!=(const TJsonValue& rhs) const EL_GETTER;

			TJsonValue& operator[](const TStringView key) EL_GETTER  { return Map()[key]; }
			const TJsonValue& operator[](const TStringView key) const EL_GETTER { return Map()[key]; }
			TJsonValue& operator[](const char* const key) EL_GETTER  { return Map()[key]; }
			const TJsonValue& operator[](const char* const key) const EL_GETTER { return Map()[key]; }

			TJsonValue& operator[](const ssys_t index) EL_LIFETIME_BOUND { return Array()[index]; }
			const TJsonValue& operator[](const ssys_t index) const EL_LIFETIME_BOUND EL_GETTER { return const_cast<TJsonValue*>(this)->Array()[index]; }

			const TJsonValue& operator()(const TStringView key) const EL_GETTER;

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

			bool IsNumber() const { return Type() == EType::NUMBER; }
			bool IsNumeric() const { return IsNumber(); }
			double ToDouble() const EL_GETTER;
			double Number() const EL_GETTER;
			double Number(const double _default) const EL_GETTER;
			explicit operator double() const { return Number(); }

			template<std::integral T>
			requires (!std::same_as<std::remove_cv_t<T>, bool>)
			T ToInteger() const EL_GETTER
			{
				using value_t = std::remove_cv_t<T>;
				EL_ERROR(Type() != EType::NUMBER, TException, TString::Format(U"requested integer value, but contains %s", JsonTypeToString(Type())));

				switch(NumberRepresentation())
				{
					case ENumberRepresentation::SIGNED_INTEGER:
						EL_ERROR(!std::in_range<value_t>(number.signed_integer), TException, U"JSON number is outside requested integer range");
						return static_cast<value_t>(number.signed_integer);

					case ENumberRepresentation::UNSIGNED_INTEGER:
						EL_ERROR(!std::in_range<value_t>(number.unsigned_integer), TException, U"JSON number is outside requested integer range");
						return static_cast<value_t>(number.unsigned_integer);

					case ENumberRepresentation::FLOATING:
						break;
				}

				const double value = number.floating;
				EL_ERROR(!std::isfinite(value) || std::trunc(value) != value, TException, U"JSON number has a fractional part and cannot be converted to an integer");

				const double upper_bound = std::ldexp(1.0, std::numeric_limits<value_t>::digits);
				if constexpr(std::is_signed_v<value_t>)
					EL_ERROR(value < -upper_bound || value >= upper_bound, TException, U"JSON number is outside requested integer range");
				else
					EL_ERROR(value < 0.0 || value >= upper_bound, TException, U"JSON number is outside requested integer range");

				return static_cast<value_t>(value);
			}

			template<std::integral T>
			requires (!std::same_as<std::remove_cv_t<T>, bool>)
			T ToInteger(const T _default) const EL_GETTER
			{
				return IsNull() ? _default : ToInteger<T>();
			}

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

			template<std::integral T>
			requires (!std::same_as<std::remove_cv_t<T>, bool>)
			TJsonValue(const T integer)
			{
				if constexpr(std::is_signed_v<T>)
				{
					type = EStorageType::NUMBER_SIGNED_INTEGER;
					number.signed_integer = static_cast<s64_t>(integer);
				}
				else
				{
					type = EStorageType::NUMBER_UNSIGNED_INTEGER;
					number.unsigned_integer = static_cast<u64_t>(integer);
				}
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

			static TJsonValue Parse(const TStringView str, const bool tolerant = false);
			static TJsonValue Parse(ITextReader& reader, const bool tolerant = false);
			static TJsonValue Parse(const file::TFile& file, const bool tolerant = false);
	};

	class TJsonParser
	{
		static u16_t ConvertCodeUnit(TStringView token);
		static std::optional<TJsonValue> ConvertNumber(TStringView token);
		static std::optional<TJsonValue> ConvertNumeric(TStringView token);

	public:
		static constexpr auto Parser(const bool tolerant = false)
		{
			using namespace text::parser;

			return Recursive<TJsonValue>([tolerant](auto self)
			{
				auto whitespace = Discard(Repeat(0, NEG1, CharList(U' ', U'\t', U'\n', U'\r')));
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
				auto code_unit = Translate(ConvertCodeUnit, Capture(Repeat(4, 4, hex)));
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
					Between(U'"'_P, Repeat(0, NEG1, plain || escaped), U'"'_P)
				);
				auto string = token(raw_string);
				auto string_value = Translate([](TString text) { return TJsonValue(std::move(text)); }, raw_string);

				// RFC 8259 number grammar:
				// -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
				auto digit = CharRange(U'0', U'9');
				auto zero = Repeat(1, 1, U'0'_P);
				auto nonzero_integer = CharRange(U'1', U'9') + Repeat(0, NEG1, digit);
				auto integer_part = zero || nonzero_integer;
				auto sign = Maybe(U'-'_P);
				auto fraction = Repeat(1, 1, U'.'_P) + OneOrMore(digit);
				auto exponent = Repeat(1, 1, CharList(U'e', U'E')) + Maybe(CharList(U'+', U'-')) + OneOrMore(digit);
				auto number_token = Capture(sign + integer_part + Maybe(fraction) + Maybe(exponent));
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
