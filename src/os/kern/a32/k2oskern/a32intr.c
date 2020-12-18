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

#include "a32kern.h"

void 
KernArch_SendIci(
    UINT32  aCurCoreIx, 
    BOOL    aSendToSpecific, 
    UINT32  aTargetCpuIx
    )
{
    UINT32 writeVal;

    if (!aSendToSpecific)
    {
        writeVal = (1 << gData.mCpuCount) - 1;
        writeVal &= ~(1 << aCurCoreIx);
    }
    else
    {
        K2_ASSERT(aTargetCpuIx < gData.mCpuCount);
        writeVal = (1 << aTargetCpuIx);
    }

    writeVal = (writeVal << 16) | aCurCoreIx;
    
    MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDSGIR, writeVal);
}

void 
A32Kern_IntrInitGicDist(
    void
)
{
    UINT32  ix;
    UINT32  numRegs32;
    UINT32  mask;

    gA32Kern_IntrCount = 32 * ((MMREG_READ32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDICTR) & 0x1F) + 1);

    //
    // disable distributor and per-cpu interface
    //
    MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDDCR, 0);
    MMREG_WRITE32(gA32Kern_GICCAddr, A32_PERIF_GICC_OFFSET_ICCICR, 0);

    numRegs32 = gA32Kern_IntrCount / 32;

    //
    // clear all enables
    //
    for (ix = 0; ix < numRegs32; ix++)
    {
        MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDICER0 + (ix * sizeof(UINT32)), 0xFFFFFFFF);
    }

    //
    // clear all pendings
    //
    for (ix = 0; ix < numRegs32; ix++)
    {
        MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDICPR0 + (ix * sizeof(UINT32)), 0xFFFFFFFF);
    }

    //
    // set all interrupts to top priority. 8 bits per interrupt
    //
    for (ix = 0; ix < gA32Kern_IntrCount / (32 / 8); ix++)
    {
        MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDIPR0 + (ix * sizeof(UINT32)), 0);
    }

    //
    // set configuration as level sensitive N:N, 2 bits per interrupt
    //
    for (ix = 0; ix < gA32Kern_IntrCount / (32 / 2); ix++)
    {
        MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDICFR0 + (ix * sizeof(UINT32)), 0);
    }

    // 
    // target all IRQs >= 16 at CPU 0. 8 bits per interrupt
    //
    for (ix = 4; ix < gA32Kern_IntrCount / (32 / 8); ix++)
    {
        MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDIPTR0 + (ix * sizeof(UINT32)), 0x01010101);
    }

    //
    // SGI 0-15 go to all processors. 8 bits per interrupt
    //
    mask = (1 << gData.mCpuCount) - 1;
    mask = mask | (mask << 8) | (mask << 16) | (mask << 24);
    for (ix = 0; ix < 16 / (32 / 8); ix++)
    {
        MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDIPTR0 + (ix * sizeof(UINT32)), mask);
    }

    //
    // enable interrupts at distributor
    //
    MMREG_WRITE32(gA32Kern_GICDAddr, A32_PERIF_GICD_OFFSET_ICDDCR,
        A32_PERIF_GICD_ICDDCR_NONSECURE_ENABLE_NONSEC);

}

void
A32Kern_IntrInitGicPerCore(
    void
)
{
    //
    // disable interrupts on this CPU interface
    //
    MMREG_WRITE32(gA32Kern_GICCAddr, A32_PERIF_GICC_OFFSET_ICCICR, 0);

    //
    // set priority mask for this cpu
    //
    MMREG_WRITE32(gA32Kern_GICCAddr, A32_PERIF_GICC_OFFSET_ICCPMR, 0xFF);

    //
    // enable interrupts on this CPU interface
    //
    MMREG_WRITE32(gA32Kern_GICCAddr, A32_PERIF_GICC_OFFSET_ICCICR,
        A32_PERIF_GICC_ICCICR_ENABLE_SECURE |
        A32_PERIF_GICC_ICCICR_ENABLE_NS);
}


UINT32
A32Kern_IntrAck(
    UINT32 *apRetAckVal
)
{
    K2_ASSERT(gA32Kern_GICCAddr != 0);
    K2_ASSERT(apRetAckVal != NULL);
    return MMREG_READ32(gA32Kern_GICCAddr, A32_PERIF_GICC_OFFSET_ICCIAR);
}

void
A32Kern_IntrEoi(
    UINT32 aAckVal
)
{
    K2_ASSERT(gA32Kern_GICCAddr != 0);
    K2_ASSERT((((aAckVal & 0x1C00) >> 10) < gData.mCpuCount) && ((aAckVal & 0x3FF) < gA32Kern_IntrCount));
    MMREG_WRITE32(gA32Kern_GICCAddr, A32_PERIF_GICC_OFFSET_ICCEOIR, aAckVal);
}

void
A32Kern_IntrSetEnable(
    UINT32  aIntrId,
    BOOL    aSetEnable
)
{
    UINT32 regOffset;

    K2_ASSERT(gA32Kern_GICCAddr != 0);
    K2_ASSERT(aIntrId < gA32Kern_IntrCount);

    regOffset = (aIntrId / 32) * sizeof(UINT32);
    aIntrId = (1 << (aIntrId & 31));

    if (aSetEnable)
    {
        regOffset += A32_PERIF_GICD_OFFSET_ICDISER0;
    }
    else
    {
        regOffset += A32_PERIF_GICD_OFFSET_ICDICER0;
    }

    MMREG_WRITE32(gA32Kern_GICDAddr, regOffset, aIntrId);
}

BOOL
A32Kern_IntrGetEnable(
    UINT32 aIntrId
)
{
    UINT32 regOffset;

    K2_ASSERT(gA32Kern_GICCAddr != 0);
    K2_ASSERT(aIntrId < gA32Kern_IntrCount);

    regOffset = (aIntrId / 32) * sizeof(UINT32);
    regOffset += A32_PERIF_GICD_OFFSET_ICDISER0;
    aIntrId = (1 << (aIntrId & 31));

    regOffset = MMREG_READ32(gA32Kern_GICDAddr, regOffset);

    return (((regOffset & aIntrId) != 0) ? TRUE : FALSE);
}

void
A32Kern_IntrClearPending(
    UINT32 aIntrId,
    BOOL   aAlsoDisable
)
{
    UINT32 regOffset;

    K2_ASSERT(gA32Kern_GICCAddr != 0);
    K2_ASSERT(aIntrId < gA32Kern_IntrCount);

    regOffset = (aIntrId / 32) * sizeof(UINT32);
    aIntrId = (1 << (aIntrId & 31));

    MMREG_WRITE32(gA32Kern_GICCAddr, regOffset + A32_PERIF_GICD_OFFSET_ICDICPR0, aIntrId);

    //
    // disable after clear or you may miss an interrupt that happens in between
    // the clear pending and the disable
    //
    if (aAlsoDisable)
    {
        MMREG_WRITE32(gA32Kern_GICDAddr, regOffset + A32_PERIF_GICD_OFFSET_ICDICER0, aIntrId);
    }
}

