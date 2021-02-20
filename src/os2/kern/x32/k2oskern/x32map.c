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

UINT32 
KernArch_MakePTE(
    UINT32 aPhysAddr, 
    UINT32 aPageMapAttr
)
{
    K2OSKERN_PHYSTRACK_PAGE *   pTrack;
    UINT32                      pte;

    pTrack = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(aPhysAddr);
    K2_ASSERT(0 == (pTrack->mFlags & K2OSKERN_PHYSTRACK_UNALLOC_FLAG));

    pte = X32_PTE_PRESENT | aPhysAddr;

    if (aPageMapAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
        pte |= X32_PTE_WRITEABLE;

    if (aPageMapAttr & K2OS_MEMPAGE_ATTR_UNCACHED)
        pte |= X32_PTE_CACHEDISABLE;

    if (aPageMapAttr & K2OS_MEMPAGE_ATTR_WRITE_THRU)
        pte |= X32_PTE_WRITETHROUGH;

    if (!(aPageMapAttr & K2OS_MEMPAGE_ATTR_KERNEL))
        pte |= X32_PTE_USER;
    else
        pte |= X32_PTE_GLOBAL;

    return pte;
}

void 
KernArch_BreakMapTransitionPageTable(
    UINT32 *apRetVirtAddrPT, 
    UINT32 *apRetPhysAddrPT
)
{
    UINT32      virtAddrPT;
    UINT32 *    pPDE;
    UINT32 *    pPTE;
    UINT32      pdePTAddr;

    pPDE = ((UINT32 *)K2OS_KVA_TRANSTAB_BASE) + (gData.LoadInfo.mTransitionPageAddr / K2_VA32_PAGETABLE_MAP_BYTES);
    pdePTAddr = (*pPDE) & K2_VA32_PAGEFRAME_MASK;
    *pPDE = 0;

    *apRetVirtAddrPT = virtAddrPT = K2OS_KVA_TO_PT_ADDR(gData.LoadInfo.mTransitionPageAddr);

    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddrPT);
    *apRetPhysAddrPT = (*pPTE) & K2_VA32_PAGEFRAME_MASK;
    K2_ASSERT((*apRetPhysAddrPT) == pdePTAddr);
    *pPTE = 0;

    K2_CpuWriteBarrier();
}

void 
KernArch_InvalidateTlbPageOnThisCore(
    UINT32 aVirtAddr
)
{
    X32_TLBInvalidatePage(aVirtAddr);
}

