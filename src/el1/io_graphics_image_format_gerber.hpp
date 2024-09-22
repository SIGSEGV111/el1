#pragma once
#include "io_types.hpp"
#include "io_text.hpp"
#include "io_graphics_image.hpp"

namespace el1::io::graphics::format::gerber
{
	using namespace io::stream;
	using namespace io::types;
	using namespace io::text;
	using namespace io::collection::list;
	using namespace io::graphics::image;

	TVectorImage LoadGerber(IBinarySource& src);
}
