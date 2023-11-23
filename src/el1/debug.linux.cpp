#include "def.hpp"
#ifdef EL_OS_LINUX

#include <cxxabi.h>
#include "io_text_string.hpp"
#include "error.hpp"

namespace el1::debug
{
	io::text::string::TString Demangle(const char* name)
	{
		int status = 100;
		char* const cstr = abi::__cxa_demangle(name, nullptr, nullptr, &status);
		try
		{
			EL_ERROR(cstr == nullptr || status != 0, TException, TString::Format("demangling of name %q failed with status %d", name, status));
			io::text::string::TString str = cstr;
			free(cstr);
			return str;
		}
		catch(...)
		{
			free(cstr);
			throw;
		}
	}
}

#endif
