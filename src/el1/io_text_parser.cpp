#include "io_text_parser.hpp"

namespace el1::io::text::parser
{
	TString TParseException::Message() const
	{
		return TString::Format(U"committed parser branch failed at character %d", position);
	}

	error::IException* TParseException::Clone() const
	{
		return new TParseException(*this);
	}
}
