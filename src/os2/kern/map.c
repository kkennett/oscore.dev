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

#include "kern.h"

void    
KernMap_MakeOnePresentPage(
    K2OSKERN_OBJ_PROCESS *apProc,
    UINT32 aVirtAddr, 
    UINT32 aPhysAddr, 
    UINT32 aPageMapAttr
)
{
    UINT32 * pPTE;
    UINT32 * pPageCount;
    UINT32   pteOld;
    UINT32   ptIndex;

    ptIndex = aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        K2_ASSERT(apProc == gpProc1);
        //
        // not allowed to map directly to mirrored pagetable
        //
        K2_ASSERT(ptIndex != (K2OS_KVA_THREAD_TLS_BASE / K2_VA32_PAGETABLE_MAP_BYTES));
    }

    aPageMapAttr &= K2OS_MEMPAGE_ATTR_MASK;

    pPTE = (UINT32*)K2_VA32_TO_PTE_ADDR(apProc->mVirtMapKVA, aVirtAddr);

    pteOld = *pPTE;

    K2_ASSERT((pteOld & K2OSKERN_PTE_PRESENT_BIT) == 0);

    *pPTE = KernArch_MakePTE(aPhysAddr, aPageMapAttr);
//    K2OSKERN_Debug("%08X MAKE %08X -> pte(%08X)\n", pPTE, aVirtAddr, *pPTE);

    if (0 == (pteOld & K2OSKERN_PTE_NP_BIT))
    {
        pPageCount = (UINT32 *)(((UINT8 *)apProc) + (K2_VA32_MEMPAGE_BYTES * K2OS_PROC_PAGES_OFFSET_PAGECOUNT));
        pPageCount[ptIndex]++;
        K2_ASSERT(pPageCount[ptIndex] <= 1024);

        //
        // check for specifically mirrored pagetable
        //
        if (ptIndex == 0)
        {
            pPageCount[K2OS_KVA_THREAD_TLS_BASE / K2_VA32_PAGETABLE_MAP_BYTES]++;
            K2_ASSERT(pPageCount[K2OS_KVA_THREAD_TLS_BASE / K2_VA32_PAGETABLE_MAP_BYTES] <= 1024);
        }
    }

    K2_CpuWriteBarrier();
}

UINT32 
KernMap_BreakOnePage(
    K2OSKERN_OBJ_PROCESS *apProc,
    UINT32 aVirtAddr, 
    UINT32 aNpFlags
)
{
    UINT32 * pPTE;
    UINT32 * pPageCount;
    UINT32   result;
    UINT32   pteOld;
    UINT32   ptIndex;

    ptIndex = aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        K2_ASSERT(apProc == gpProc1);
    }

    pPTE = (UINT32*)K2_VA32_TO_PTE_ADDR(apProc->mVirtMapKVA, aVirtAddr);

    pteOld = *pPTE;

    K2_ASSERT((pteOld & (K2OSKERN_PTE_PRESENT_BIT | K2OSKERN_PTE_NP_BIT)) != 0);

    result = (pteOld & K2_VA32_PAGEFRAME_MASK);

    if (aNpFlags & K2OSKERN_PTE_NP_BIT)
    {
        K2_ASSERT(0 == (aNpFlags & K2OSKERN_PTE_PRESENT_BIT));

        *pPTE = aNpFlags;
    }
    else
    {
        *pPTE = 0;

        pPageCount = (UINT32 *)(((UINT8 *)apProc) + (K2_VA32_MEMPAGE_BYTES * K2OS_PROC_PAGES_OFFSET_PAGECOUNT));
        K2_ASSERT(pPageCount[ptIndex] > 0);
        pPageCount[ptIndex]--;

        //
        // check for specifically mirrored pagetable
        //
        if (ptIndex == 0)
        {
            K2_ASSERT(pPageCount[K2OS_KVA_THREAD_TLS_BASE / K2_VA32_PAGETABLE_MAP_BYTES] > 0);
            pPageCount[K2OS_KVA_THREAD_TLS_BASE / K2_VA32_PAGETABLE_MAP_BYTES]--;
        }
    }
//    K2OSKERN_Debug("%08X xxxx %08X -> pte(%08X)\n", pPTE, aVirtAddr, *pPTE);

    K2_CpuWriteBarrier();

    return result;
}

void    
KernMap_FlushTlb(
    K2OSKERN_OBJ_PROCESS *  apProc,
    UINT32                  aVirtAddr,
    UINT32                  aPageCount
)
{
    //
    // flush this range accross all cores for the specified process
    //
    K2_ASSERT(0);
}
