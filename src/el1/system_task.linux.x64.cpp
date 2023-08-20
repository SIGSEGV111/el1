#include "def.hpp"

#ifdef EL_OS_LINUX
#ifdef __x86_64__
#include "system_task.hpp"

asm(R"(
.section .text
.global __SwapRegisters__
.type __SwapRegisters__,@function
__SwapRegisters__:

/* RDI is the first function argument => pointer to destination register buffer */
/* RSI is the second function argument => pointer to source register buffer */

/* the following registers must be callee saved: RBX, RBP, R12-R15 - all other registers have to be saved by the caller */

movq %rbx,  0(%rdi)
movq %rbp,  8(%rdi)
movq %r12, 16(%rdi)
movq %r13, 24(%rdi)
movq %r14, 32(%rdi)
movq %r15, 40(%rdi)

leaq 8(%rsp), %rcx		/* Save RSP, but exclude the return address */
movq %rcx, 48(%rdi)

movq 0(%rsp), %rcx
movq %rcx, 56(%rdi)		/* Save the return address as RIP */

movq  0(%rsi), %rbx
movq  8(%rsi), %rbp
movq 16(%rsi), %r12
movq 24(%rsi), %r13
movq 32(%rsi), %r14
movq 40(%rsi), %r15
movq 48(%rsi), %rsp

movq 56(%rsi), %rcx
pushq %rcx
ret
)");

namespace el1::system::task
{
	void TFiber::InitRegisters()
	{
		memset(&this->registers, 0, sizeof(this->registers));
		this->registers.rsp = (u64_t)this->p_stack + this->sz_stack - 8; // -8 for 16-byte alignment (not exactly sure WHY though)
		this->registers.rip = (u64_t)&TFiber::Boot;
		*(u64_t**)this->registers.rsp = nullptr;

		// FIXME: set first argument to "this"
	}
}

#endif
#endif
