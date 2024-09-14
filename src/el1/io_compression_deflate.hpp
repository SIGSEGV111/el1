#pragma once
#include "io_collection_list.hpp"

namespace el1::io::compression::deflate
{
	using namespace io::collection::list;
	TList<byte_t> Inflate(array_t<const byte_t> input);
}
