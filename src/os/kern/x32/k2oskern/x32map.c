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

UINT32 KernArch_MakePTE(UINT32 aPhysAddr, UINTN  aMapType)
{
    K2OSKERN_PHYSTRACK_PAGE *   pTrack;
    UINT32                      pte;

    pTrack = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(aPhysAddr);
    K2_ASSERT(!(pTrack->mFlags & K2OSKERN_PHYSTRACK_FREE_FLAG));
    K2_ASSERT(aMapType < K2OS_VIRTMAPTYPE_COUNT);

    if ((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) == K2OS_VIRTMAPTYPE_STACK_GUARD)
    {
        pte = K2OSKERN_PTE_NP_STACK_GUARD;
    }
    else
    {
        pte = X32_PTE_PRESENT | aPhysAddr;

        if (aMapType & K2OS_VIRTMAPTYPE_WRITEABLE_BIT)
            pte |= X32_PTE_WRITEABLE;

        if (((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) == K2OS_VIRTMAPTYPE_DEVICE) ||
            ((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) == K2OS_VIRTMAPTYPE_UNCACHED))
        {
            pte |= (X32_PTE_WRITETHROUGH | X32_PTE_CACHEDISABLE);
        }
        else if ((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) == K2OS_VIRTMAPTYPE_WRITETHRU_CACHED)
        {
            pte |= X32_PTE_WRITETHROUGH;
        }

        if (!(aMapType & K2OS_VIRTMAPTYPE_KERN_BIT))
            pte |= X32_PTE_USER;
        else
        {
            pte |= X32_PTE_GLOBAL;
            if (aMapType == K2OS_VIRTMAPTYPE_KERN_TRANSITION)
            {
                pte |= (X32_PTE_WRITEABLE | X32_PTE_WRITETHROUGH);
            }
            else if (aMapType == K2OS_VIRTMAPTYPE_KERN_PAGETABLE)
            {
                //
                // if pagetable is in memory that can be writethrough
                // then use that.  otherwise use cache-disabled memory
                //
                if (pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_WT_CAP)
                    pte |= (X32_PTE_WRITEABLE | X32_PTE_WRITETHROUGH);
                else
                    pte |= (X32_PTE_WRITEABLE | X32_PTE_CACHEDISABLE);
            }
        }
    }

    return pte;
}

void KernArch_Translate(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, BOOL *apRetPtPresent, UINT32 *apRetPte, UINT32 *apRetAccessAttr)
{
    UINT32      transBase;
    UINT32 *    pPDE;

    K2_ASSERT(apProc != NULL);

    //
    // this may get called before proc0 is set up. so if we are called with proc 0
    // we just use the transtab base as that is the same thing.
    //
    if (apProc == gpProc0)
        transBase = K2OS_KVA_TRANSTAB_BASE;
    else
        transBase = (apProc->mTransTableKVA & K2_VA32_PAGEFRAME_MASK);

    pPDE = ((UINT32 *)transBase) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);

    if (!((*pPDE) & X32_PDE_PRESENT))
    {
        *apRetPtPresent = FALSE;
        return;
    }

    *apRetPtPresent = TRUE;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        *apRetPte = *((UINT32*)K2OS_KVA_TO_PTE_ADDR(aVirtAddr));
    }
    else
    {
        //
        // this may get called before proc0 is set up. so if we are called with proc 0
        // we just use the kernel va map base as that is the same thing.
        //
        if (apProc == gpProc0)
            *apRetPte = *((UINT32*)K2OS_KVA_TO_PTE_ADDR(aVirtAddr));
        else
            *apRetPte = *((UINT32*)K2_VA32_TO_PTE_ADDR(apProc->mVirtMapKVA, aVirtAddr));
    }

    if ((*apRetPte) & X32_PTE_WRITEABLE)
    {
        *apRetAccessAttr = K2OS_ACCESS_ATTR_ANY;
    }
    else
    {
        *apRetAccessAttr = K2OS_ACCESS_ATTR_TEXT;
    }
}

BOOL KernArch_VerifyPteKernAccessAttr(UINT32 aPTE, UINT32 aAttr)
{
    if (aAttr & K2OS_ACCESS_ATTR_W)
    {
        return (aPTE & X32_PTE_WRITEABLE) ? TRUE : FALSE;
    }
    // must not be writeable
    return (aPTE & X32_PTE_WRITEABLE) ? FALSE : TRUE;
}

void KernArch_MapPageTable(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 aPhysAddrPT)
{




    K2_ASSERT(0);
}

void KernArch_BreakMapPageTable(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 *apRetVirtAddrPT, UINT32 *apRetPhysAddrPT)
{
    UINT32      virtAddrPT;
    UINT32      transBase;
    UINT32 *    pPDE;
    UINT32 *    pPTE;
    UINT32      pdePTAddr;

    K2_ASSERT(apProc != NULL);

    //
    // this may get called before proc0 is set up. so if we are called with proc 0
    // we just use the transtab base as that is the same thing.
    //
    if (apProc == gpProc0)
        transBase = K2OS_KVA_TRANSTAB_BASE;
    else
        transBase = (apProc->mTransTableKVA & K2_VA32_PAGEFRAME_MASK);

    pPDE = ((UINT32 *)transBase) + (aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES);
    pdePTAddr = (*pPDE) & K2_VA32_PAGEFRAME_MASK;
    *pPDE = 0;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        virtAddrPT = K2OS_KVA_TO_PT_ADDR(aVirtAddr);
    }
    else
    {
        //
        // this may get called before proc0 is set up. so if we are called with proc 0
        // we just use the kernel va map base as that is the same thing.
        //
        if (apProc == gpProc0)
            virtAddrPT = K2_VA32_TO_PT_ADDR(K2OS_KVA_KERNVAMAP_BASE, aVirtAddr);
        else
            virtAddrPT = K2_VA32_TO_PT_ADDR(apProc->mVirtMapKVA, aVirtAddr);
    }

    K2_ASSERT(virtAddrPT >= K2OS_KVA_KERN_BASE);
    *apRetVirtAddrPT = virtAddrPT;

    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddrPT);
    *apRetPhysAddrPT = (*pPTE) & K2_VA32_PAGEFRAME_MASK;
    K2_ASSERT((*apRetPhysAddrPT) == pdePTAddr);
    *pPTE = 0;

    K2_CpuWriteBarrier();
}

void KernArch_InvalidateTlbPageOnThisCore(UINT32 aVirtAddr)
{
    X32_TLBInvalidatePage(aVirtAddr);
}

