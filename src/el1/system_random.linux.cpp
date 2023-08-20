#include "def.hpp"
#ifdef EL_OS_LINUX

#include "system_random.hpp"
#include "io_file.hpp"

namespace el1::system::random
{
	TSystemRandom::TSystemRandom() : stream(io::file::TPath("/dev/urandom"))
	{
	}
}

#endif
