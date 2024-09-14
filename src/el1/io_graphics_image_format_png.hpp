#pragma once
#include "io_graphics_image.hpp"

namespace el1::io::graphics::image::format::png
{
	using namespace io::types;
	using namespace io::stream;
	using namespace io::graphics::image;

	TRasterImage LoadPNG(IBinarySource& stream);
}
