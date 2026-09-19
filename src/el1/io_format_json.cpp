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

			case EType::STRING:
				return "string";

			case EType::ARRAY:
				return "array";

			case EType::MAP:
				return "object";
		}

		EL_THROW(TLogicException);
	}

	////////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////////

	void TJsonValue::Destruct() noexcept
	{
		switch(Type())
		{
			case EType::NULLVALUE:
			case EType::BOOLEAN:
			case EType::NUMBER:
				// nothing to do
				break;

			case EType::STRING:
				String().~TString();
				break;

			case EType::ARRAY:
				Array().~TList();
				break;

			case EType::MAP:
				Map().~TJsonObject();
				break;
		}

		type = EStorageType::NULLVALUE;
	}

	////////////////////////////////////////////////////////////////////

	bool TJsonValue::operator==(const TJsonValue& rhs) const
	{
		if(this->Type() == rhs.Type())
		{
			switch(this->Type())
			{
				case EType::NULLVALUE:
					return true;

				case EType::BOOLEAN:
					return this->Boolean() == rhs.Boolean();

				case EType::NUMBER:
				{
					const auto integer_equals_floating = [](const number_t& integer, const ENumberRepresentation integer_representation, const number_t& floating)
					{
						const double value = floating.floating;
						if(!std::isfinite(value) || std::trunc(value) != value)
							return false;

						if(integer_representation == ENumberRepresentation::SIGNED_INTEGER)
						{
							const double limit = std::ldexp(1.0, std::numeric_limits<s64_t>::digits);
							return value >= -limit && value < limit && static_cast<s64_t>(value) == integer.signed_integer;
						}

						const double limit = std::ldexp(1.0, std::numeric_limits<u64_t>::digits);
						return value >= 0.0 && value < limit && static_cast<u64_t>(value) == integer.unsigned_integer;
					};

					if(NumberRepresentation() == ENumberRepresentation::FLOATING && rhs.NumberRepresentation() == ENumberRepresentation::FLOATING)
						return number.floating == rhs.number.floating;

					if(NumberRepresentation() == ENumberRepresentation::FLOATING)
						return integer_equals_floating(rhs.number, rhs.NumberRepresentation(), number);

					if(rhs.NumberRepresentation() == ENumberRepresentation::FLOATING)
						return integer_equals_floating(number, NumberRepresentation(), rhs.number);

					if(NumberRepresentation() == ENumberRepresentation::SIGNED_INTEGER && rhs.NumberRepresentation() == ENumberRepresentation::SIGNED_INTEGER)
						return number.signed_integer == rhs.number.signed_integer;

					if(NumberRepresentation() == ENumberRepresentation::UNSIGNED_INTEGER && rhs.NumberRepresentation() == ENumberRepresentation::UNSIGNED_INTEGER)
						return number.unsigned_integer == rhs.number.unsigned_integer;

					if(NumberRepresentation() == ENumberRepresentation::SIGNED_INTEGER)
						return number.signed_integer >= 0 && static_cast<u64_t>(number.signed_integer) == rhs.number.unsigned_integer;

					return rhs.number.signed_integer >= 0 && number.unsigned_integer == static_cast<u64_t>(rhs.number.signed_integer);
				}

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

	bool TJsonValue::Contains(const TStringView key) const
	{
		return IsObject() && Object().Contains(key);
	}

	TJsonValue& TJsonValue::operator[](const TStringView key)
	{
		TJsonValue* const value = Object().Get(key);
		EL_ERROR(value == nullptr, TKeyNotFoundException<TString>, TString(key));
		return *value;
	}

	const TJsonValue& TJsonValue::operator[](const TStringView key) const
	{
		const TJsonValue* const value = Object().Get(key);
		EL_ERROR(value == nullptr, TKeyNotFoundException<TString>, TString(key));
		return *value;
	}

	TJsonValue& TJsonValue::operator[](const char* const key)
	{
		const TString owned_key(key);
		return Object()[owned_key.View()];
	}

	const TJsonValue& TJsonValue::operator[](const char* const key) const
	{
		const TString owned_key(key);
		return Object()[owned_key.View()];
	}

	TJsonValue& TJsonValue::Add(const TStringView key, TJsonValue value)
	{
		return Object().Add(TString(key), std::move(value));
	}

	TJsonValue& TJsonValue::Set(const TStringView key, TJsonValue value)
	{
		return Object().Set(TString(key), std::move(value));
	}

	bool TJsonValue::Remove(const TStringView key)
	{
		return Object().Remove(key);
	}

	TJsonValue& TJsonValue::Append(TJsonValue value)
	{
		return Array().MoveAppend(std::move(value));
	}

	TJsonValue& TJsonValue::Insert(const ssys_t index, TJsonValue value)
	{
		return Array().MoveInsert(index, std::move(value));
	}

	void TJsonValue::Remove(const ssys_t index)
	{
		Array().Remove(index);
	}

	////////////////////////////////////////////////////////////////////

	bool TJsonValue::IsNull() const
	{
		return type == EStorageType::NULLVALUE;
	}

	void TJsonValue::SetNull()
	{
		*this = TJsonValue();
	}

	////////////////////////////////////////////////////////////////////

	std::optional<bool> TJsonValue::TryBoolean() const noexcept
	{
		return IsBoolean() ? std::optional<bool>(boolean) : std::nullopt;
	}

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

	std::optional<double> TJsonValue::TryNumber() const noexcept
	{
		if(!IsNumber())
			return std::nullopt;

		switch(NumberRepresentation())
		{
			case ENumberRepresentation::SIGNED_INTEGER:
				return static_cast<double>(number.signed_integer);

			case ENumberRepresentation::UNSIGNED_INTEGER:
				return static_cast<double>(number.unsigned_integer);

			case ENumberRepresentation::FLOATING:
				return number.floating;
		}

		return std::nullopt;
	}

	double TJsonValue::ToDouble() const
	{
		return Number();
	}

	double TJsonValue::Number() const
	{
		EL_ERROR(Type() != EType::NUMBER, TException, TString::Format(U"requested number value, but contains %s", JsonTypeToString(Type())));

		switch(NumberRepresentation())
		{
			case ENumberRepresentation::SIGNED_INTEGER:
				return static_cast<double>(number.signed_integer);

			case ENumberRepresentation::UNSIGNED_INTEGER:
				return static_cast<double>(number.unsigned_integer);

			case ENumberRepresentation::FLOATING:
				return number.floating;
		}

		EL_THROW(TLogicException);
	}

	double TJsonValue::Number(const double _default) const
	{
		return IsNumber() ? Number() : _default;
	}

	////////////////////////////////////////////////////////////////////

	std::optional<TStringView> TJsonValue::TryString() const noexcept
	{
		if(!IsString())
			return std::nullopt;
		return TStringView(*reinterpret_cast<const TString*>(__placeholder));
	}

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

		switch(rhs.Type())
		{
			case EType::NULLVALUE:
				break;

			case EType::BOOLEAN:
				this->boolean = rhs.boolean;
				break;

			case EType::NUMBER:
				this->number = rhs.number;
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

		switch(rhs.Type())
		{
			case EType::NULLVALUE:
				break;

			case EType::BOOLEAN:
				this->boolean = rhs.boolean;
				break;

			case EType::NUMBER:
				this->number = rhs.number;
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
		type = EStorageType::BOOLEAN;
		this->boolean = boolean;
	}

	TJsonValue::TJsonValue(const double number)
	{
		type = EStorageType::NUMBER_FLOATING;
		this->number.floating = number;
	}

	TJsonValue::TJsonValue(const TString& string)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TString(string);
		type = EStorageType::STRING;
	}

	TJsonValue::TJsonValue(TString&& string)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TString(std::move(string));
		type = EStorageType::STRING;
	}

	TJsonValue::TJsonValue(const TStringView string)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TString(string);
		type = EStorageType::STRING;
	}

	TJsonValue::TJsonValue(const char* const string)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TString(string);
		type = EStorageType::STRING;
	}

	TJsonValue::TJsonValue(const char32_t* const string)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TString(string);
		type = EStorageType::STRING;
	}

	TJsonValue::TJsonValue(const TJsonArray& array)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TJsonArray(array);
		type = EStorageType::ARRAY;
	}

	TJsonValue::TJsonValue(const array_t<const TJsonValue> array)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TJsonArray(array);
		type = EStorageType::ARRAY;
	}

	TJsonValue::TJsonValue(TJsonArray&& array)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TJsonArray(std::move(array));
		type = EStorageType::ARRAY;
	}

	TJsonValue::TJsonValue(const TConstJsonMap& map)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TJsonMap(map);
		type = EStorageType::MAP;
	}

	TJsonValue::TJsonValue(TJsonMap&& map)
	{
		type = EStorageType::NULLVALUE;
		new (__placeholder) TJsonMap(std::move(map));
		type = EStorageType::MAP;
	}

	////////////////////////////////////////////////////////////////////

	TJsonValue::TJsonValue(const TJsonValue& other)
	{
		type = EStorageType::NULLVALUE;
		(*this) = other;
	}

	TJsonValue::TJsonValue(TJsonValue&& other)
	{
		type = EStorageType::NULLVALUE;
		(*this) = std::move(other);
	}

	TJsonValue::TJsonValue()
	{
		type = EStorageType::NULLVALUE;
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

	TString JsonUnquote(const TStringView input)
	{
		TJsonValue value = TJsonValue::Parse(input);
		EL_ERROR(!value.IsString(), TInvalidArgumentException, "input", "input must contain one JSON string");
		return value.String();
	}

	TString JsonQuote(const TStringView input)
	{
		TString output;

		output.chars.Append('\"');
		for(usys_t i = 0; i < input.Length(); i++)
			if(input[i] == '\"' || input[i] == '\\')
			{
				output.chars.Append('\\');
				output.chars.Append(input[i]);
			}
			else if(input[i] == '\n')
			{
				output.chars.Append('\\');
				output.chars.Append('n');
			}
			else if(input[i] == '\t')
			{
				output.chars.Append('\\');
				output.chars.Append('t');
			}
			else if(input[i] < 32)
			{
				output += TString::Format(U"\\u%04x", input[i]);
			}
			else
			{
				output.chars.Append(input[i]);
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

			case EType::NUMBER:
				switch(NumberRepresentation())
				{
					case ENumberRepresentation::SIGNED_INTEGER:
						str = TString::Format(U"%d", number.signed_integer);
						break;

					case ENumberRepresentation::UNSIGNED_INTEGER:
						str = TString::Format(U"%u", number.unsigned_integer);
						break;

					case ENumberRepresentation::FLOATING:
						str = TString::Format(U"%d", number.floating);
						break;
				}
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

	TJsonValueProxy TJsonValue::operator()(const TStringView key)
	{
		return TJsonValueProxy(*this, key);
	}

	const TJsonValue& TJsonValue::operator()(const TStringView key) const
	{
		if(!IsObject())
			return NULLVALUE;

		const TJsonValue* const value = Object().Get(key);
		return value != nullptr ? *value : NULLVALUE;
	}

	////////////////////////////////////////////////////////////////////

	TJsonValueProxy::TJsonValueProxy(TJsonValue& base, const TStringView key) : root(&base)
	{
		path.Append(TString(key));
	}

	const TJsonValue& TJsonValueProxy::Resolve() const
	{
		const TJsonValue* current = root;
		for(const TString& key : path)
		{
			if(current == nullptr || !current->IsObject())
				return TJsonValue::NULLVALUE;

			current = current->Object().Get(key.View());
			if(current == nullptr)
				return TJsonValue::NULLVALUE;
		}

		return current != nullptr ? *current : TJsonValue::NULLVALUE;
	}

	TJsonValue& TJsonValueProxy::Materialize()
	{
		EL_ERROR(root == nullptr, TLogicException);
		TJsonValue* current = root;

		for(const TString& key : path)
		{
			if(current->IsNull())
				*current = TJsonObject();

			EL_ERROR(!current->IsObject(), TException, TString::Format(U"cannot create JSON member %q below %s", key, JsonTypeToString(current->Type())));

			TJsonValue* child = current->Object().Get(key.View());
			if(child == nullptr)
				child = &current->Object().Add(TString(key), TJsonValue());

			current = child;
		}

		return *current;
	}

	TJsonValue& TJsonValueProxy::MaterializeAs(const EType type)
	{
		TJsonValue& value = Materialize();
		if(value.IsNull())
		{
			switch(type)
			{
				case EType::ARRAY:
					value = TJsonArray();
					break;

				case EType::OBJECT:
					value = TJsonObject();
					break;

				default:
					EL_THROW(TLogicException);
			}
		}

		EL_ERROR(value.Type() != type, TException, TString::Format(U"requested %s mutation, but JSON path contains %s", JsonTypeToString(type), JsonTypeToString(value.Type())));
		return value;
	}

	TJsonValueProxy TJsonValueProxy::operator()(const TStringView key) const &
	{
		TJsonValueProxy result(*this);
		result.path.Append(TString(key));
		return result;
	}

	TJsonValueProxy TJsonValueProxy::operator()(const TStringView key) &&
	{
		path.Append(TString(key));
		return std::move(*this);
	}

	TJsonValueProxy& TJsonValueProxy::operator=(TJsonValue value)
	{
		Materialize() = std::move(value);
		return *this;
	}

	TJsonValue& TJsonValueProxy::Add(const TStringView key, TJsonValue value)
	{
		return MaterializeAs(EType::OBJECT).Add(key, std::move(value));
	}

	TJsonValue& TJsonValueProxy::Set(const TStringView key, TJsonValue value)
	{
		return MaterializeAs(EType::OBJECT).Set(key, std::move(value));
	}

	bool TJsonValueProxy::Remove(const TStringView key)
	{
		const TJsonValue& value = Resolve();
		if(value.IsNull())
			return false;
		EL_ERROR(!value.IsObject(), TException, TString::Format(U"requested object mutation, but JSON path contains %s", JsonTypeToString(value.Type())));
		return const_cast<TJsonValue&>(value).Remove(key);
	}

	TJsonValue& TJsonValueProxy::Append(TJsonValue value)
	{
		return MaterializeAs(EType::ARRAY).Append(std::move(value));
	}

	TJsonValue& TJsonValueProxy::Insert(const ssys_t index, TJsonValue value)
	{
		return MaterializeAs(EType::ARRAY).Insert(index, std::move(value));
	}

	void TJsonValueProxy::Remove(const ssys_t index)
	{
		const TJsonValue& value = Resolve();
		EL_ERROR(value.IsNull(), TException, U"cannot remove an array element from a missing JSON path");
		EL_ERROR(!value.IsArray(), TException, TString::Format(U"requested array mutation, but JSON path contains %s", JsonTypeToString(value.Type())));
		const_cast<TJsonValue&>(value).Remove(index);
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
		const auto value = text::number::TryParseHex<u16_t>(token);
		EL_ERROR(!value.has_value(), TLogicException);
		return *value;
	}

	std::optional<TJsonValue> TJsonParser::ConvertNumber(const TStringView token)
	{
		const auto value = text::number::TryParseNumber<double>(token, 10, true);
		return value ? std::optional<TJsonValue>(TJsonValue(*value)) : std::nullopt;
	}

	std::optional<TJsonValue> TJsonParser::ConvertNumeric(const TStringView token)
	{
		if(!token.Contains(U'.') && !token.Contains(U'e') && !token.Contains(U'E'))
		{
			if(token[0] == U'-')
			{
				if(const auto value = text::number::TryParseInteger<s64_t>(token))
					return TJsonValue(*value);
			}
			else if(const auto value = text::number::TryParseInteger<u64_t>(token))
			{
				return TJsonValue(*value);
			}
		}

		return ConvertNumber(token);
	}

	TJsonValue TJsonValue::Parse(const TStringView str, const bool tolerant)
	{
		TStringViewTextReader reader(str);
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
	const TJsonValue TJsonValue::ZERO = TJsonValue(0);
	const TJsonValue TJsonValue::EMPTY_STRING = TJsonValue(TString());
	const TJsonValue TJsonValue::EMPTY_ARRAY = TJsonValue(TJsonArray());
	const TJsonValue TJsonValue::EMPTY_MAP = TJsonValue(TJsonMap());

	TJsonValue TJsonValue::Object(TJsonMap members)
	{
		return TJsonValue(std::move(members));
	}

	TJsonValue TJsonValue::Array(TJsonArray values)
	{
		return TJsonValue(std::move(values));
	}
}
