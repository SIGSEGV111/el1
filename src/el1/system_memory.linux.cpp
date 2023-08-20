#include "def.hpp"
#ifdef EL_OS_LINUX

#include "system_memory.hpp"
#include "error.hpp"

#include <malloc.h>
#include <sys/mman.h>
#include <unistd.h>

namespace el1::system::memory
{
	usys_t UseableSize(void* const p_mem)
	{
		return malloc_usable_size(p_mem);
	}

	void VirtualAllocAt(void* const p_mem, const usys_t sz_bytes, const bool readable, const bool writeable, const bool executable, const bool shared, const bool overwrite)
	{
		EL_SYSERR(mmap(p_mem, sz_bytes, (readable || writeable || executable) ? ((readable ? PROT_READ : 0) | (writeable ? PROT_WRITE : 0) | (executable ? PROT_EXEC : 0)) : PROT_NONE, MAP_ANONYMOUS | (shared ? MAP_SHARED : MAP_PRIVATE) | (executable ? MAP_EXECUTABLE : 0) | MAP_FIXED | (overwrite ? 0 : MAP_FIXED_NOREPLACE) | ((readable || writeable || executable) ? 0 : MAP_NORESERVE), -1, 0));
	}

	void* VirtualAlloc(const usys_t sz_bytes, const bool readable, const bool writeable, const bool executable, const bool shared)
	{
		return EL_SYSERR(mmap(nullptr, sz_bytes, (readable || writeable || executable) ? ((readable ? PROT_READ : 0) | (writeable ? PROT_WRITE : 0) | (executable ? PROT_EXEC : 0)) : PROT_NONE, MAP_ANONYMOUS | (shared ? MAP_SHARED : MAP_PRIVATE) | (executable ? MAP_EXECUTABLE : 0) | ((readable || writeable || executable) ? 0 : MAP_NORESERVE), -1, 0));
	}

	void* VirtualRealloc(void* const p_mem, const usys_t sz_bytes_old, const usys_t sz_bytes_new, const bool readable, const bool writeable, const bool executable, const bool shared, const bool allow_move)
	{
		return EL_SYSERR(mremap(p_mem, sz_bytes_old, sz_bytes_new, (allow_move ? MREMAP_MAYMOVE : MREMAP_FIXED)));
	}

	void VirtualFree(void* const p_mem, const usys_t sz_bytes)
	{
		EL_SYSERR(munmap(p_mem, sz_bytes));
	}
}

#endif
