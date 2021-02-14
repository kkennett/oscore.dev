#include "a32kernasm.inc"

.extern A32Kern_ExceptionCommon

//void K2_CALLCONV_REGS KernArch_RaiseException(K2STAT aExceptionCode);

BEGIN_A32_PROC(KernArch_RaiseException)
    stmdb r13!, {r14}
    stmdb r13!, {r14}
    sub r13, r13, #4
    stmdb r13!, {r0-r12}
    add r0, r13, #(16*4)
    str r0, [r13, #(13*4)]
    mrs r2, cpsr
    orr r3, r2, #A32_PSR_I_BIT
    msr cpsr, r3
    stmdb r13!, {r2}
    mov r0, #A32KERN_EXCEPTION_REASON_RAISE_EXCEPTION
    mov r1, r13
    b A32Kern_ExceptionCommon
END_A32_PROC(KernArch_RaiseException)

    .end
