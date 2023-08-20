#include "def.hpp"
#ifdef EL_OS_CLASS_POSIX

#include "error.hpp"
#include "io_text_string.hpp"
#include <string.h>
#include <errno.h>

namespace el1::error
{
	using namespace io::types;
	using namespace io::text::string;

	TString TSyscallException::Message() const
	{
		return TString::Format("syscall failed with error-code %d (%s)", error_code, strerror(error_code));
	}

	IException* TSyscallException::Clone() const
	{
		return new TSyscallException(*this);
	}

	TSyscallException::TSyscallException() : error_code(errno) {}

	/***************************************************/

	TString TPthreadException::Message() const
	{
		return TString::Format("pthread function call failed with error-code %d (%s)", error_code, strerror(error_code));
	}

	IException* TPthreadException::Clone() const
	{
		return new TPthreadException(*this);
	}
}

#endif
