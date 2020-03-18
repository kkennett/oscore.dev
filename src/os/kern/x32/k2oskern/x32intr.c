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

#include "x32kern.h"

BOOL
K2_CALLCONV_REGS
K2OSKERN_SetIntr(
    BOOL aEnable
)
{
    return !X32_SetCoreInterruptMask(!aEnable);
}

BOOL
K2_CALLCONV_REGS
K2OSKERN_GetIntr(
    void
    )
{
    return X32_GetCoreInterruptMask() ? FALSE : TRUE;
}

void
X32Kern_PICInit(void)
{
    /* start the init sequence (ICW1) */
    X32_IoWrite8(X32PC_8259_PIC_ICW1_INIT | X32PC_8259_PIC_ICW1_WORD4NEEDED, X32PC_PIC1_COMMAND);
    X32_IoWait();
    X32_IoWrite8(X32PC_8259_PIC_ICW1_INIT | X32PC_8259_PIC_ICW1_WORD4NEEDED, X32PC_PIC2_COMMAND);
    X32_IoWait();

    /* define the PIC vector bases (ICW2) */
    X32_IoWrite8(32, X32PC_PIC1_DATA);    /* master IRQ 0-7 -> IRQ 32->39 */
    X32_IoWait();
    X32_IoWrite8(40, X32PC_PIC2_DATA);    /* slave  IRQ 0-7 -> IRQ 40->47 */
    X32_IoWait();

    /* define the slave location (ICW3) */
    X32_IoWrite8(0x04, X32PC_PIC1_DATA);  /* MASTER, so bit 2 is a SLAVE */
    X32_IoWait();
    X32_IoWrite8(2, X32PC_PIC2_DATA);     /* SLAVE, id is 2 */
    X32_IoWait();

    /* operation mode (ICW4) - not special, nonbuffered, normal EOI, 8086 mode */
    X32_IoWrite8(X32PC_8259_PIC_ICW4_8086MODE, X32PC_PIC1_DATA);
    X32_IoWait();
    X32_IoWrite8(X32PC_8259_PIC_ICW4_8086MODE, X32PC_PIC2_DATA);
    X32_IoWait();

    /* init finished */

    /* mask all interrupts */
    X32_IoWrite8(0xFB, X32PC_PIC1_DATA);    /* bit 2 clear (2nd pic) */
    X32_IoWrite8(0xFF, X32PC_PIC2_DATA);
}

void 
X32Kern_APICInit(UINT32 aCpuIx)
{
    UINT32 reg;

    //
    // enable APIC 
    //
    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_SPUR);
    if (!(reg & (1 << 8)))
    {
        reg |= (1 << 8);
        MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_SPUR, reg);
        reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_SPUR);
    }

    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_ID) >> 24;
    K2_ASSERT(reg == aCpuIx);

    // Task Priority 
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TPR, 0);

    // Destination Format - select flat model
    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_DFR);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_DFR, reg | 0xF0000000);

    // Logical Destination - set as processor bit (flat model gives us logical CPUs 0-7)
    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LDR);
    reg &= 0x00FFFFFF;
    reg |= (1 << (aCpuIx + 24));
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LDR, reg);

    // Spurious Register - set as intr 255
    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_SPUR);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_SPUR, reg | 0xFF);

    // mask everything and target linear device vectors
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_CMCI,  X32_LOCAPIC_LVT_MASK | X32KERN_INTR_LVT_CMCI);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_TIMER, X32_LOCAPIC_LVT_MASK | X32KERN_INTR_LVT_TIMER);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_THERM, X32_LOCAPIC_LVT_MASK | X32KERN_INTR_LVT_THERM);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_PERF,  X32_LOCAPIC_LVT_MASK | X32KERN_INTR_LVT_PERF);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_LINT0, X32_LOCAPIC_LVT_MASK | X32KERN_INTR_LVT_LINT0);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_LINT1, X32_LOCAPIC_LVT_MASK | X32KERN_INTR_LVT_LINT1);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_ERROR, X32_LOCAPIC_LVT_MASK | X32KERN_INTR_LVT_ERROR);

    // set timer divisor to 4 to get bus clock rate (FSB rate is gBusClockRate * 4)
    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_DIV);
    reg &= ~X32_LOCAPIC_TIMER_DIV_MASK;
    reg |= X32_LOCAPIC_TIMER_DIV_4;
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_DIV, reg);

    // make sure the timer is stopped
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_INIT, 0);

    // set timer mode to one shot
    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_TIMER);
    reg &= ~X32_LOCAPIC_LVT_TIMER_MODE_MASK;
    reg |= X32_LOCAPIC_LVT_TIMER_ONESHOT;
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_TIMER, reg);
}

void 
X32Kern_IoApicInit(void)
{
    UINT32 v;

    MMREG_WRITE32(K2OSKERN_X32_IOAPIC_KVA, X32_IOAPIC_OFFSET_IOREGSEL, X32_IOAPIC_REGIX_IOAPICVER);
    v = MMREG_READ32(K2OSKERN_X32_IOAPIC_KVA, X32_IOAPIC_OFFSET_IOWIN);
    K2_ASSERT(v != 0);
}

void KernArch_SendIci(UINT32 aCurCoreIx, BOOL aSendToSpecific, UINT32 aTargetCpuIx)
{
    UINT32 reg;

    /* wait for pending send bit to clear */
    do {
        reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_ICR_LOW32);
    } while (reg & (1 << 12));

    /* set target CPUs to send to */
    if (!aSendToSpecific)
    {
        /* set mask */
        reg = (1 << gData.mCpuCount) - 1;
        reg &= ~(1 << aCurCoreIx);
    }
    else
        reg = 1 << aTargetCpuIx;
    reg <<= 24;

    /* send to logical CPU interrupt 32 + my Index */
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_ICR_HIGH32, reg);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_ICR_LOW32, X32_LOCAPIC_ICR_LOW_LEVEL_ASSERT | X32_LOCAPIC_ICR_LOW_LOGICAL | X32_LOCAPIC_ICR_LOW_MODE_FIXED | (X32KERN_INTR_ICI_BASE + aCurCoreIx));
}
