#include "def.hpp"

#ifdef EL_OS_LINUX
#if defined(__riscv) && __riscv_xlen == 64
#include "system_task.hpp"

namespace el1::system::task
{
	void SwapRegisters(context_registers_t* const current, const context_registers_t* const target) noexcept
	{
		asm volatile(R"(
			# a0 is the first function argument => pointer to destination register buffer
			# a1 is the second function argument => pointer to source register buffer

			# Save callee-saved registers to the destination buffer
			sd  x8,  0(a0)
			sd  x9,  8(a0)
			sd x18, 16(a0)
			sd x19, 24(a0)
			sd x20, 32(a0)
			sd x21, 40(a0)
			sd x22, 48(a0)
			sd x23, 56(a0)
			sd x24, 64(a0)
			sd x25, 72(a0)
			sd x26, 80(a0)
			sd x27, 88(a0)
			sd sp,  96(a0)
			sd ra, 104(a0)

			# Load registers from the source buffer
			ld  x8,  0(a1)
			ld  x9,  8(a1)
			ld x18, 16(a1)
			ld x19, 24(a1)
			ld x20, 32(a1)
			ld x21, 40(a1)
			ld x22, 48(a1)
			ld x23, 56(a1)
			ld x24, 64(a1)
			ld x25, 72(a1)
			ld x26, 80(a1)
			ld x27, 88(a1)
			ld sp,  96(a1)
			ld ra, 104(a1)

			ret
		)");
	}

	void TFiber::InitRegisters()
	{
		memset(&this->registers, 0, sizeof(this->registers));
		this->registers.sp = (u64_t)this->p_stack + this->sz_stack - 8; // -8 for 16-byte alignment (not exactly sure WHY though)
		this->registers.pc = (u64_t)&TFiber::Boot;
		*(u64_t**)this->registers.sp = nullptr;

		// FIXME: set first argument to "this"
	}
}

#endif
#endif
