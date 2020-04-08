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

static BOOL     sgUseIoApic = FALSE;
static UINT32   sgIoRedirectionEntryCount = 0; // something like 24

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

void X32Kern_PIC_EOI_BitNum(UINT8 aBitNum)
{
    BOOL disp;

    K2_ASSERT((aBitNum != X32PC_IRQ_PIC2) && (aBitNum <= X32PC_IRQ_LAST));

    /* disable interrupts */
    disp = K2OSKERN_SetIntr(FALSE);

    if (aBitNum > 7)
    {
        /* mark specific EOI for slave PIC */
        aBitNum -= 8;

        X32_IoWrite8(X32PC_8259_OCW_SPECIFIC_EOI(aBitNum), X32PC_PIC2_COMMAND);

        /* set to EOI slave's port on master PIC */
        aBitNum = X32PC_IRQ_PIC2;
    }

    /* mark specific EOI for master PIC */
    X32_IoWrite8(X32PC_8259_OCW_SPECIFIC_EOI(aBitNum), X32PC_PIC1_COMMAND);

    /* put interrupts back to whatever they were */
    K2OSKERN_SetIntr(disp);
}

UINT32 X32Kern_PIC_ReadMask(void)
{
    BOOL    disp;
    UINT32  mask;

    /* disable interrupts */
    disp = K2OSKERN_SetIntr(FALSE);

    /* read the mask for the first PIC */
    mask = X32_IoRead8(X32PC_PIC1_DATA);
    mask |= (((UINT32)X32_IoRead8(X32PC_PIC2_DATA)) << 8);

    /* put interrupts back to whatever they were */
    K2OSKERN_SetIntr(disp);

    return mask;
}

void X32Kern_PIC_Mask_BitNum(UINT32 aBitNum)
{
    BOOL    disp;
    UINT8   mask;
    UINT16  port;

    K2_ASSERT((aBitNum != X32PC_IRQ_PIC2) && (aBitNum <= X32PC_IRQ_LAST));

    /* disable interrupts */
    disp = K2OSKERN_SetIntr(FALSE);

    if (aBitNum > 7)
    {
        /* mask on slave PIC */
        aBitNum -= 8;
        port = X32PC_PIC2_DATA;
    }
    else
        port = X32PC_PIC1_DATA;

    /* change the mask */
    mask = X32_IoRead8(port);
    if (!(mask & (1 << aBitNum)))
    {
        mask |= (1 << aBitNum);
        X32_IoWrite8(mask, port);
    }

    /* put interrupts back to whatever they were */
    K2OSKERN_SetIntr(disp);
}

void X32Kern_PIC_Unmask_BitNum(UINT32 aBitNum)
{
    BOOL    disp;
    UINT16  port;
    UINT8   mask;

    K2_ASSERT((aBitNum != X32PC_IRQ_PIC2) && (aBitNum <= X32PC_IRQ_LAST));

    /* disable interrupts */
    disp = K2OSKERN_SetIntr(FALSE);

    if (aBitNum > 7)
    {
        /* mask on slave PIC */
        aBitNum -= 8;
        port = X32PC_PIC2_DATA;
    }
    else
        port = X32PC_PIC1_DATA;

    /* change the mask */
    mask = X32_IoRead8(port);
    if (mask & (1 << aBitNum))
    {
        mask &= ~(1 << aBitNum);
        X32_IoWrite8(mask, port);
    }

    /* put interrupts back to whatever they were */
    K2OSKERN_SetIntr(disp);
}

static
void
sPic_SetDevIntrMask(
    UINT32  aDevIntr,
    BOOL    aSetMask
)
{
    UINT32 bitNum;

    bitNum = gX32Kern_IntrOverrideMap[aDevIntr];
    if (bitNum == (UINT32)-1)
        bitNum = aDevIntr;

    if (aSetMask)
    {
//        K2OSKERN_Debug("PIC.Mask Device Intr %d(%d)\n", aDevIntr, bitNum);
        X32Kern_PIC_Mask_BitNum(bitNum);
    }
    else
    {
//        K2OSKERN_Debug("PIC.Unmask Device Intr %d(%d)\n", aDevIntr, bitNum);
        X32Kern_PIC_Unmask_BitNum(bitNum);
    }
}

UINT32 sReadIoApic(UINT32 aReg)
{
    MMREG_WRITE32(K2OSKERN_X32_IOAPIC_KVA, X32_IOAPIC_OFFSET_IOREGSEL, aReg);
    return MMREG_READ32(K2OSKERN_X32_IOAPIC_KVA, X32_IOAPIC_OFFSET_IOWIN);
}

void sWriteIoApic(UINT32 aReg, UINT32 aVal)
{
    MMREG_WRITE32(K2OSKERN_X32_IOAPIC_KVA, X32_IOAPIC_OFFSET_IOREGSEL, aReg);
    MMREG_WRITE32(K2OSKERN_X32_IOAPIC_KVA, X32_IOAPIC_OFFSET_IOWIN, aVal);
}

void X32Kern_IoApicInit(void)
{
    UINT32 v;

    //
    // if this is called, the IOAPIC exists
    //
    sgUseIoApic = TRUE;

    v = sReadIoApic(X32_IOAPIC_REGIX_IOAPICVER);

    K2_ASSERT((v & 0x1FF) != 0);
    sgIoRedirectionEntryCount = ((v & 0x00FF0000) >> 16) + 1;
}

UINT32  
KernArch_DevIntrToSysIrq(
    UINT32 aDevIntr
)
{
    UINT32 ret;

    ret = gX32Kern_IntrOverrideMap[aDevIntr];
    if (ret != (UINT32)-1)
    {
        return ret + X32KERN_INTR_DEV_BASE;
    }
    return aDevIntr + X32KERN_INTR_DEV_BASE;
}

UINT32
KernArch_SysIrqToDevIntr(
    UINT32 aSysIrq
)
{
    K2_ASSERT(aSysIrq >= X32KERN_INTR_DEV_BASE);

    return gX32Kern_IrqToDevIntrMap[aSysIrq];
}

static
void
sIoApic_SetDevIntrMask(
    UINT32  aDevIntr,
    BOOL    aSetMask
)
{
    UINT32 v;
    UINT32 redIx;

    redIx = gX32Kern_IntrOverrideMap[aDevIntr];
    if (redIx == (UINT32)-1)
        redIx = aDevIntr;

    v = sReadIoApic(X32_IOAPIC_REGIX_REDLO(redIx));
    if (aSetMask)
    {
//        K2OSKERN_Debug("IOAPIC.Mask Device Intr %d(%d)\n", aDevIntr, redIx);
        v |= X32_IOAPIC_REDLO_MASK;
    }
    else
    {
//        K2OSKERN_Debug("IOAPIC.Unmask Device Intr %d(%d)\n", aDevIntr, redIx);
        v &= ~X32_IOAPIC_REDLO_MASK;
    }
    sWriteIoApic(X32_IOAPIC_REGIX_REDLO(redIx), v);
}

void X32Kern_ConfigDevIntr(K2OSKERN_INTR_CONFIG const *apConfig)
{
    UINT32 redIx;
    UINT32 redHi;
    UINT32 redLo;

    if (gpX32Kern_MADT == NULL)
        return;

    //
    // default to core 0 targeted
    //
    redHi = (1 << X32_IOAPIC_REDHI_DEST_SHIFT);

    redIx = gX32Kern_IntrOverrideMap[apConfig->mSourceId];
    if (redIx != (UINT32)-1)
    {
        //
        // use flags in override
        //
        switch (gX32Kern_IntrOverrideFlags[apConfig->mSourceId] & ACPI_APIC_INTR_FLAG_POLARITY_MASK)
        {
        case ACPI_APIC_INTR_FLAG_POLARITY_BUS:
            if ((gX32Kern_IntrOverrideFlags[apConfig->mSourceId] & ACPI_APIC_INTR_FLAG_TRIGGER_MASK) == ACPI_APIC_INTR_FLAG_TRIGGER_LEVEL)
            {
                redLo = X32_IOAPIC_REDLO_POLARITY_LOW;
            }
            else
            {
                redLo = X32_IOAPIC_REDLO_POLARITY_HIGH;
            }
            break;

        case ACPI_APIC_INTR_FLAG_POLARITY_ACTIVE_HIGH:
            redLo = X32_IOAPIC_REDLO_POLARITY_HIGH;
            break;

        case ACPI_APIC_INTR_FLAG_POLARITY_ACTIVE_LOW:
            redLo = X32_IOAPIC_REDLO_POLARITY_LOW;
            break;

        default:
            K2_ASSERT(0);
            return;
        }

        switch (gX32Kern_IntrOverrideFlags[apConfig->mSourceId] & ACPI_APIC_INTR_FLAG_TRIGGER_MASK)
        {
        case ACPI_APIC_INTR_FLAG_TRIGGER_BUS:
        case ACPI_APIC_INTR_FLAG_TRIGGER_EDGE:
            redLo |= X32_IOAPIC_REDLO_TRIGGER_EDGE;
            break;

        case ACPI_APIC_INTR_FLAG_TRIGGER_LEVEL:
            redLo |= X32_IOAPIC_REDLO_TRIGGER_LEVEL;
            break;

        default:
            K2_ASSERT(0);
            return;
        }
    }
    else
    {
        //
        // not overridden in ACPI - use flags in apConfig
        //
        redIx = apConfig->mSourceId;

        K2_ASSERT(apConfig->mDestCoreIx < gData.mCpuCount);

        redHi = (1 << (X32_IOAPIC_REDHI_DEST_SHIFT + apConfig->mDestCoreIx));

        if (apConfig->mIsActiveLow)
            redLo = X32_IOAPIC_REDLO_POLARITY_LOW;
        else
            redLo = X32_IOAPIC_REDLO_POLARITY_HIGH;

        if (apConfig->mIsLevelTriggered)
            redLo |= X32_IOAPIC_REDLO_TRIGGER_LEVEL;
        else
            redLo |= X32_IOAPIC_REDLO_TRIGGER_EDGE;
    }

    redLo |= X32_IOAPIC_REDLO_MASK;
    redLo |= X32_IOAPIC_REDLO_DEST_LOG;
    redLo |= X32_IOAPIC_REDLO_MODE_FIXED;
    redLo |= ((redIx + X32KERN_INTR_DEV_BASE) & X32_IOAPIC_REDLO_VECTOR_MASK);

//    K2OSKERN_Debug("Writing to IOAPIC %d target %d, LO %08X HI %08X\n", redIx, (redLo & X32_IOAPIC_REDLO_VECTOR_MASK), redLo, redHi);

    sWriteIoApic(X32_IOAPIC_REGIX_REDLO(redIx), redLo);
    sWriteIoApic(X32_IOAPIC_REGIX_REDHI(redIx), redHi);
}

void X32Kern_MaskDevIntr(UINT8 aDevIntrId)
{
    K2_ASSERT(K2OSKERN_GetIntr() == FALSE);
    if (gpX32Kern_MADT != NULL)
        sIoApic_SetDevIntrMask(aDevIntrId, TRUE);
    else
        sPic_SetDevIntrMask(aDevIntrId, TRUE);
}

void X32Kern_UnmaskDevIntr(UINT8 aDevIntrId)
{
    K2_ASSERT(K2OSKERN_GetIntr() == FALSE);
    if (gpX32Kern_MADT != NULL)
        sIoApic_SetDevIntrMask(aDevIntrId, FALSE);
    else
        sPic_SetDevIntrMask(aDevIntrId, FALSE);
}

void X32Kern_EOI(UINT32 aSysIrq)
{
    UINT32 devIntr;

    devIntr = KernArch_SysIrqToDevIntr(aSysIrq);

    if (gpX32Kern_MADT != NULL)
    {
        MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_EOI, devIntr);
    }
    else
    {
        X32Kern_PIC_EOI_BitNum((UINT8)devIntr);
    }
}

