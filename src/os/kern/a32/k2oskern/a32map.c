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

UINT32 KernArch_MakePTE(UINT32 aPhysAddr, UINTN  aMapType)
{
    K2OSKERN_PHYSTRACK_PAGE *   pTrack;
    UINT32                      pte;
    KernPageList                onList = KernPageList_Error;

    if (aMapType != K2OS_VIRTMAPTYPE_DEVICE)
    {
        pTrack = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(aPhysAddr);
        K2_ASSERT(!(pTrack->mFlags & K2OSKERN_PHYSTRACK_FREE_FLAG));
        onList = (KernPageList)K2OSKERN_PHYSTRACK_PAGE_FLAGS_GET_LIST(pTrack->mFlags);
    }

    K2_ASSERT(aMapType < K2OS_VIRTMAPTYPE_COUNT);

    if ((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) == K2OS_VIRTMAPTYPE_STACK_GUARD)
        return K2OSKERN_PTE_NP_STACK_GUARD;

    pte = A32_PTE_PRESENT | aPhysAddr;

    if (gA32Kern_IsMulticoreCapable)
        pte |= A32_PTE_SHARED;

    //
    // set EXEC_NEVER if appropriate and possible
    //
    if (((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) != K2OS_VIRTMAPTYPE_TEXT) &&
        (aMapType != K2OS_VIRTMAPTYPE_KERN_TRANSITION))
    {
        // appropriate
        if (pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_XP_CAP)
        {
            // possible
            pte |= A32_PTE_EXEC_NEVER;
        }
    }

    if ((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) == K2OS_VIRTMAPTYPE_DEVICE)
    {
        if (onList != KernPageList_DeviceU)
        {
            K2OSKERN_Debug("--DeviceMap unknown 0x%08X (not on phys track list)\n", aPhysAddr);
        }
        if (gA32Kern_IsMulticoreCapable)
            pte |= A32_MMU_PTE_REGIONTYPE_SHAREABLE_DEVICE;
        else
            pte |= A32_MMU_PTE_REGIONTYPE_NONSHAREABLE_DEVICE;
    }
    else if ((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) == K2OS_VIRTMAPTYPE_UNCACHED)
    {
        K2_ASSERT(pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_UC_CAP);
        pte |= A32_MMU_PTE_REGIONTYPE_UNCACHED;
    }
    else if ((aMapType & K2OS_VIRTMAPTYPE_SUBTYPE_MASK) == K2OS_VIRTMAPTYPE_WRITETHRU_CACHED)
    {
        K2_ASSERT(onList == KernPageList_DeviceC);
        K2_ASSERT(pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_WT_CAP);
        pte |= A32_MMU_PTE_REGIONTYPE_CACHED_WRITETHRU;
    }
    else
    {
        if (aMapType & K2OS_VIRTMAPTYPE_KERN_BIT)
        {
            if ((aMapType == K2OS_VIRTMAPTYPE_KERN_TRANSITION) ||
                (aMapType == K2OS_VIRTMAPTYPE_KERN_PAGETABLE))
            {
                if (aMapType == K2OS_VIRTMAPTYPE_KERN_TRANSITION)
                {
                    K2_ASSERT(onList == KernPageList_Trans);
                }
                else
                {
                    K2_ASSERT(onList == KernPageList_Paging);
                }
                if (pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_WT_CAP)
                    pte |= A32_MMU_PTE_REGIONTYPE_CACHED_WRITETHRU;
                else
                {
                    K2_ASSERT(pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_UC_CAP);
                    pte |= A32_MMU_PTE_REGIONTYPE_UNCACHED;
                }
            }
            else
            {
                K2_ASSERT(pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_WB_CAP);
                pte |= A32_MMU_PTE_REGIONTYPE_CACHED_WRITEBACK;
            }
        }
        else
        {
            K2_ASSERT(pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_WB_CAP);
            pte |= A32_MMU_PTE_REGIONTYPE_CACHED_WRITEBACK;
        }
    }

    if (!(aMapType & K2OS_VIRTMAPTYPE_KERN_BIT))
    {
        //
        // user space PTE
        //
        pte |= A32_PTE_NOT_GLOBAL;
        if (aMapType & K2OS_VIRTMAPTYPE_WRITEABLE_BIT)
            pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_RW;
        else
            pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_RO;
    }
    else
    {
        //
        // kernel space PTE
        //
        if ((aMapType & K2OS_VIRTMAPTYPE_WRITEABLE_BIT) ||
            (aMapType == K2OS_VIRTMAPTYPE_KERN_TRANSITION) ||
            (aMapType == K2OS_VIRTMAPTYPE_KERN_PAGETABLE))
            pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_NONE;
        else
            pte |= A32_MMU_PTE_PERMIT_KERN_RO_USER_NONE;
    }

    return pte;
}

void KernArch_Translate(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, BOOL *apRetPtPresent, UINT32 *apRetPte, UINT32 *apRetAccessAttr)
{
    UINT32          transBase;
    A32_TRANSTBL *  pTrans;
    A32_TTBEQUAD *  pQuad;
    UINT32          pte;

    K2_ASSERT(apProc != NULL);

    //
    // this may get called before proc0 is set up. so if we are called with proc 0
    // we just use the transtab base as that is the same thing.
    //
    if (apProc == gpProc0)
        transBase = K2OS_KVA_TRANSTAB_BASE;
    else
        transBase = (apProc->mTransTableKVA & K2_VA32_PAGEFRAME_MASK);

    pTrans = (A32_TRANSTBL *)transBase;

    pQuad = &pTrans->QuadEntry[aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES];

    if (pQuad->Quad[0].PTBits.mPresent == 0)
    {
        *apRetPtPresent = FALSE;
        return;
    }

    *apRetPtPresent = TRUE;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        *apRetPte = pte = *((UINT32*)K2OS_KVA_TO_PTE_ADDR(aVirtAddr));

        pte &= A32_MMU_PTE_PERMIT_MASK;
        if ((pte == A32_MMU_PTE_PERMIT_KERN_RW_USER_NONE) ||
            (pte == A32_MMU_PTE_PERMIT_KERN_RW_USER_RO) ||
            (pte == A32_MMU_PTE_PERMIT_KERN_RW_USER_RW))
        {
            if (pte & A32_PTE_EXEC_NEVER)
            {
                *apRetAccessAttr = K2OS_ACCESS_ATTR_DATA;
            }
            else
            {
                *apRetAccessAttr = K2OS_ACCESS_ATTR_ANY;
            }
        }
        else if ((pte == A32_MMU_PTE_PERMIT_KERN_RO_USER_NONE) ||
                 (pte == A32_MMU_PTE_PERMIT_KERN_RO_USER_RO))
        {
            if (pte & A32_PTE_EXEC_NEVER)
            {
                *apRetAccessAttr = K2OS_ACCESS_ATTR_READ;
            }
            else
            {
                *apRetAccessAttr = K2OS_ACCESS_ATTR_TEXT;
            }
        }
        else
            *apRetAccessAttr = 0;
    }
    else
    {
        //
        // this may get called before proc0 is set up. so if we are called with proc 0
        // we just use the kernel va map base as that is the same thing.
        //
        if (apProc == gpProc0)
            pte = *((UINT32*)K2OS_KVA_TO_PTE_ADDR(aVirtAddr));
        else
            pte = *((UINT32*)K2_VA32_TO_PTE_ADDR(apProc->mVirtMapKVA, aVirtAddr));
        
        *apRetPte = pte;
        pte &= A32_MMU_PTE_PERMIT_MASK;

        if (pte == A32_MMU_PTE_PERMIT_KERN_RW_USER_RW)
        {
            if (pte & A32_PTE_EXEC_NEVER)
            {
                *apRetAccessAttr = K2OS_ACCESS_ATTR_DATA;
            }
            else
            {
                *apRetAccessAttr = K2OS_ACCESS_ATTR_ANY;
            }
        }
        else if ((pte == A32_MMU_PTE_PERMIT_KERN_RW_USER_RO) ||
                 (pte == A32_MMU_PTE_PERMIT_KERN_RO_USER_RO))
        {
            if (pte & A32_PTE_EXEC_NEVER)
            {
                *apRetAccessAttr = K2OS_ACCESS_ATTR_READ;
            }
            else
            {
                *apRetAccessAttr = K2OS_ACCESS_ATTR_TEXT;
            }
        }
        else
            *apRetAccessAttr = 0;
    }
}

BOOL KernArch_VerifyPteKernAccessAttr(UINT32 aPTE, UINT32 aAttr)
{
    BOOL flag;

    aPTE &= A32_MMU_PTE_PERMIT_MASK;

    flag = ((aPTE == A32_MMU_PTE_PERMIT_KERN_RW_USER_NONE) ||
        (aPTE == A32_MMU_PTE_PERMIT_KERN_RW_USER_RO) ||
        (aPTE == A32_MMU_PTE_PERMIT_KERN_RW_USER_RW));

    if (aAttr & K2OS_ACCESS_ATTR_W)
    {
        // pte must be writeable
        return flag;
    }

    // must not be writeable
    return !flag;
}

void KernArch_MapPageTable(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 aPhysAddrPT)
{
    K2_ASSERT(0);
}

void KernArch_BreakMapPageTable(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 *apRetVirtAddrPT, UINT32 *apRetPhysAddrPT)
{
    UINT32          virtAddrPT;
    UINT32          transBase;
    A32_TRANSTBL *  pTrans;
    A32_TTBEQUAD *  pQuad;
    UINT32 *        pPTE;
    UINT32          pdePTAddr;

    K2_ASSERT(apProc != NULL);

    //
    // this may get called before proc0 is set up. so if we are called with proc 0
    // we just use the transtab base as that is the same thing.
    //
    if (apProc == gpProc0)
        transBase = K2OS_KVA_TRANSTAB_BASE;
    else
        transBase = (apProc->mTransTableKVA & K2_VA32_PAGEFRAME_MASK);

    pTrans = (A32_TRANSTBL *)transBase;

    pQuad = &pTrans->QuadEntry[aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES];

    K2_ASSERT(pQuad->Quad[0].PTBits.mPresent != 0);

    pdePTAddr = pQuad->Quad[0].mAsUINT32 & K2_VA32_PAGEFRAME_MASK;

    pQuad->Quad[0].mAsUINT32 = 0;
    pQuad->Quad[1].mAsUINT32 = 0;
    pQuad->Quad[2].mAsUINT32 = 0;
    pQuad->Quad[3].mAsUINT32 = 0;

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
    if (gA32Kern_IsMulticoreCapable)
        A32_TLBInvalidateMVA_MP_AllASID(aVirtAddr);
    else
        A32_TLBInvalidateMVA_UP_AllASID(aVirtAddr);
    A32_ISB();
}

