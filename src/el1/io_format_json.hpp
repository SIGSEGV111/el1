#pragma once

#include "io_types.hpp"
#include "io_text.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_stream_producer.hpp"

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

	enum class EReason : u8_t
	{
		UNEXPECTED_EOF,
		SYNTAX_ERROR,
		NUMBER_TOO_BIG,
		UNESCAPED_CTRL,
		INVALID_ESCAPE,
	};

	struct TInvalidJsonException : IException
	{
		const iosize_t pos;
		const iosize_t line;
		const TUTF32 chr;
		const EReason reason;

		TString Message() const final override;
		IException* Clone() const override;

		TInvalidJsonException(const iosize_t pos, const iosize_t line, const TUTF32 chr, const EReason reason) : pos(pos), line(line), chr(chr), reason(reason) {}
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

			TJsonValue& operator[](const ssys_t index) { return Array()[index]; }
			const TJsonValue& operator[](const ssys_t index) const EL_GETTER { return Array()[index]; }

			const TJsonValue& operator()(const TString& key) const EL_GETTER;

			#if (__SIZEOF_SIZE_T__ != __SIZEOF_INT__)	// ssys_t vs. int
				TJsonValue& operator[](const int index) { return Array()[index]; }
				const TJsonValue& operator[](const int index) const EL_GETTER { return Array()[index]; }
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
			TJsonArray& Array() EL_GETTER;
			const array_t<const TJsonValue>& Array() const EL_GETTER;
			const array_t<const TJsonValue>& Array(const array_t<const TJsonValue>& _default) const EL_GETTER;
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
			void ToStream(stream::ISink<TUTF32>&) const;
			stream::producer::TProducerPipe<TUTF32> Pipe() const;

			static const TJsonValue NULLVALUE;
			static const TJsonValue TRUE;
			static const TJsonValue FALSE;
			static const TJsonValue ZERO;
			static const TJsonValue EMPTY_STRING;
			static const TJsonValue EMPTY_ARRAY;
			static const TJsonValue EMPTY_MAP;

			static TJsonValue Parse(const TString& str, const bool tolerant = false);
			static TJsonValue Parse(const file::TFile& file, const bool tolerant = false);
	};

	class TJsonParser
	{
		protected:
			iosize_t pos;
			iosize_t line;
			const TUTF32* current_char;
			TJsonValue value;
			const bool tolerant;

			static bool IsWhitespace(const TUTF32& chr)
			{
				return chr.code == ' ' || chr.code == '\t' || chr.code == '\n';
			}

			template<typename TSourceStream>
			const TUTF32* NextChar(TSourceStream* const source)
			{
				if(current_char != nullptr)
					return current_char;

				current_char = source->NextItem();
				if(current_char == nullptr)
					return nullptr;
				pos++;
				return current_char;
			}

			void ConsumeChar()
			{
				current_char = nullptr;
			}

			template<typename TSourceStream>
			void EatLiteral(TSourceStream* const source, const char* const literal)
			{
				for(usys_t i = 0; literal[i] != 0; i++)
				{
					const TUTF32* const chr = NextChar(source);
					EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);
					EL_ERROR(chr->code != (unsigned char)literal[i], TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
					ConsumeChar();
				}
			}

			template<typename TSourceStream>
			const TUTF32* EatWhitespace(TSourceStream* const source)
			{
				const TUTF32* chr;
				for(chr = NextChar(source); chr != nullptr && IsWhitespace(*chr); chr = NextChar(source))
				{
					if(chr->code == '\n')
						line++;
					ConsumeChar();
				}
				return chr;
			}

			template<typename TSourceStream>
			TJsonValue ParseArray(TSourceStream* const source)
			{
				TJsonArray list;

				{
					const TUTF32* const chr = NextChar(source);
					EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);
					EL_ERROR(chr->code != '[', TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
					ConsumeChar();
				}

				for(;;)
				{
					const TUTF32* chr = EatWhitespace(source);
					EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);

					if(chr->code == ']')
					{
						ConsumeChar();
						break;
					}
					else
					{
						list.Append(ParseAny(source));
						chr = EatWhitespace(source);
						EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);

						if(chr->code == ']')
						{
							ConsumeChar();
							break;
						}
						else if(chr->code == ',')
						{
							ConsumeChar();
							chr = EatWhitespace(source);
							EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);
							EL_ERROR(chr->code == ']', TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
						}
						else
							EL_THROW(TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
					}
				}

				return TJsonValue(std::move(list));
			}

			template<typename TSourceStream>
			TJsonValue ParseMap(TSourceStream* const source)
			{
				TJsonMap map;

				{
					const TUTF32* const chr = NextChar(source);
					EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);
					EL_ERROR(chr->code != '{', TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
					ConsumeChar();
				}

				for(;;)
				{
					const TUTF32* chr = EatWhitespace(source);
					EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);

					if(chr->code == '}')
					{
						ConsumeChar();
						break;
					}
					else
					{
						TJsonValue key = ParseString(source);
						chr = EatWhitespace(source);
						EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);
						EL_ERROR(chr->code != ':', TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
						ConsumeChar();
						TJsonValue value = ParseAny(source);
						map.Add(std::move(key.String()), std::move(value));

						chr = EatWhitespace(source);
						EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);

						if(chr->code == '}')
						{
							ConsumeChar();
							break;
						}
						else if(chr->code == ',')
						{
							ConsumeChar();
							chr = EatWhitespace(source);
							EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);
							EL_ERROR(chr->code == '}', TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
						}
						else
							EL_THROW(TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
					}
				}

				return TJsonValue(std::move(map));
			}

			template<typename TSourceStream>
			TJsonValue ParseString(TSourceStream* const source)
			{
				TString str;
				bool escape = false;

				{
					const TUTF32* const chr = NextChar(source);
					EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);
					EL_ERROR(chr->code != '\"', TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
					ConsumeChar();
				}

				for(;;)
				{
					const TUTF32* const chr = NextChar(source);
					EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);
					EL_ERROR(!tolerant && chr->code < 32, TInvalidJsonException, pos, line, *chr, EReason::UNESCAPED_CTRL);

					if(chr->code == '\\')
					{
						if(escape)
						{
							str += '\\';
							escape = false;
						}
						else
						{
							escape = true;
						}
					}
					else if(chr->code == '\"')
					{
						if(escape)
						{
							str += '\"';
							escape = false;
						}
						else
						{
							ConsumeChar();
							break;
						}
					}
					else
					{
						if(escape)
						{
							switch(chr->code)
							{
								case 'b':
									str += '\b';
									break;
								case 'f':
									str += '\f';
									break;
								case 'n':
									str += '\n';
									break;
								case 'r':
									str += '\r';
									break;
								case 't':
									str += '\t';
									break;

								case 'u':
								{
									EL_NOT_IMPLEMENTED;
								}

								default:
								{
									EL_ERROR(!tolerant, TInvalidJsonException, pos, line, *chr, EReason::INVALID_ESCAPE);
									str += chr;
								}
							}
							escape = false;
						}
						else
						{
							str += *chr;
						}
					}

					ConsumeChar();
				}

				return TJsonValue(std::move(str));
			}

			template<typename TSourceStream>
			TJsonValue ParseNumber(TSourceStream* const source)
			{
				double number = 0.0;
				double divider = 10.0;

				u8_t integer_part[22];
				bool parse_int = true;
				bool negative = false;
				unsigned ii = 0;

				for(;;)
				{
					const TUTF32* const chr = NextChar(source);
					EL_ERROR(chr == nullptr && ii == 0 && parse_int, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);

					if(chr == nullptr)
						break;
					else if(chr->code == '-' && parse_int && ii == 0 && !negative)
					{
						negative = true;
					}
					else if(chr->code == '.' && parse_int)
					{
						parse_int = false;
					}
					else if(chr->code >= '0' && chr->code <= '9')
					{
						if(parse_int)
						{
							EL_ERROR(ii >= sizeof(integer_part), TInvalidJsonException, pos, line, *chr, EReason::NUMBER_TOO_BIG);
							integer_part[ii++] = ((u8_t)(chr->code - '0'));
						}
						else
						{
							number += (chr->code - '0') / divider;
							divider *= 10.0;
						}
					}
					else if(IsWhitespace(*chr) || chr->code == ',' || chr->code == ']' || chr->code == '}')
						break;
					else
						EL_THROW(TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);

					ConsumeChar();
				}

				u64_t m = 1;
				for(usys_t i = ii; i > 0; i--)
				{
					number += m * integer_part[i-1];
					m *= 10;
				}

				if(negative)
					number *= -1.0;

				return TJsonValue(number);
			}

			template<typename TSourceStream>
			TJsonValue ParseNull(TSourceStream* const source)
			{
				EatLiteral(source, "null");
				value.SetNull();
				return TJsonValue();
			}

			template<typename TSourceStream>
			TJsonValue ParseBoolean(TSourceStream* const source)
			{
				const TUTF32* const chr = NextChar(source);
				EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);

				if(chr->code == 't')
				{
					EatLiteral(source, "true");
					return TJsonValue(true);
				}
				else if(chr->code == 'f')
				{
					EatLiteral(source, "false");
					return TJsonValue(false);
				}
				else
					EL_THROW(TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
			}

			template<typename TSourceStream>
			TJsonValue ParseAny(TSourceStream* const source)
			{
				const TUTF32* const chr = EatWhitespace(source);
				EL_ERROR(chr == nullptr, TInvalidJsonException, pos, line, TUTF32::TERMINATOR, EReason::UNEXPECTED_EOF);

				if(chr->code == '[')
					return ParseArray(source);
				else if(chr->code == '{')
					return ParseMap(source);
				else if(chr->code == '\"')
					return ParseString(source);
				else if( (chr->code >= '0' && chr->code <= '9') || chr->code == '-' || chr->code == '.' )
					return ParseNumber(source);
				else if(chr->code == 'n')
					return ParseNull(source);
				else if(chr->code == 't' || chr->code == 'f')
					return ParseBoolean(source);
				else
					EL_THROW(TInvalidJsonException, pos, line, *chr, EReason::SYNTAX_ERROR);
			}

		public:
			using TIn = TUTF32;
			using TOut = TJsonValue;

			template<typename TSourceStream>
			const TJsonValue* NextItem(TSourceStream* const source)
			{
				const TUTF32* const chr = EatWhitespace(source);
				if(chr == nullptr)
					return nullptr;

				return &(value = ParseAny(source));
			}

			TJsonParser(const bool tolerant = false) : pos(0), line(1), current_char(nullptr), value(), tolerant(tolerant) {}
	};
}
