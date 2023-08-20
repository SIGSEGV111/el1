#include "def.hpp"
#ifdef EL_OS_LINUX

#include "system_random.hpp"

namespace el1::system::random
{
	TSystemRandom::TSystemRandom() : file("/dev/urandom")
	{
	}
}

#endif
