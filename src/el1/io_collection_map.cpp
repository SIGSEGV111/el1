#include "io_collection_map.hpp"
#include "io_text_string.hpp"

namespace el1::io::collection::map
{
	using namespace io::text::string;

	TString TGenericKeyNotFoundException::Message() const
	{
		return "the requested key was not found";
	}

	TString TGenericKeyAlreadyExistsException::Message() const
	{
		return "the key associated with the new item already exists in the map";
	}
}
