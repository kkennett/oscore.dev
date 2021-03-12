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

static BOOL sgUseIoApics = FALSE;

void
X32Kern_PICInit(
    void
)
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
X32Kern_PIC_EOI_BitNum(
    UINT8 aBitNum
)
{
    K2_ASSERT((aBitNum != X32PC_IRQ_PIC2) && (aBitNum <= X32PC_IRQ_LAST));

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
}

UINT32 
X32Kern_PIC_ReadMask(
    void
)
{
    UINT32  mask;

    /* read the mask for the first PIC */
    mask = X32_IoRead8(X32PC_PIC1_DATA);
    mask |= (((UINT32)X32_IoRead8(X32PC_PIC2_DATA)) << 8);

    return mask;
}

void 
X32Kern_PIC_Mask_BitNum(
    UINT32 aBitNum
)
{
    UINT8   mask;
    UINT16  port;

    K2_ASSERT((aBitNum != X32PC_IRQ_PIC2) && (aBitNum <= X32PC_IRQ_LAST));

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
}

void 
X32Kern_PIC_Unmask_BitNum(
    UINT32 aBitNum
)
{
    UINT16  port;
    UINT8   mask;

    K2_ASSERT((aBitNum != X32PC_IRQ_PIC2) && (aBitNum <= X32PC_IRQ_LAST));

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
}

static void
sPic_SetDevIrqMask(
    UINT32  aDevIrq,
    BOOL    aSetMask
)
{
    UINT32 bitNum;

    bitNum = gX32Kern_GlobalSystemIrqOverrideMap[aDevIrq];
    if (bitNum == 0xFFFF)
        bitNum = aDevIrq;

    if (aSetMask)
    {
//        K2OSKERN_Debug("PIC.Mask Device Irq %d(%d)\n", aDevIrq, bitNum);
        X32Kern_PIC_Mask_BitNum(bitNum);
    }
    else
    {
//        K2OSKERN_Debug("PIC.Unmask Device Irq %d(%d)\n", aDevIrq, bitNum);
        X32Kern_PIC_Unmask_BitNum(bitNum);
    }
}

static UINT32 
sReadIoApic(
    UINT32 aIndex, 
    UINT32 aReg
)
{
    MMREG_WRITE32(K2OS_KVA_X32_IOAPICS + (aIndex * K2_VA32_MEMPAGE_BYTES), X32_IOAPIC_OFFSET_IOREGSEL, aReg);
    return MMREG_READ32(K2OS_KVA_X32_IOAPICS + (aIndex * K2_VA32_MEMPAGE_BYTES), X32_IOAPIC_OFFSET_IOWIN);
}

static void 
sWriteIoApic(
    UINT32 aIndex, 
    UINT32 aReg, 
    UINT32 aVal
)
{
    MMREG_WRITE32(K2OS_KVA_X32_IOAPICS + (aIndex * K2_VA32_MEMPAGE_BYTES), X32_IOAPIC_OFFSET_IOREGSEL, aReg);
    MMREG_WRITE32(K2OS_KVA_X32_IOAPICS + (aIndex * K2_VA32_MEMPAGE_BYTES), X32_IOAPIC_OFFSET_IOWIN, aVal);
}

void 
X32Kern_IoApicInit(
    UINT32 aIndex
)
{
    UINT32 v;
    UINT32 lineCount;

    //
    // if this is called, the IOAPIC at this index exists
    //
    sgUseIoApics = TRUE;
    
    v = sReadIoApic(aIndex, X32_IOAPIC_REGIX_IOAPICVER);
    K2_ASSERT((v & 0x1FF) != 0);
    
    lineCount = ((v & 0x00FF0000) >> 16) + 1;
    K2_ASSERT(lineCount > 0);

    v = gpX32Kern_MADT_IoApic[aIndex]->GlobalSystemIrqLineBase;
    do {
        if (v >= X32_DEVIRQ_MAX_COUNT)
            break;

        if ((v + X32KERN_DEVVECTOR_BASE) >= X32KERN_DEVVECTOR_LAST_ALLOWED)
            break;

        if (gX32Kern_IrqToIoApicIndexMap[v] != 0xFF)
        {
            K2OSKERN_Debug("*** Vector %d attempt to map to more than one APIC.  Ignored.\n");
        }
        else
        {
            gX32Kern_IrqToIoApicIndexMap[v] = (UINT8)aIndex;
        }
        v++;
    } while (--lineCount);
}

void
X32Kern_APICInit(
    UINT32 aCpuIx
)
{
    UINT32 reg;

    //
    // enable APIC 
    //
    reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_SPUR);
    if (!(reg & (1 << 8)))
    {
        reg |= (1 << 8);
        MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_SPUR, reg);
        reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_SPUR);
    }

    reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_ID) >> 24;
    K2_ASSERT(reg == aCpuIx);

    // Task Priority 
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TPR, 0);

    // Destination Format - select flat model
    reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_DFR);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_DFR, reg | 0xF0000000);

    // Logical Destination - set as processor bit (flat model gives us logical CPUs 0-7)
    reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LDR);
    reg &= 0x00FFFFFF;
    reg |= (1 << (aCpuIx + 24));
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LDR, reg);

    // Spurious Register - set as intr 255
    reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_SPUR);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_SPUR, reg | 0xFF);

    // mask everything and target linear device vectors
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_CMCI, X32_LOCAPIC_LVT_MASK | X32KERN_DEVVECTOR_LVT_CMCI);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_TIMER, X32_LOCAPIC_LVT_MASK | X32KERN_DEVVECTOR_LVT_TIMER);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_THERM, X32_LOCAPIC_LVT_MASK | X32KERN_DEVVECTOR_LVT_THERM);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_PERF, X32_LOCAPIC_LVT_MASK | X32KERN_DEVVECTOR_LVT_PERF);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_LINT0, X32_LOCAPIC_LVT_MASK | X32KERN_DEVVECTOR_LVT_LINT0);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_LINT1, X32_LOCAPIC_LVT_MASK | X32KERN_DEVVECTOR_LVT_LINT1);
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_ERROR, X32_LOCAPIC_LVT_MASK | X32KERN_DEVVECTOR_LVT_ERROR);
}

UINT32 
KernArch_DevIrqToVector(
    UINT32 aDevIrq
)
{
    UINT32 ret;

    K2_ASSERT(aDevIrq < X32_DEVIRQ_MAX_COUNT);

    ret = gX32Kern_GlobalSystemIrqOverrideMap[aDevIrq];
    if (ret == 0xFFFF)
        ret = aDevIrq;

    return ret + X32KERN_DEVVECTOR_BASE;
}

UINT32 
KernArch_VectorToDevIrq(
    UINT32 aVector
)
{
    UINT32 ret;

    K2_ASSERT(aVector < X32_NUM_IDT_ENTRIES);
    
    ret = gX32Kern_VectorToBeforeAnyOverrideIrqMap[aVector];
    if (ret == 0xFFFF)
        return aVector - X32KERN_DEVVECTOR_BASE;

    return ret;
}

static void
sIoApic_SetDevIrqMask(
    UINT32  aIrqIx,
    BOOL    aSetMask
)
{
    UINT32  v;
    UINT32  redIx;
    UINT32  ioApicIndex;

    redIx = gX32Kern_GlobalSystemIrqOverrideMap[aIrqIx];
    if (redIx == 0xFFFF)
        redIx = aIrqIx;

    ioApicIndex = gX32Kern_IrqToIoApicIndexMap[redIx];
    K2_ASSERT(ioApicIndex < gX32Kern_IoApicCount);
    K2_ASSERT(gpX32Kern_MADT_IoApic[ioApicIndex] != NULL);

    redIx -= gpX32Kern_MADT_IoApic[ioApicIndex]->GlobalSystemIrqLineBase;

    v = sReadIoApic(ioApicIndex, X32_IOAPIC_REGIX_REDLO(redIx));
    if (aSetMask)
    {
        v |= X32_IOAPIC_REDLO_MASK;
    }
    else
    {
        v &= ~X32_IOAPIC_REDLO_MASK;
    }
    sWriteIoApic(ioApicIndex, X32_IOAPIC_REGIX_REDLO(redIx), v);
}

void 
X32Kern_ConfigDevIrq(
    K2OSKERN_IRQ_CONFIG const *apConfig
)
{
    UINT32  redIx;
    UINT32  redHi;
    UINT32  redLo;
    UINT32  ioApicIndex;

    if (apConfig->mSourceIrq >= X32_DEVIRQ_LVT_BASE)
    {
//        K2OSKERN_Debug("ConfigDevIrq(%d) ignored\n", apConfig->mSourceIrq);
        return;
    }

    if (gpX32Kern_MADT == NULL)
        return;

    //
    // default to core 0 targeted
    //
    redHi = (1 << X32_IOAPIC_REDHI_DEST_SHIFT);

    redIx = gX32Kern_GlobalSystemIrqOverrideMap[apConfig->mSourceIrq];
    if (redIx != 0xFFFF)
    {
        //
        // use flags in override
        //
        switch (gX32Kern_GlobalSystemIrqOverrideFlags[apConfig->mSourceIrq] & ACPI_APIC_INTR_FLAG_POLARITY_MASK)
        {
        case ACPI_APIC_INTR_FLAG_POLARITY_BUS:
            if ((gX32Kern_GlobalSystemIrqOverrideFlags[apConfig->mSourceIrq] & ACPI_APIC_INTR_FLAG_TRIGGER_MASK) == ACPI_APIC_INTR_FLAG_TRIGGER_LEVEL)
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

        switch (gX32Kern_GlobalSystemIrqOverrideFlags[apConfig->mSourceIrq] & ACPI_APIC_INTR_FLAG_TRIGGER_MASK)
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
        redIx = apConfig->mSourceIrq;

        K2_ASSERT(apConfig->mDestCoreIx < gData.LoadInfo.mCpuCoreCount);

        redHi = (1 << (X32_IOAPIC_REDHI_DEST_SHIFT + apConfig->mDestCoreIx));

        if (apConfig->Line.mIsActiveLow)
            redLo = X32_IOAPIC_REDLO_POLARITY_LOW;
        else
            redLo = X32_IOAPIC_REDLO_POLARITY_HIGH;

        if (apConfig->Line.mIsEdgeTriggered)
            redLo |= X32_IOAPIC_REDLO_TRIGGER_EDGE;
        else
            redLo |= X32_IOAPIC_REDLO_TRIGGER_LEVEL;
    }

    ioApicIndex = gX32Kern_IrqToIoApicIndexMap[redIx];

    K2_ASSERT(ioApicIndex < gX32Kern_IoApicCount);
    K2_ASSERT(gpX32Kern_MADT_IoApic[ioApicIndex] != NULL);

    redIx -= gpX32Kern_MADT_IoApic[ioApicIndex]->GlobalSystemIrqLineBase;

    redLo |= X32_IOAPIC_REDLO_MASK;
    redLo |= X32_IOAPIC_REDLO_DEST_LOG;
    redLo |= X32_IOAPIC_REDLO_MODE_FIXED;
    redLo |= ((redIx + X32KERN_DEVVECTOR_BASE) & X32_IOAPIC_REDLO_VECTOR_MASK);

//    K2OSKERN_Debug("Writing to IOAPIC %d target %d, LO %08X HI %08X\n", redIx, (redLo & X32_IOAPIC_REDLO_VECTOR_MASK), redLo, redHi);

    sWriteIoApic(ioApicIndex, X32_IOAPIC_REGIX_REDLO(redIx), redLo);
    sWriteIoApic(ioApicIndex, X32_IOAPIC_REGIX_REDHI(redIx), redHi);
}

static void
sLvt_SetDevIrqMask(
    UINT32  aDevIrq,
    BOOL    aSetMask
)
{
//    K2OSKERN_CPUCORE *  pThisCore;
    UINT32              offset;
    UINT32              localIrqIx;
    UINT32              reg;

//    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;

    localIrqIx = aDevIrq - X32_DEVIRQ_LVT_BASE;

    K2_ASSERT(localIrqIx < 7);

    if (localIrqIx == 0)
        offset = X32_LOCAPIC_OFFSET_LVT_CMCI;
    else
        offset = X32_LOCAPIC_OFFSET_LVT_TIMER + (0x10 * (localIrqIx - 1));

    reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, offset);
    if (aSetMask)
    {
//        K2OSKERN_Debug("LVT.Mask Core %d local ix %d (%08X)\n", pThisCore->mCoreIx, localIrqIx, offset);
        reg |= X32_LOCAPIC_LVT_MASK;
    }
    else
    {
//        K2OSKERN_Debug("LVT.Unmask Core %d local ix %d (%08X)\n", pThisCore->mCoreIx, localIrqIx, offset);
        reg &= ~(X32_LOCAPIC_LVT_MASK | X32_LOCAPIC_LVT_STATUS);
    }
    MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, offset, reg);
}

void 
X32Kern_MaskDevIrq(
    UINT8 aDevIrqIx
)
{
    K2_ASSERT(aDevIrqIx < X32_DEVIRQ_MAX_COUNT);

    if (aDevIrqIx >= X32_DEVIRQ_LVT_BASE)
        sLvt_SetDevIrqMask(aDevIrqIx, TRUE);
    else if (sgUseIoApics)
        sIoApic_SetDevIrqMask(aDevIrqIx, TRUE);
    else
        sPic_SetDevIrqMask(aDevIrqIx, TRUE);
}

void 
X32Kern_UnmaskDevIrq(
    UINT8 aDevIrqIx
)
{
    K2_ASSERT(aDevIrqIx < X32_DEVIRQ_MAX_COUNT);

    if (aDevIrqIx >= X32_DEVIRQ_LVT_BASE)
        sLvt_SetDevIrqMask(aDevIrqIx, FALSE);
    else if (sgUseIoApics)
        sIoApic_SetDevIrqMask(aDevIrqIx, FALSE);
    else
        sPic_SetDevIrqMask(aDevIrqIx, FALSE);
}

void 
X32Kern_EOI(
    UINT32 aVector
)
{
    if (sgUseIoApics)
    {
        MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_EOI, 0);
    }
    else
    {
        X32Kern_PIC_EOI_BitNum((UINT8)KernArch_VectorToDevIrq(aVector));
    }
}

BOOL
KernArch_PollIrq(
    K2OSKERN_CPUCORE volatile * apThisCore
)
{
    // Call X32Kern_Intr_OnIrq for any detected pending irq interrupt
    return FALSE;
}

