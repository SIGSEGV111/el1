#include "io_format_json.hpp"
#include "io_file.hpp"

namespace el1::io::format::json
{
	using namespace stream::producer;
	using namespace file;

	// LCOV_EXCL_START
	TString TInvalidJsonException::Message() const
	{
		if(chr == U'\0')
			return TString::Format(U"invalid JSON at character %d (line %d): unexpected end of input", pos, line);
		return TString::Format(U"invalid JSON at character %d (line %d) near %q", pos, line, chr);
	}
	// LCOV_EXCL_STOP

	IException* TInvalidJsonException::Clone() const
	{
		return new TInvalidJsonException(*this);
	}

	////////////////////////////////////////////////////////////////////

	const char* JsonTypeToString(const EType type)
	{
		switch(type)
		{
			case EType::NULLVALUE:
				return "null-value";

			case EType::BOOLEAN:
				return "boolean";

			case EType::NUMBER:
				return "number";

			case EType::INTEGER:
				return "integer";

			case EType::STRING:
				return "string";

			case EType::ARRAY:
				return "array";

			case EType::MAP:
				return "map";
		}

		EL_THROW(TLogicException);
	}

	////////////////////////////////////////////////////////////////////

	void TJsonValue::Destruct() noexcept
	{
		switch(type)
		{
			case EType::NULLVALUE:
			case EType::BOOLEAN:
			case EType::NUMBER:
			case EType::INTEGER:
				// nothing to do
				break;

			case EType::STRING:
				String().~TString();
				break;

			case EType::ARRAY:
				Array().~TList();
				break;

			case EType::MAP:
				Map().~TSortedMap();
				break;
		}

		type = EType::NULLVALUE;
	}

	////////////////////////////////////////////////////////////////////

	bool TJsonValue::operator==(const TJsonValue& rhs) const
	{
		if(this->type == rhs.type)
		{
			switch(this->type)
			{
				case EType::NULLVALUE:
					return true;

				case EType::BOOLEAN:
					return this->Boolean() == rhs.Boolean();

				case EType::NUMBER:
					return this->Number() == rhs.Number();

				case EType::INTEGER:
					return this->Integer() == rhs.Integer();

				case EType::STRING:
					return this->String() == rhs.String();

				case EType::ARRAY:
				{
					auto a1 = this->Array();
					auto a2 = rhs.Array();

					if(a1.Count() != a2.Count())
						return false;

					for(usys_t i = 0; i < a1.Count(); i++)
						if(a1[i] != a2[i])
							return false;

					return true;
				}

				case EType::MAP:
				{
					auto& a1 = this->Map().Items();
					auto& a2 = rhs.Map().Items();

					if(a1.Count() != a2.Count())
						return false;

					for(usys_t i = 0; i < a1.Count(); i++)
						if(a1[i].key != a2[i].key || a1[i].value != a2[i].value)
							return false;

					return true;
				}
			}
		}

		return false;
	}

	bool TJsonValue::operator!=(const TJsonValue& rhs) const
	{
		return !(*this == rhs);
	}

	////////////////////////////////////////////////////////////////////

	bool TJsonValue::IsNull() const
	{
		return type == EType::NULLVALUE;
	}

	void TJsonValue::SetNull()
	{
		*this = TJsonValue();
	}

	////////////////////////////////////////////////////////////////////

	bool& TJsonValue::Boolean()
	{
		EL_ERROR(Type() != EType::BOOLEAN, TException, TString::Format(U"requested boolean value, but contains %s", JsonTypeToString(Type())));
		return boolean;
	}

	const bool& TJsonValue::Boolean() const
	{
		return const_cast<TJsonValue*>(this)->Boolean();
	}

	const bool& TJsonValue::Boolean(const bool& _default) const
	{
		return IsBoolean() ? Boolean() : _default;
	}

	////////////////////////////////////////////////////////////////////

	s64_t& TJsonValue::Integer()
	{
		EL_ERROR(Type() != EType::INTEGER, TException, TString::Format(U"requested integer value, but contains %s", JsonTypeToString(Type())));
		return integer;
	}

	const s64_t& TJsonValue::Integer() const
	{
		return const_cast<TJsonValue*>(this)->Integer();
	}

	const s64_t& TJsonValue::Integer(const s64_t& _default) const
	{
		return IsInteger() ? Integer() : _default;
	}

	////////////////////////////////////////////////////////////////////

	double TJsonValue::ToDouble() const
	{
		EL_ERROR(!IsNumeric(), TException, TString::Format(U"requested numeric value, but contains %s", JsonTypeToString(Type())));
		return IsInteger() ? (double)Integer() : Number();
	}

	double& TJsonValue::Number()
	{
		EL_ERROR(Type() != EType::NUMBER, TException, TString::Format(U"requested number value, but contains %s", JsonTypeToString(Type())));
		return number;
	}

	const double& TJsonValue::Number() const
	{
		return const_cast<TJsonValue*>(this)->Number();
	}

	const double& TJsonValue::Number(const double& _default) const
	{
		return IsNumber() ? Number() : _default;
	}

	////////////////////////////////////////////////////////////////////

	TString& TJsonValue::String()
	{
		EL_ERROR(Type() != EType::STRING, TException, TString::Format(U"requested string value, but contains %s", JsonTypeToString(Type())));
		return *reinterpret_cast<TString*>(__placeholder);
	}

	const TString& TJsonValue::String() const
	{
		return const_cast<TJsonValue*>(this)->String();
	}

	const TString& TJsonValue::String(const TString& _default) const
	{
		return IsString() ? String() : _default;
	}

	////////////////////////////////////////////////////////////////////

	TJsonArray& TJsonValue::Array() EL_LIFETIME_BOUND
	{
		EL_ERROR(Type() != EType::ARRAY, TException, TString::Format(U"requested array value, but contains %s", JsonTypeToString(Type())));
		return *reinterpret_cast<TJsonArray*>(__placeholder);
	}

	array_t<const TJsonValue> TJsonValue::Array() const EL_LIFETIME_BOUND
	{
		return const_cast<TJsonValue*>(this)->Array();
	}

	array_t<const TJsonValue> TJsonValue::Array(const array_t<const TJsonValue>& _default) const
	{
		return IsArray() ? array_t<const TJsonValue>(const_cast<TJsonValue*>(this)->Array()) : _default;
	}

	////////////////////////////////////////////////////////////////////

	TJsonMap& TJsonValue::Map()
	{
		EL_ERROR(Type() != EType::MAP, TException, TString::Format(U"requested map value, but contains %s", JsonTypeToString(Type())));
		return *reinterpret_cast<TJsonMap*>(__placeholder);
	}

	const TConstJsonMap& TJsonValue::Map() const
	{
		return const_cast<TJsonValue*>(this)->Map();
	}

	const TConstJsonMap& TJsonValue::Map(const TConstJsonMap& _default) const
	{
		return IsMap() ? (const TConstJsonMap&)const_cast<TJsonValue*>(this)->Map() : _default;
	}

	////////////////////////////////////////////////////////////////////

	TJsonValue& TJsonValue::operator=(const TJsonValue& rhs)
	{
		if(this == &rhs)
			return *this;
		Destruct();

		switch(rhs.type)
		{
			case EType::NULLVALUE:
				break;

			case EType::BOOLEAN:
				this->boolean = rhs.boolean;
				break;

			case EType::NUMBER:
				this->number = rhs.number;
				break;

			case EType::INTEGER:
				this->integer = rhs.integer;
				break;

			case EType::STRING:
				new (__placeholder) TString(rhs.String());
				break;

			case EType::ARRAY:
				new (__placeholder) TJsonArray(rhs.Array());
				break;

			case EType::MAP:
				new (__placeholder) TJsonMap(rhs.Map());
				break;
		}

		this->type = rhs.type;
		return *this;
	}

	TJsonValue& TJsonValue::operator=(TJsonValue&& rhs)
	{
		if(this == &rhs)
			return *this;
		Destruct();

		switch(rhs.type)
		{
			case EType::NULLVALUE:
				break;

			case EType::BOOLEAN:
				this->boolean = rhs.boolean;
				break;

			case EType::NUMBER:
				this->number = rhs.number;
				break;

			case EType::INTEGER:
				this->integer = rhs.integer;
				break;

			case EType::STRING:
				new (__placeholder) TString(std::move(rhs.String()));
				break;

			case EType::ARRAY:
				new (__placeholder) TJsonArray(std::move(rhs.Array()));
				break;

			case EType::MAP:
				new (__placeholder) TJsonMap(std::move(rhs.Map()));
				break;
		}

		this->type = rhs.type;
		return *this;
	}

	////////////////////////////////////////////////////////////////////

	TJsonValue::TJsonValue(const bool boolean)
	{
		type = EType::BOOLEAN;
		this->boolean = boolean;
	}

	TJsonValue::TJsonValue(const s64_t integer)
	{
		type = EType::INTEGER;
		this->integer = integer;
	}

	TJsonValue::TJsonValue(const double number)
	{
		type = EType::NUMBER;
		this->number = number;
	}

	TJsonValue::TJsonValue(const TString& string)
	{
		type = EType::NULLVALUE;
		new (__placeholder) TString(string);
		type = EType::STRING;
	}

	TJsonValue::TJsonValue(TString&& string)
	{
		type = EType::NULLVALUE;
		new (__placeholder) TString(std::move(string));
		type = EType::STRING;
	}

	TJsonValue::TJsonValue(const char* const string)
	{
		type = EType::NULLVALUE;
		new (__placeholder) TString(string);
		type = EType::STRING;
	}

	TJsonValue::TJsonValue(const TJsonArray& array)
	{
		type = EType::NULLVALUE;
		new (__placeholder) TJsonArray(array);
		type = EType::ARRAY;
	}

	TJsonValue::TJsonValue(const array_t<const TJsonValue> array)
	{
		type = EType::NULLVALUE;
		new (__placeholder) TJsonArray(array);
		type = EType::ARRAY;
	}

	TJsonValue::TJsonValue(TJsonArray&& array)
	{
		type = EType::NULLVALUE;
		new (__placeholder) TJsonArray(std::move(array));
		type = EType::ARRAY;
	}

	TJsonValue::TJsonValue(const TConstJsonMap& map)
	{
		type = EType::NULLVALUE;
		new (__placeholder) TJsonMap(map);
		type = EType::MAP;
	}

	TJsonValue::TJsonValue(TJsonMap&& map)
	{
		type = EType::NULLVALUE;
		new (__placeholder) TJsonMap(std::move(map));
		type = EType::MAP;
	}

	////////////////////////////////////////////////////////////////////

	TJsonValue::TJsonValue(const TJsonValue& other)
	{
		type = EType::NULLVALUE;
		(*this) = other;
	}

	TJsonValue::TJsonValue(TJsonValue&& other)
	{
		type = EType::NULLVALUE;
		(*this) = std::move(other);
	}

	TJsonValue::TJsonValue()
	{
		type = EType::NULLVALUE;
	}

	TJsonValue::~TJsonValue()
	{
		Destruct();
	}

	TString TJsonValue::ToString() const
	{
		TString str;
		TListSink<char32_t> sink(&str.chars);
		this->ToStream(sink);
		return str;
	}

	TString JsonUnquote(const TString& input)
	{
		TJsonValue value = TJsonValue::Parse(input);
		EL_ERROR(!value.IsString(), TInvalidArgumentException, "input", "input must contain one JSON string");
		return value.String();
	}

	TString JsonQuote(const TString& input)
	{
		TString output;

		output.chars.Append('\"');
		for(usys_t i = 0; i < input.chars.Count(); i++)
			if(input.chars[i] == '\"' || input.chars[i] == '\\')
			{
				output.chars.Append('\\');
				output.chars.Append(input.chars[i]);
			}
			else if(input.chars[i] == '\n')
			{
				output.chars.Append('\\');
				output.chars.Append('n');
			}
			else if(input.chars[i] == '\t')
			{
				output.chars.Append('\\');
				output.chars.Append('t');
			}
			else if(input.chars[i] < 32)
			{
				output += TString::Format(U"\\u%04x", input.chars[i]);
			}
			else
			{
				output.chars.Append(input.chars[i]);
			}

		output.chars.Append('\"');
		return output;
	}

	// TODO: this function should produce a stream instead of writing to one
	void TJsonValue::ToStream(stream::ISink<char32_t>& sink) const
	{
		TString str;
		bool first = true;

		switch(this->Type())
		{
			case EType::NULLVALUE:
				str = U"null";
				sink.WriteAll(str.chars);
				break;

			case EType::BOOLEAN:
				if(this->Boolean())
					str = U"true";
				else
					str = U"false";
				sink.WriteAll(str.chars);
				break;

			case EType::INTEGER:
				str = TString::Format(U"%d", this->Integer());
				sink.WriteAll(str.chars);
				break;

			case EType::NUMBER:
				str = TString::Format(U"%d", this->Number());
				sink.WriteAll(str.chars);
				break;

			case EType::STRING:
				str = JsonQuote(this->String());
				sink.WriteAll(str.chars);
				break;

			case EType::ARRAY:
				str = U"[";
				sink.WriteAll(str.chars);
				for(const TJsonValue& v : this->Array())
				{
					if(!first)
					{
						str = U",";
						sink.WriteAll(str.chars);
					}
					v.ToStream(sink);
					first = false;
				}
				str = U"]";
				sink.WriteAll(str.chars);
				break;

			case EType::MAP:
				str = U"{";
				sink.WriteAll(str.chars);
				for(const auto& kv : this->Map().Items())
				{
					if(!first)
					{
						str = U",";
						sink.WriteAll(str.chars);
					}

					str = JsonQuote(kv.key);
					sink.WriteAll(str.chars);
					str = U":";
					sink.WriteAll(str.chars);
					kv.value.ToStream(sink);
					first = false;
				}
				str = U"}";
				sink.WriteAll(str.chars);
				break;
		}
	}

	TProducerPipe<char32_t> TJsonValue::Pipe() const
	{
		return TProducerPipe<char32_t>(
			[this](ISink<char32_t>& sink)
			{
				this->ToStream(sink);
			}
		);
	}

	const TJsonValue& TJsonValue::operator()(const TString& key) const
	{
		return (IsMap() && Map().Contains(key)) ? Map()[key] : NULLVALUE;
	}

	////////////////////////////////////////////////////////////////////

	/**************************************************************************/
	// JSON syntax is described by TJsonParser::Parser(). Number conversion uses
	// the generic text scanner so JSON does not duplicate numeric parsing.
	namespace
	{
		[[noreturn]] void ThrowSyntaxError(ITextReader& reader, const usys_t lookahead)
		{
			const text_position_t position = reader.Position(lookahead);
			char32_t chr = U'\0';
			if(reader.Ensure(lookahead + 1))
				chr = reader[lookahead];
			EL_THROW(TInvalidJsonException, position.character_index, position.line_index + 1, chr);
		}
	}

	u16_t TJsonParser::ConvertCodeUnit(const TStringView token)
	{
		TStringViewTextReader reader(token);
		u16_t value = 0;
		reader.Scan(U"%x", value);
		EL_ERROR(reader.Ensure(1), TLogicException);
		return value;
	}

	std::optional<TJsonValue> TJsonParser::ConvertInteger(const TStringView token)
	{
		const auto value = text::scan::ParseNumber<s64_t>(token, 10);
		return value ? std::optional<TJsonValue>(TJsonValue(*value)) : std::nullopt;
	}

	std::optional<TJsonValue> TJsonParser::ConvertNumber(const TStringView token)
	{
		const auto value = text::scan::ParseNumber<double>(token, 10);
		return value ? std::optional<TJsonValue>(TJsonValue(*value)) : std::nullopt;
	}

	std::optional<TJsonValue> TJsonParser::ConvertNumeric(const TStringView token)
	{
		return token.Contains(U'.') || token.Contains(U'e') || token.Contains(U'E') ? ConvertNumber(token) : ConvertInteger(token);
	}

	TJsonValue TJsonValue::Parse(const TString& str, const bool tolerant)
	{
		TStringViewTextReader reader(str.View());
		TJsonValue value = Parse(reader, tolerant);
		if(reader.Ensure(1))
			ThrowSyntaxError(reader, 0);
		return value;
	}

	TJsonValue TJsonValue::Parse(ITextReader& reader, const bool tolerant)
	{
		text::parser::TParseContext context(reader);
		usys_t consumed = 0;
		std::optional<TJsonValue> value;
		try
		{
			value = TJsonParser::Parser(tolerant).TryParse(context, consumed);
		}
		catch(const text::parser::TParseException& error)
		{
			ThrowSyntaxError(reader, error.position);
		}
		if(!value)
			ThrowSyntaxError(reader, context.FarthestPosition());
		reader.Shift(consumed);
		return std::move(*value);
	}

	TJsonValue TJsonValue::Parse(const TFile& file, const bool tolerant)
	{
		TMapping map(const_cast<TFile*>(&file), TAccess::RO);
		auto source = map.Source();
		TStreamTextReader reader(&source);
		return Parse(reader, tolerant);
	}

	const TJsonValue TJsonValue::NULLVALUE = TJsonValue();
	const TJsonValue TJsonValue::TRUE = TJsonValue(true);
	const TJsonValue TJsonValue::FALSE = TJsonValue(false);
	const TJsonValue TJsonValue::ZERO = TJsonValue((s64_t)0);
	const TJsonValue TJsonValue::EMPTY_STRING = TJsonValue(TString());
	const TJsonValue TJsonValue::EMPTY_ARRAY = TJsonValue(TJsonArray());
	const TJsonValue TJsonValue::EMPTY_MAP = TJsonValue(TJsonMap());
}
