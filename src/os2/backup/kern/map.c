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

UINT32 * sGetPTE(UINT32 aVirtMapBase, UINT32 aVirtAddr)
{
    UINT32* pPTE;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        K2_ASSERT(aVirtMapBase == K2OS_KVA_KERNVAMAP_BASE);
        pPTE = (UINT32*)K2OS_KVA_TO_PTE_ADDR(aVirtAddr);
    }
    else
    {
        K2_ASSERT(aVirtMapBase != K2OS_KVA_KERNVAMAP_BASE);
        pPTE = (UINT32*)K2_VA32_TO_PTE_ADDR(aVirtMapBase, aVirtAddr);
    }

    return pPTE;
}

void KernMap_MakeOnePresentPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aPhysAddr, UINT32 aPageMapAttr)
{
    UINT32 *    pPTE;
    UINT32 *    pPageCount;
    UINT32      pteOld;
    UINT32      pteNew;

    aPageMapAttr &= K2OS_MEMPAGE_ATTR_MASK;

    pPTE = sGetPTE(aVirtMapBase, aVirtAddr);
    pteOld = *pPTE;

    K2_ASSERT((pteOld & K2OSKERN_PTE_PRESENT_BIT) == 0);

    pteNew = KernArch_MakePTE(aPhysAddr, aPageMapAttr);

    KernArch_WritePTE(TRUE, aVirtAddr, pPTE, pteNew);

    if (0 == (pteOld & K2OSKERN_PTE_NP_BIT))
    {
        if (aVirtAddr >= K2OS_KVA_KERN_BASE)
        {
            pPageCount = ((UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);
        }
        else
        {
            K2_ASSERT(0);
        }
        (*pPageCount)++;
        K2_ASSERT((*pPageCount) <= 1024);
    }

    K2_CpuWriteBarrier();
}

void KernMap_MakeOneNotPresentPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aNpFlags, UINT32 aContent)
{
    UINT32 *    pPTE;
    UINT32 *    pPageCount;
    UINT32      pteOld;
    UINT32      pteNew;

    K2_ASSERT((aNpFlags & K2OSKERN_PTE_NP_CONTENT_MASK) == 0);
    K2_ASSERT((aContent & ~K2OSKERN_PTE_NP_CONTENT_MASK) == 0);
    K2_ASSERT(aNpFlags & K2OSKERN_PTE_NP_BIT);

    pPTE = sGetPTE(aVirtMapBase, aVirtAddr);
    pteOld = *pPTE;

    K2_ASSERT((pteOld & (K2OSKERN_PTE_PRESENT_BIT | K2OSKERN_PTE_NP_BIT)) == 0);

    pteNew = aNpFlags | aContent;

    KernArch_WritePTE(TRUE, aVirtAddr, pPTE, pteNew);

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        pPageCount = ((UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);
    }
    else
    {
        K2_ASSERT(0);
    }
    (*pPageCount)++;
    K2_ASSERT((*pPageCount) <= 1024);

    K2_CpuWriteBarrier();
}

UINT32 KernMap_BreakOnePage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aNpFlags)
{
    UINT32 *    pPTE;
    UINT32 *    pPageCount;
    UINT32      result;
    UINT32      pteOld;

    pPTE = sGetPTE(aVirtMapBase, aVirtAddr);
    pteOld = *pPTE;

    K2_ASSERT((pteOld & (K2OSKERN_PTE_PRESENT_BIT | K2OSKERN_PTE_NP_BIT)) != 0);

    result = ((*pPTE) & K2_VA32_PAGEFRAME_MASK);

    if (aNpFlags & K2OSKERN_PTE_NP_BIT)
    {
        K2_ASSERT(0 == (aNpFlags & K2OSKERN_PTE_PRESENT_BIT));
        
        KernArch_WritePTE(FALSE, aVirtAddr, pPTE, aNpFlags);
    }
    else
    {
        KernArch_WritePTE(FALSE, aVirtAddr, pPTE, 0);

        if (aVirtAddr >= K2OS_KVA_KERN_BASE)
        {
            pPageCount = ((UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);
        }
        else
        {
            K2_ASSERT(0);
        }
        K2_ASSERT((*pPageCount) > 0);
        (*pPageCount)--;
    }

    K2_CpuWriteBarrier();

    return result;
}
