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

void X32Kern_GDTSetup(void)
{
    UINT32 addr;
    UINT32 coreIx;

    K2MEM_Zero(gX32Kern_GDT, X32_SIZEOF_GDTENTRY * X32_NUM_SEGMENTS);
    K2MEM_Zero(gX32Kern_LDT, X32_SIZEOF_GDTENTRY * K2OS_MAX_CPU_COUNT);
    
    /* entry 1 - kernel 4GB flat code segment */
    gX32Kern_GDT[X32_SEGMENT_KERNEL_CODE].mLimitLow16 = 0xFFFF;
    gX32Kern_GDT[X32_SEGMENT_KERNEL_CODE].mAttrib = 0x9A;          
    gX32Kern_GDT[X32_SEGMENT_KERNEL_CODE].mLimitHigh4Attrib4 = 0xCF;

    /* entry 2 - kernel 4GB flat data segment */
    gX32Kern_GDT[X32_SEGMENT_KERNEL_DATA].mLimitLow16 = 0xFFFF;
    gX32Kern_GDT[X32_SEGMENT_KERNEL_DATA].mAttrib = 0x92;          
    gX32Kern_GDT[X32_SEGMENT_KERNEL_DATA].mLimitHigh4Attrib4 = 0xCF;

    /* entry 3 - kernel 64KB code segment */
    gX32Kern_GDT[X32_SEGMENT_SELECTOR_KERNEL_64K_CODE].mLimitLow16 = 0xFFFF;
    gX32Kern_GDT[X32_SEGMENT_SELECTOR_KERNEL_64K_CODE].mAttrib = 0x9A;          
    gX32Kern_GDT[X32_SEGMENT_SELECTOR_KERNEL_64K_CODE].mLimitHigh4Attrib4 = 0;

    /* entry 4 - kernel 64KB data segment */
    gX32Kern_GDT[X32_SEGMENT_SELECTOR_KERNEL_64K_DATA].mLimitLow16 = 0xFFFF;
    gX32Kern_GDT[X32_SEGMENT_SELECTOR_KERNEL_64K_DATA].mAttrib = 0x92;          
    gX32Kern_GDT[X32_SEGMENT_SELECTOR_KERNEL_64K_DATA].mLimitHigh4Attrib4 = 0;

    /* entry 5 - user 4GB flat code segment */
    gX32Kern_GDT[X32_SEGMENT_USER_CODE].mLimitLow16 = 0xFFFF;
    gX32Kern_GDT[X32_SEGMENT_USER_CODE].mAttrib = 0xFA;          
    gX32Kern_GDT[X32_SEGMENT_USER_CODE].mLimitHigh4Attrib4 = 0xCF;

    /* entry 6 - user 4GB flat data segment */
    gX32Kern_GDT[X32_SEGMENT_USER_DATA].mLimitLow16 = 0xFFFF;
    gX32Kern_GDT[X32_SEGMENT_USER_DATA].mAttrib = 0xF2;          
    gX32Kern_GDT[X32_SEGMENT_USER_DATA].mLimitHigh4Attrib4 = 0xCF;

    /* entry 7 - kernel mode LDT.  entries in LDT are indexed per-core values */
    addr = (UINT32)&gX32Kern_LDT;
    gX32Kern_GDT[X32_SEGMENT_KERNEL_LDT].mLimitLow16 = X32_SIZEOF_GDTENTRY * K2OS_MAX_CPU_COUNT;
    gX32Kern_GDT[X32_SEGMENT_KERNEL_LDT].mAttrib = 0x82;  /* Present, DPL 0, system segment type 2 (LDT desc) */
    gX32Kern_GDT[X32_SEGMENT_KERNEL_LDT].mBaseLow16 = (UINT16)(addr & 0xFFFF);
    gX32Kern_GDT[X32_SEGMENT_KERNEL_LDT].mBaseMid8 = (UINT8)((addr >> 16) & 0xFF);
    gX32Kern_GDT[X32_SEGMENT_KERNEL_LDT].mBaseHigh8 = (UINT8)((addr >> 24) & 0xFF);
    addr = (UINT32)&gX32Kern_PerCoreFS[0];
    for(coreIx=0; coreIx < K2OS_MAX_CPU_COUNT; coreIx++)
    {
        gX32Kern_LDT[coreIx].mLimitLow16 = sizeof(UINT32);
        gX32Kern_LDT[coreIx].mBaseLow16 = (UINT16)(addr & 0xFFFF);
        gX32Kern_LDT[coreIx].mBaseMid8 = (UINT8)((addr >> 16) & 0xFF);
        gX32Kern_LDT[coreIx].mAttrib = 0x92; // present DPL 0, nonsystem, writeable
        gX32Kern_LDT[coreIx].mBaseHigh8 = (UINT8)((addr >> 24) & 0xFF);
        addr += sizeof(UINT32);
    }

    /* any other GDT stuff left as null entries (TSS) until per-cores set them up */

    gX32Kern_GDTPTR.mBaseAddr = (UINT32)&gX32Kern_GDT;
    gX32Kern_GDTPTR.mTableSize = (X32_NUM_SEGMENTS * X32_SIZEOF_GDTENTRY) - 1;

    /* install GDT */
    X32Kern_GDTFlush();
}

