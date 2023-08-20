#include "io_stream.hpp"
#include "io_text_string.hpp"

namespace el1::io::stream
{
	using namespace io::text::string;
	using namespace error;

	TString TStreamDryException::Message() const
	{
		return L"source stream ran out of elements";
	}

	IException* TStreamDryException::Clone() const
	{
		return new TStreamDryException(*this);
	}

	TString TSinkFloodedException::Message() const
	{
		return L"sink stream refused to accept further elements";
	}

	IException* TSinkFloodedException::Clone() const
	{
		return new TSinkFloodedException(*this);
	}
}
