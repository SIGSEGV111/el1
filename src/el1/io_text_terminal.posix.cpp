#include "def.hpp"
#ifdef EL_OS_CLASS_POSIX

#include "io_text_terminal.hpp"

namespace el1::io::text::terminal
{
	stream::TKernelStream stdin (system::handle::THandle(0, true));
	stream::TKernelStream stdout(system::handle::THandle(1, true));
	stream::TKernelStream stderr(system::handle::THandle(2, true));
}

#endif
