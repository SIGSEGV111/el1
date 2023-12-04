#pragma once
#include "io_types.hpp"

namespace el1::io::graphics::color
{
	using namespace io::types;

	template<typename TCC>
	struct rgb_t
	{
		TCC red, green, blue;
	};

	using rgb8_t = rgb_t<u8_t>;
	using rgbf_t = rgb_t<float>;
}
