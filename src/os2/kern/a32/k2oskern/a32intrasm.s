//   
//   BSD 3-Clause License
//   
//   Copyright (c) 2020, Kurt Kennett
//   All rights reserved.
//   
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//   
//   1. Redistributions of source code must retain the above copyright notice, this
//      list of conditions and the following disclaimer.
//   
//   2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//   
//   3. Neither the name of the copyright holder nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//   
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "a32kernasm.inc"

//void A32Kern_ResetVector(void);
BEGIN_A32_PROC(A32Kern_ResetVector)
    // should be unused
    b A32Kern_ResetVector
END_A32_PROC(A32Kern_ResetVector)

.extern A32Kern_ResumeInMonitor
.extern A32Kern_InterruptHandler
.extern A32Kern_CheckSvcInterrupt
.extern A32Kern_CheckIrqInterrupt

BEGIN_A32_PROC(A32Kern_ExceptionCommon)

    // r0 is reason for exception
    // r13 points to base of exception context (SPSR field)
    // spsr, r13, r14 not saved yet.
    
    // save spsr
    mrs r1, spsr
    str r1, [r13]

    // mask off where we came from. change usr mode to sys mode for purposes of saving
    and r1, r1, #A32_PSR_MODE_MASK
    cmp r1, #A32_PSR_MODE_USR
    moveq r1, #A32_PSR_MODE_SYS

    // save exception context base so can push stuff into context from another mode (r13,r14 different)
    mov r12, r13    

    // get current state into r2 and r3, remove mode bits in r2
    mrs r2, cpsr
    mov r3, r2
    bic r2, r2, #A32_PSR_MODE_MASK

    // change to source mode (will never be USR) and retrieve R13, R14, then come back again
    orr r2, r2, r1
    msr cpsr, r2
    str r13, [r12, #56]
    str r14, [r12, #60]
    msr cpsr, r3

    // context saved. go to C code now
    // r0 is reason
    mov r1, r13     // r1 is stack pointer in exception stack (points to exception context)
    mov r2, r3      // r2 is cpsr to tell where we are
    bl A32Kern_InterruptHandler

    // on return pop exception from stack and go to OS monitor with stack at r0

    // context saved to kernel mode thread stack - restore r13 back to bottom of exception mode stack (whatever it is)
    add r13, r13, #64

    // now just resume in sys with r0 value returned from A32Kern_InterruptHandler above
    b A32Kern_ResumeInMonitor
    // not reached here
END_A32_PROC(A32Kern_ExceptionCommon)

//void A32Kern_UndExceptionVector(void);
BEGIN_A32_PROC(A32Kern_UndExceptionVector)
    sub r14, r14, #4
    // lr points to undefined instruction
    sub r13, r13, #12
    stmda r13!, {r0-r12}
    mov r11, #0
    str r14, [r13, #64]
    mov r0, #A32KERN_EXCEPTION_REASON_UNDEFINED_INSTRUCTION
    b A32Kern_ExceptionCommon
END_A32_PROC(A32Kern_UndExceptionVector)

//void A32Kern_SvcExceptionVector(void);
BEGIN_A32_PROC(A32Kern_SvcExceptionVector)
    // lr points to instruction after the SWI
    stmda r13!, {r12,r14}
    mov r12, r13
    stmda r12, {r13}^
    sub r13, r13, #40
    stmda r13!, {r0-r3}
    mrs r12, spsr
    str r12, [r13]
    mov r0, r13
    bl A32Kern_CheckSvcInterrupt
    ldr r12, [r13]
    msr spsr_cxsf, r12
    cmp r0, #0
    bne full_save_svc
    ldmib r13!, {r0-r3}
    add r13, r13, #40
    ldmib r13!, {r12, pc}^ 
full_save_svc:
    add r13, r13, #20
    stmia r13, {r4-r11}
    mov r11, #0
    sub r13, r13, #20
    ldr r12, [r13, #60]
    str r12, [r13, #52]
    mov r0, #A32KERN_EXCEPTION_REASON_SYSTEM_CALL
    b A32Kern_ExceptionCommon
END_A32_PROC(A32Kern_SvcExceptionVector)

//void A32Kern_PrefetchAbortExceptionVector(void);
BEGIN_A32_PROC(A32Kern_PrefetchAbortExceptionVector)
    sub r14, r14, #4
    // lr points to address of cause of prefetch abort
    sub r13, r13, #12
    stmda r13!, {r0-r12}
    mov r11, #0
    str r14, [r13, #64]
    mov r0, #A32KERN_EXCEPTION_REASON_PREFETCH_ABORT
    b A32Kern_ExceptionCommon
END_A32_PROC(A32Kern_PrefetchAbortExceptionVector)

//void A32Kern_DataAbortExceptionVector(void);
BEGIN_A32_PROC(A32Kern_DataAbortExceptionVector)
    sub r14, r14, #8
    // lr points to address of cause of data abort
    sub r13, r13, #12
    stmda r13!, {r0-r12}
    mov r11, #0
    str r14, [r13, #64]
    mov r0, #A32KERN_EXCEPTION_REASON_DATA_ABORT
    b A32Kern_ExceptionCommon
END_A32_PROC(A32Kern_DataAbortExceptionVector)

//void A32Kern_IRQExceptionVector(void);
.extern A32Kern_INTR_IRQ
BEGIN_A32_PROC(A32Kern_IRQExceptionVector)
    sub r14, r14, #4
    // lr points to address to return to after IRQ handling
    stmda r13!, {r12,r14}
    mov r12, r13
    stmda r12, {r13}^
    sub r13, r13, #40
    stmda r13!, {r0-r3}
    mrs r12, spsr
    str r12, [r13]
    mov r0, r13
    bl A32Kern_CheckIrqInterrupt
    ldr r12, [r13]
    msr spsr_cxsf, r12
    cmp r0, #0
    bne full_save_irq
    ldmib r13!, {r0-r3}
    add r13, r13, #40
    ldmib r13!, {r12, pc}^ 
full_save_irq:
    add r13, r13, #20
    stmia r13, {r4-r11}
    mov r11, #0
    sub r13, r13, #20
    ldr r12, [r13, #60]
    str r12, [r13, #52]
    mov r0, #A32KERN_EXCEPTION_REASON_IRQ_RESCHED
    b A32Kern_ExceptionCommon
END_A32_PROC(A32Kern_IRQExceptionVector)

//void A32Kern_FIQExceptionVector(void);
BEGIN_A32_PROC(A32Kern_FIQExceptionVector)
    // should be unused
    b A32Kern_FIQExceptionVector
END_A32_PROC(A32Kern_FIQExceptionVector)

    .end



