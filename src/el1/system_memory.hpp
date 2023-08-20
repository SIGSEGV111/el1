#pragma once

#include "def.hpp"
#include "io_types.hpp"

namespace el1::system::memory
{
	using namespace io::types;

	struct IAllocator
	{
		virtual void* Alloc(const usys_t sz_bytes_min) = 0;
		virtual void* Realloc(void* const p_mem, const usys_t sz_bytes_new, const bool allow_move) = 0;
		virtual usys_t UseableSize(void* const p_mem) = 0;
		virtual void Free(void* const p_mem) = 0;
	};

	// returns the actual size in bytes that was allocated for a specific memory block - this is usually a bit more than what was requested (due to alignment and anti-fragementation techniques). Return 0 if a nullptr is passed as argument.
	usys_t UseableSize(void* const p_mem);

	void* VirtualAlloc(const usys_t sz_bytes, const bool readable, const bool writeable, const bool executable, const bool shared);
	void VirtualAllocAt(void* const p_mem, const usys_t sz_bytes, const bool readable, const bool writeable, const bool executable, const bool shared, const bool overwrite);
	void* VirtualRealloc(void* const p_mem, const usys_t sz_bytes_old, const usys_t sz_bytes_new, const bool readable, const bool writeable, const bool executable, const bool shared, const bool allow_move);
	void VirtualFree(void* p_mem, const usys_t sz_bytes);
}
