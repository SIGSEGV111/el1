#include "io_serialization.hpp"
#include "io_text_number.hpp"

namespace el1::io::serialization
{
	TString TTypeId::ToString() const
	{
		return TString::Format(U"%016x%016x", high, low);
	}

	TTypeId TTypeId::FromString(const TStringView text)
	{
		EL_ERROR(text.Length() != 32, TInvalidArgumentException, "text", "type id must contain exactly 32 hexadecimal digits");
		const auto high = text::number::TryParseHex<u64_t>(text.SliceSL(0, 16));
		const auto low = text::number::TryParseHex<u64_t>(text.SliceSL(16, 16));
		EL_ERROR(!high.has_value() || !low.has_value(), TInvalidArgumentException, "text", "type id contains a non-hexadecimal digit");
		return {*high, *low};
	}
}
