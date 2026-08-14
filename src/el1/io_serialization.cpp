#include "io_serialization.hpp"

namespace el1::io::serialization
{
	TString TTypeId::ToString() const
	{
		return TString::Format(U"%016x%016x", high, low);
	}

	TTypeId TTypeId::FromString(const TStringView text)
	{
		EL_ERROR(text.Length() != 32, TInvalidArgumentException, "text", "type id must contain exactly 32 hexadecimal digits");
		auto parse = [&](const usys_t begin)
		{
			u64_t value = 0;
			for(usys_t i = begin; i < begin + 16; i++)
			{
				const char32_t chr = text[i];
				u8_t digit;
				if(chr >= U'0' && chr <= U'9') digit = (u8_t)(chr - U'0');
				else if(chr >= U'a' && chr <= U'f') digit = (u8_t)(10 + chr - U'a');
				else if(chr >= U'A' && chr <= U'F') digit = (u8_t)(10 + chr - U'A');
				else EL_THROW(TInvalidArgumentException, "text", "type id contains a non-hexadecimal digit");
				value = (value << 4) | digit;
			}
			return value;
		};
		return {parse(0), parse(16)};
	}
}
