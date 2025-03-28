#include "def.hpp"

#ifdef EL_OS_LINUX
#if defined(__arm__) && !defined(__aarch64__)

#include "system_task.hpp"

namespace el1::system::task
{
	// https://developer.arm.com/documentation/den0013/d/Introduction-to-Assembly-Language/Introduction-to-the-GNU-Assembler/GNU-tools-naming-conventions
	// https://developer.arm.com/documentation/den0013/d/Application-Binary-Interfaces/Procedure-Call-Standard?lang=en
	// https://developer.arm.com/documentation/den0013/d/Instruction-Summary/Instruction-Summary/ADC?lang=en
	// https://sourceforge.net/p/predef/wiki/Architectures/
	void SwapRegisters(context_registers_t* const current, const context_registers_t* const target) noexcept
	{
		asm volatile(R"(
			.arm
			.syntax unified
			.align  2

			/* R0: current (destination buffer) */
			/* R1: target (source buffer) */

			/* save SP */
			mov r2, r13

			/* Save the return address */
			mov r3, lr

			/* store registers to array at r0 */
			stm r0!, {r4, r5, r6, r7, r8, r9, r10, r11}
			stm r0!, {r2, r3}

			/* load registers from array at r1 */
			ldm r1!, {r4, r5, r6, r7, r8, r9, r10, r11, r13}
			ldm r1!, {r3}

			mov lr, #0

			/* return to address in r3 */
			bx r3
		)");
	}

	void TFiber::InitRegisters()
	{
		memset(&this->registers, 0, sizeof(this->registers));
		this->registers.r13 = (u32_t)this->p_stack + this->sz_stack;
		this->registers.r15 = (u32_t)&TFiber::Boot;

		// FIXME: set first argument to "this"
	}
}

#endif
#endif
