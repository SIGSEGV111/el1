#include "def.hpp"

#ifdef EL_OS_LINUX
#if defined(__riscv) && __riscv_xlen == 64
#include "system_task.hpp"

#if defined(__riscv_float_abi_double)
	#define EL_RISCV64_SAVE_FP_REGISTERS R"(
fsd  f8, 112(a0)
fsd  f9, 120(a0)
fsd f18, 128(a0)
fsd f19, 136(a0)
fsd f20, 144(a0)
fsd f21, 152(a0)
fsd f22, 160(a0)
fsd f23, 168(a0)
fsd f24, 176(a0)
fsd f25, 184(a0)
fsd f26, 192(a0)
fsd f27, 200(a0)
)"
	#define EL_RISCV64_LOAD_FP_REGISTERS R"(
fld  f8, 112(a1)
fld  f9, 120(a1)
fld f18, 128(a1)
fld f19, 136(a1)
fld f20, 144(a1)
fld f21, 152(a1)
fld f22, 160(a1)
fld f23, 168(a1)
fld f24, 176(a1)
fld f25, 184(a1)
fld f26, 192(a1)
fld f27, 200(a1)
)"
#elif defined(__riscv_float_abi_single)
	#define EL_RISCV64_SAVE_FP_REGISTERS R"(
fsw  f8, 112(a0)
fsw  f9, 116(a0)
fsw f18, 120(a0)
fsw f19, 124(a0)
fsw f20, 128(a0)
fsw f21, 132(a0)
fsw f22, 136(a0)
fsw f23, 140(a0)
fsw f24, 144(a0)
fsw f25, 148(a0)
fsw f26, 152(a0)
fsw f27, 156(a0)
)"
	#define EL_RISCV64_LOAD_FP_REGISTERS R"(
flw  f8, 112(a1)
flw  f9, 116(a1)
flw f18, 120(a1)
flw f19, 124(a1)
flw f20, 128(a1)
flw f21, 132(a1)
flw f22, 136(a1)
flw f23, 140(a1)
flw f24, 144(a1)
flw f25, 148(a1)
flw f26, 152(a1)
flw f27, 156(a1)
)"
#else
	#define EL_RISCV64_SAVE_FP_REGISTERS ""
	#define EL_RISCV64_LOAD_FP_REGISTERS ""
#endif

asm(R"(
.section .text
.global __SwapRegisters__
.type __SwapRegisters__, @function
__SwapRegisters__:

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

# Save floating-point callee-saved registers for hard-float ABIs
)"
EL_RISCV64_SAVE_FP_REGISTERS
R"(

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

# Restore floating-point callee-saved registers for hard-float ABIs
)"
EL_RISCV64_LOAD_FP_REGISTERS
R"(

ret
)");

#undef EL_RISCV64_SAVE_FP_REGISTERS
#undef EL_RISCV64_LOAD_FP_REGISTERS

namespace el1::system::task
{
	void TFiber::InitRegisters()
	{
		memset(&this->registers, 0, sizeof(this->registers));
		this->registers.sp = (u64_t)this->p_stack + this->sz_stack;
		this->registers.ra = (u64_t)&TFiber::Boot;

		// FIXME: set first argument to "this"
	}
}

#endif
#endif
