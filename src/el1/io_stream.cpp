#include "io_stream.hpp"
#include "io_text_string.hpp"

namespace el1::io::stream
{
	using namespace io::text::string;
	using namespace error;

	TString TStreamDryException::Message() const
	{
		return U"source stream ran out of elements";
	}

	IException* TStreamDryException::Clone() const
	{
		return new TStreamDryException(*this);
	}

	TString TSinkFloodedException::Message() const
	{
		return U"sink stream refused to accept further elements";
	}

	IException* TSinkFloodedException::Clone() const
	{
		return new TSinkFloodedException(*this);
	}

	TString TLimitExceededException::Message() const
	{
		return U"stream sink exceeded configured item limit";
	}

	IException* TLimitExceededException::Clone() const
	{
		return new TLimitExceededException(*this);
	}
}
