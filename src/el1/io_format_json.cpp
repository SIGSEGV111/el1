#include "io_format_json.hpp"
#include "io_text_encoding_utf8.hpp"
#include "io_file.hpp"

namespace el1::io::format::json
{
	using namespace text::encoding::utf8;
	using namespace stream::producer;
	using namespace file;

	// LCOV_EXCL_START
	TString TInvalidJsonException::Message() const
	{
		switch(reason)
		{
			case EReason::UNEXPECTED_EOF:
				return TString::Format("unexpected EOF in line %d", line);
				break;

			case EReason::SYNTAX_ERROR:
				return TString::Format("syntax error in line %d ('%c')", line, chr);
				break;

			case EReason::NUMBER_TOO_BIG:
				return TString::Format("number too big in line %d ('%c')", line, chr);
				break;

			case EReason::UNESCAPED_CTRL:
				return TString::Format("unescaped control character in string encountered in line %d (0x%02x)", line, chr);
				break;

			case EReason::INVALID_ESCAPE:
				return TString::Format("incorrect escape of character %q in line %d", chr, line);
				break;
		}

		EL_THROW(TLogicException);
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
		EL_ERROR(Type() != EType::BOOLEAN, TException, TString::Format("requested boolean value, but contains %s", JsonTypeToString(Type())));
		return boolean;
	}

	const bool& TJsonValue::Boolean() const
	{
		return const_cast<TJsonValue*>(this)->Boolean();
	}

	////////////////////////////////////////////////////////////////////

	double& TJsonValue::Number()
	{
		EL_ERROR(Type() != EType::NUMBER, TException, TString::Format("requested number value, but contains %s", JsonTypeToString(Type())));
		return *reinterpret_cast<double*>(__placeholder);
	}

	const double& TJsonValue::Number() const
	{
		return const_cast<TJsonValue*>(this)->Number();
	}

	////////////////////////////////////////////////////////////////////

	TString& TJsonValue::String()
	{
		EL_ERROR(Type() != EType::STRING, TException, TString::Format("requested string value, but contains %s", JsonTypeToString(Type())));
		return *reinterpret_cast<TString*>(__placeholder);
	}

	const TString& TJsonValue::String() const
	{
		return const_cast<TJsonValue*>(this)->String();
	}

	////////////////////////////////////////////////////////////////////

	TJsonArray& TJsonValue::Array()
	{
		EL_ERROR(Type() != EType::ARRAY, TException, TString::Format("requested array value, but contains %s", JsonTypeToString(Type())));
		return *reinterpret_cast<TJsonArray*>(__placeholder);
	}

	const array_t<const TJsonValue>& TJsonValue::Array() const
	{
		return const_cast<TJsonValue*>(this)->Array();
	}

	////////////////////////////////////////////////////////////////////

	TJsonMap& TJsonValue::Map()
	{
		EL_ERROR(Type() != EType::MAP, TException, TString::Format("requested map value, but contains %s", JsonTypeToString(Type())));
		return *reinterpret_cast<TJsonMap*>(__placeholder);
	}

	const TConstJsonMap& TJsonValue::Map() const
	{
		return const_cast<TJsonValue*>(this)->Map();
	}

	////////////////////////////////////////////////////////////////////

	TJsonValue& TJsonValue::operator=(const TJsonValue& rhs)
	{
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
		TListSink<TUTF32> sink(&str.chars);
		this->ToStream(sink);
		return str;
	}

	TString JsonUnquote(const TString& input)
	{
		TString output;

		EL_ERROR(input.Length() < 2, TInvalidArgumentException, "input", "input must be at least of length 2");
		EL_ERROR(input.chars[ 0] != '\"', TInvalidArgumentException, "input", "input must begin with a quote");
		EL_ERROR(input.chars[-1] != '\"', TInvalidArgumentException, "input", "input must end with a quote");
		EL_ERROR(input.chars[-2] == '\\', TInvalidArgumentException, "input", "input the final quotation mark cannot be escaped");

		for(usys_t i = 1; i < input.chars.Count() - 1; i++)
		{
			if(input.chars[i].code == '\\')
			{
				i++;
				switch(input.chars[i].code)
				{
					case 'b':
						output.chars.Append('\b');
						break;
					case 'f':
						output.chars.Append('\f');
						break;
					case 'n':
						output.chars.Append('\n');
						break;
					case 'r':
						output.chars.Append('\r');
						break;
					case 't':
						output.chars.Append('\t');
						break;
					case '\"':
						output.chars.Append('\"');
						break;
					case '\\':
						output.chars.Append('\\');
						break;

					case 'u':
					{
						EL_NOT_IMPLEMENTED;
					}

					default:
						EL_THROW(TException, TString::Format("invalid escape of character %q encountered in input", input.chars[i]));
				}
			}
			else
			{
				output.chars.Append(input.chars[i]);
			}
		}

		return output;
	}

	TString JsonQuote(const TString& input)
	{
		TString output;

		output.chars.Append('\"');
		for(usys_t i = 0; i < input.chars.Count(); i++)
			if(input.chars[i].code == '\"' || input.chars[i].code == '\\')
			{
				output.chars.Append('\\');
				output.chars.Append(input.chars[i]);
			}
			else if(input.chars[i].code == '\n')
			{
				output.chars.Append('\\');
				output.chars.Append('n');
			}
			else if(input.chars[i].code == '\t')
			{
				output.chars.Append('\\');
				output.chars.Append('t');
			}
			else if(input.chars[i].code < 32)
			{
				output += TString::Format("\\u%04x", input.chars[i].code);
			}
			else
			{
				output.chars.Append(input.chars[i]);
			}

		output.chars.Append('\"');
		return output;
	}

	// TODO: this function is ugly - implement ITextWriter and use this instead
	// also this function should produce a stream NOT write to one
	void TJsonValue::ToStream(stream::ISink<TUTF32>& sink) const
	{
		TString str;
		bool first = true;

		switch(this->Type())
		{
			case EType::NULLVALUE:
				str = L"null";
				sink.WriteAll(str.chars);
				break;

			case EType::BOOLEAN:
				if(this->Boolean())
					str = L"true";
				else
					str = L"false";
				sink.WriteAll(str.chars);
				break;

			case EType::NUMBER:
			{
				const s64_t i = (s64_t)this->Number();
				if((double)i == this->Number())
				{
					// integer
					str = TString::Format("%d", i);
				}
				else
				{
					// float
					str = TString::Format("%d", this->Number());
				}
				sink.WriteAll(str.chars);
				break;
			}

			case EType::STRING:
				str = JsonQuote(this->String());
				sink.WriteAll(str.chars);
				break;

			case EType::ARRAY:
				str = L"[";
				sink.WriteAll(str.chars);
				for(const TJsonValue& v : this->Array())
				{
					if(!first)
					{
						str = L",";
						sink.WriteAll(str.chars);
					}
					v.ToStream(sink);
					first = false;
				}
				str = L"]";
				sink.WriteAll(str.chars);
				break;

			case EType::MAP:
				str = L"{";
				sink.WriteAll(str.chars);
				for(const auto& kv : this->Map().Items())
				{
					if(!first)
					{
						str = L",";
						sink.WriteAll(str.chars);
					}

					str = JsonQuote(kv.key);
					sink.WriteAll(str.chars);
					str = L":";
					sink.WriteAll(str.chars);
					kv.value.ToStream(sink);
					first = false;
				}
				str = L"}";
				sink.WriteAll(str.chars);
				break;
		}
	}

	TProducerPipe<TUTF32> TJsonValue::Pipe() const
	{
		return TProducerPipe<TUTF32>(
			[this](ISink<TUTF32>& sink)
			{
				this->ToStream(sink);
			}
		);
	}

	////////////////////////////////////////////////////////////////////

	TJsonValue TJsonValue::Parse(const TString& str)
	{
		return str.chars.Pipe().Transform(TJsonParser()).First();
	}

	TJsonValue TJsonValue::Parse(const TFile& file)
	{
		TMapping map(const_cast<TFile*>(&file), TAccess::RO);
		return map.Pipe().Transform(TUTF8Decoder()).Transform(TJsonParser()).First();
	}

	const TJsonValue TJsonValue::NULLVALUE = TJsonValue();
	const TJsonValue TJsonValue::TRUE = TJsonValue(true);
	const TJsonValue TJsonValue::FALSE = TJsonValue(false);
	const TJsonValue TJsonValue::ZERO = TJsonValue(0.0);
	const TJsonValue TJsonValue::EMPTY_STRING = TJsonValue(L"");
	const TJsonValue TJsonValue::EMPTY_ARRAY = TJsonValue(TJsonArray());
	const TJsonValue TJsonValue::EMPTY_MAP = TJsonValue(TJsonMap());
}
