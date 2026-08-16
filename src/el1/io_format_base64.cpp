#include "io_format_base64.hpp"

namespace el1::io::format::base64
{
	using namespace collection::list;

	TString EncodeBase64Url(const array_t<const byte_t> data)
	{
		TString result = data.Pipe().Transform(TBase64Encoder()).Collect();
		result.Replace(TStringView(U"+"), TStringView(U"-"));
		result.Replace(TStringView(U"/"), TStringView(U"_"));
		while(result.EndsWith(TStringView(U"=")))
			result.Truncate(result.Length() - 1);
		return result;
	}

	TList<byte_t> DecodeBase64Url(TString text)
	{
		EL_ERROR((text.Length() % 4U) == 1U, error::TInvalidArgumentException, "text", "invalid base64url length");
		text.Replace(TStringView(U"-"), TStringView(U"+"));
		text.Replace(TStringView(U"_"), TStringView(U"/"));
		while((text.Length() % 4U) != 0U)
			text += TStringView(U"=");
		return text.chars.Pipe().Transform(TBase64Decoder()).Collect();
	}
}
