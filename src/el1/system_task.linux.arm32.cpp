#include "def.hpp"

#ifdef EL_OS_LINUX
#if defined(__arm__) && !defined(__aarch64__)

#include "system_task.hpp"

// https://developer.arm.com/documentation/den0013/d/Introduction-to-Assembly-Language/Introduction-to-the-GNU-Assembler/GNU-tools-naming-conventions
// https://developer.arm.com/documentation/den0013/d/Application-Binary-Interfaces/Procedure-Call-Standard?lang=en
// https://developer.arm.com/documentation/den0013/d/Instruction-Summary/Instruction-Summary/ADC?lang=en
// https://sourceforge.net/p/predef/wiki/Architectures/

asm(R"(
.arm
.syntax unified
.section .text
.align  2
.global __SwapRegisters__
.type   __SwapRegisters__, %function
__SwapRegisters__:

/* R0 is the first function argument => pointer to destination register buffer */
/* R1 is the second function argument => pointer to source register buffer */

/* the following registers must be callee saved: r4-r11,r13(SP),r15(PC) - all other registers have to be saved by the caller */

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

namespace el1::system::task
{
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
