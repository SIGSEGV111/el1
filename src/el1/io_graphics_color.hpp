#pragma once
#include "io_types.hpp"
#include "math_vector.hpp"

namespace el1::io::graphics::color
{
	using namespace io::types;

	template<typename T>
	using rgb_t = math::vector::vector_t<T, 3>;

	template<typename T>
	using rgba_t = math::vector::vector_t<T, 4>;

	using rgb8_t  = rgb_t<u8_t>;
	using rgb16_t = rgb_t<u16_t>;
	using rgbf_t  = rgb_t<float>;
}
