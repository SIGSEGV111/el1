#pragma once

#include "def.hpp"
#include "io_types.hpp"

namespace el1::math
{
	enum class ERoundingMode : io::types::u8_t
	{
		TO_NEAREST,      // rounds up when the digit is >=(base/2) and down otherwise
		TOWARDS_ZERO,    // rounds up when the number is negative and down when the number is positive
		AWAY_FROM_ZERO,  // the opposite of TOWARDS_ZERO
		DOWNWARD,        // always rounds down (negative numbers round away from zero, positive numbers round towards zero)
		UPWARD           // always rounds up (negative numbers round towards zero, positive numbers away from zero)
	};
}

