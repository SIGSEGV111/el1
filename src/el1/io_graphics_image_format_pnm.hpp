#pragma once
#include "io_graphics_image.hpp"
#include "io_stream.hpp"

namespace el1::io::graphics::image::format::pnm
{
	using namespace io::graphics::image;

	void SaveP6(const TRasterImage& img, stream::IBinarySink& stream, const u16_t max_value = 255);
	TRasterImage LoadP6(stream::IBinarySource& stream);
}
