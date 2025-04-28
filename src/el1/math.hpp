#pragma once

#include "def.hpp"
#include "io_types.hpp"

namespace el1::math
{
	enum class ERoundingMode : io::types::u8_t
	{
		TO_NEAREST,		// Round to nearest; ties round away from zero
		TO_NEAREST_EVEN,// Round to nearest; ties round to even (IEEE 754 default)
		TOWARDS_ZERO,	// Truncate toward zero
		AWAY_FROM_ZERO,	// Round away from zero
		DOWNWARD,		// Round toward negative infinity
		UPWARD,			// Round toward positive infinity
		STOCHASTIC		// Randomized rounding based on distance to adjacent values
	};
}
