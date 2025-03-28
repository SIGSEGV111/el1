#include "def.hpp"

#ifdef EL_OS_LINUX
#if defined(__aarch64__)

#include "system_task.hpp"

namespace el1::system::task
{
	// https://hev.cc/3052.html
	// https://developer.arm.com/documentation/102374/0100/Procedure-Call-Standard
	void SwapRegisters(context_registers_t* const current, const context_registers_t* const target) noexcept
	{
		asm volatile(R"(
			/* store registers to array at x0 (current) */
			str x19, [x0,  0]
			str x20, [x0,  8]
			str x21, [x0, 16]
			str x22, [x0, 24]
			str x23, [x0, 32]
			str x24, [x0, 40]
			str x25, [x0, 48]
			str x26, [x0, 56]
			str x27, [x0, 64]
			str x28, [x0, 72]
			str x29, [x0, 80]
			str x30, [x0, 88]

			/* store SP */
			mov x9, sp
			str x9, [x0, 96]

			/* load registers from array at x1 (target) */
			ldr x19, [x1,  0]
			ldr x20, [x1,  8]
			ldr x21, [x1, 16]
			ldr x22, [x1, 24]
			ldr x23, [x1, 32]
			ldr x24, [x1, 40]
			ldr x25, [x1, 48]
			ldr x26, [x1, 56]
			ldr x27, [x1, 64]
			ldr x28, [x1, 72]
			ldr x29, [x1, 80]
			ldr x30, [x1, 88]

			/* load SP */
			ldr x9, [x1, 96]

			/* move x9 into SP */
			add	sp, x9, 0

			ret
		)");
	}

	void TFiber::InitRegisters()
	{
		memset(&this->registers, 0, sizeof(this->registers));
		this->registers.sp = (u64_t)this->p_stack + this->sz_stack;
		this->registers.lr = (u64_t)&TFiber::Boot;

		// FIXME: set first argument to "this"
	}
}

#endif
#endif
