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

UINT32 KernArch_MakePTE(UINT32 aPhysAddr, UINT32 aPageMapAttr)
{
    K2OSKERN_PHYSTRACK_PAGE *   pTrack;
    UINT32                      pte;
    KernPhysPageList                onList = KernPhysPageList_Error;

    if (!(aPageMapAttr & K2OS_MEMPAGE_ATTR_DEVICEIO))
    {
        pTrack = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(aPhysAddr);
        K2_ASSERT(!(pTrack->mFlags & K2OSKERN_PHYSTRACK_UNALLOC_FLAG));
        onList = (KernPhysPageList)K2OSKERN_PHYSTRACK_PAGE_FLAGS_GET_LIST(pTrack->mFlags);
    }

    pte = A32_PTE_PRESENT | (aPhysAddr & A32_PTE_PAGEPHYS_MASK);

    if (gA32Kern_IsMulticoreCapable)
        pte |= A32_PTE_SHARED;

    //
    // set EXEC_NEVER if appropriate and possible
    //
    if (0 ==(aPageMapAttr & K2OS_MEMPAGE_ATTR_EXEC))
    {
        // appropriate
        if (pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_XP_CAP)
        {
            // possible
            pte |= A32_PTE_EXEC_NEVER;
        }
    }

    if (aPageMapAttr & K2OS_MEMPAGE_ATTR_DEVICEIO)
    {
        if (onList != KernPhysPageList_DeviceU)
        {
            K2OSKERN_Debug("--DeviceMap unknown 0x%08X (not on phys track list)\n", aPhysAddr);
        }
        if (gA32Kern_IsMulticoreCapable)
            pte |= A32_MMU_PTE_REGIONTYPE_SHAREABLE_DEVICE;
        else
            pte |= A32_MMU_PTE_REGIONTYPE_NONSHAREABLE_DEVICE;
    }
    else if (aPageMapAttr & K2OS_MEMPAGE_ATTR_UNCACHED)
    {
        K2_ASSERT(pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_UC_CAP);
        pte |= A32_MMU_PTE_REGIONTYPE_UNCACHED;
    }
    else if (aPageMapAttr & K2OS_MEMPAGE_ATTR_WRITE_THRU)
    {
        K2_ASSERT(pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_WT_CAP);
        pte |= A32_MMU_PTE_REGIONTYPE_CACHED_WRITETHRU;
    }
    else
    {
        K2_ASSERT(0 != (pTrack->mFlags & K2OSKERN_PHYSTRACK_PROP_WB_CAP));
        pte |= A32_MMU_PTE_REGIONTYPE_CACHED_WRITEBACK;
        if ((K2OS_MEMPAGE_ATTR_KERNEL | K2OS_MEMPAGE_ATTR_PAGING) == (aPageMapAttr & (K2OS_MEMPAGE_ATTR_KERNEL | K2OS_MEMPAGE_ATTR_PAGING)))
        {
            if (aPageMapAttr & K2OS_MEMPAGE_ATTR_TRANSITION)
            {
                K2_ASSERT(onList == KernPhysPageList_Trans);
            }
            else
            {
                K2_ASSERT(onList == KernPhysPageList_Paging);
            }
        }
    }

    if (!(aPageMapAttr & K2OS_MEMPAGE_ATTR_KERNEL))
    {
        //
        // user space PTE
        //
        pte |= A32_PTE_NOT_GLOBAL;
        if (aPageMapAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
            pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_RW;
        else
            pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_RO;
    }
    else
    {
        //
        // kernel space PTE
        //
        if (aPageMapAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
            pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_NONE;
        else
            pte |= A32_MMU_PTE_PERMIT_KERN_RO_USER_NONE;
    }

    return pte;
}

void KernArch_WritePTE(BOOL aIsMake, UINT32 aVirtAddr, UINT32* pPTE, UINT32 aPTE)
{
    BOOL    intState;
    UINT32  transResult;
    UINT32  transResult2;
    UINT32  chk;
    A32_TTBEQUAD* pQuad;

    intState = K2OSKERN_SetIntr(FALSE);

    *pPTE = aPTE;
    A32_DMB();

    if (!aIsMake)
    {
        K2_ASSERT(0 == (aPTE & A32_PTE_PRESENT));

        if (gA32Kern_IsMulticoreCapable)
        {
            A32_TLBInvalidateMVA_MP_AllASID(aVirtAddr);
            A32_BPInvalidateAll_MP();
        }
        else
        {
            A32_TLBInvalidateMVA_UP_AllASID(aVirtAddr);
            A32_BPInvalidateAll_UP();
        }
        A32_ISB();
    }
    else if (0 != (aPTE & A32_PTE_PRESENT))
    {
        //
        // made a present page - test it
        //
        transResult = A32_TranslateVirtPrivRead(aVirtAddr);
        if (transResult & 1)
        {
            K2OSKERN_Debug("FAIL TESTREAD(%08X:%08X)->%08X\n", aVirtAddr, aPTE, transResult);

            K2OSKERN_Debug("External Abort: %c\n", (transResult & 0x40) ? '1' : '0');
            K2OSKERN_Debug("Fail Code:      %02X\n", (transResult & 0x3E) >> 1);

            //
            // first level check
            //
            K2OSKERN_Debug("TTBCR = %08X\n", A32_ReadTTBCR());
            K2OSKERN_Debug("TTBR0 = %08X\n", A32_ReadTTBR0());
            K2OSKERN_Debug("TTBR1 = %08X\n", A32_ReadTTBR1());

            K2OSKERN_Debug("TTB is at virtual %08X\n", K2OS_KVA_TRANSTAB_BASE);
            transResult2 = A32_TranslateVirtPrivRead(K2OS_KVA_TRANSTAB_BASE);
            K2OSKERN_Debug("Trans[%08X] -> %08X - should be same page as TTBRx\n", K2OS_KVA_TRANSTAB_BASE, transResult2);

            pQuad = &((A32_TRANSTBL*)K2OS_KVA_TRANSTAB_BASE)->QuadEntry[aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES];

            K2OSKERN_Debug("TTB Quad @ %08X\n", pQuad);
            K2OSKERN_Debug("Quad[0] = %08X\n", pQuad->Quad[0].mAsUINT32);
            K2OSKERN_Debug("Quad[1] = %08X\n", pQuad->Quad[1].mAsUINT32);
            K2OSKERN_Debug("Quad[2] = %08X\n", pQuad->Quad[2].mAsUINT32);
            K2OSKERN_Debug("Quad[3] = %08X\n", pQuad->Quad[3].mAsUINT32);

            chk = K2OS_KVA_TO_PT_ADDR(aVirtAddr);
            K2OSKERN_Debug("PT is at virtual %08X, pPTE = %08X\n", chk, pPTE);
            transResult2 = A32_TranslateVirtPrivRead(chk);
            K2OSKERN_Debug("Trans[%08X] -> %08X - should be same page as Quad\n", chk, transResult2);

            K2_ASSERT(0);
        }
        else
        {
            K2_ASSERT((aPTE & A32_PTE_PAGEPHYS_MASK) == (transResult & A32_PTE_PAGEPHYS_MASK));
        }
    }

    K2OSKERN_SetIntr(intState);
}

UINT32 * KernArch_Translate(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 *apRetPDE, BOOL *apRetPtPresent, UINT32 *apRetPte, UINT32 *apRetMemPageAttr)
{
    UINT32          transBase;
    A32_TRANSTBL *  pTrans;
    A32_TTBEQUAD *  pQuad;
    UINT32 *        pPTE;
    UINT32          pte;
    UINT32          memProp;
    A32_TTBE        chk;

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

    chk = pQuad->Quad[0];
    *apRetPDE = chk.mAsUINT32;
    if (chk.PTBits.mPresent == 0)
    {
        *apRetPtPresent = FALSE;
        return NULL;
    }
    K2_ASSERT((chk.mAsUINT32 + 0x400) == pQuad->Quad[1].mAsUINT32);
    K2_ASSERT((chk.mAsUINT32 + 0x800) == pQuad->Quad[2].mAsUINT32);
    K2_ASSERT((chk.mAsUINT32 + 0xC00) == pQuad->Quad[3].mAsUINT32);

    *apRetPtPresent = TRUE;
    *apRetMemPageAttr = 0;

    if (aVirtAddr >= K2OS_KVA_KERN_BASE)
    {
        pPTE = ((UINT32*)K2OS_KVA_TO_PTE_ADDR(aVirtAddr));

        *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_KERNEL;
    }
    else
    {
        //
        // this may get called before proc0 is set up. so if we are called with proc 0
        // we just use the kernel va map base as that is the same thing.
        //
        if (apProc == gpProc0)
            pPTE = ((UINT32*)K2OS_KVA_TO_PTE_ADDR(aVirtAddr));
        else
            pPTE = ((UINT32*)K2_VA32_TO_PTE_ADDR(apProc->mVirtMapKVA, aVirtAddr));
    }

    *apRetPte = pte = *pPTE;

    memProp = (A32_MMU_PTE_TEX_111 | A32_PTE_B | A32_PTE_C) & pte;

    if ((memProp == A32_MMU_PTE_REGIONTYPE_SHAREABLE_DEVICE) ||
        (memProp == A32_MMU_PTE_REGIONTYPE_NONSHAREABLE_DEVICE))
        *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_DEVICEIO;
    else
    {
        if (memProp == A32_MMU_PTE_REGIONTYPE_UNCACHED)
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_UNCACHED;
        else if (memProp == A32_MMU_PTE_REGIONTYPE_CACHED_WRITETHRU)
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_WRITE_THRU;
        // else write back
    }

    if ((pte & A32_MMU_PTE_PERMIT_MASK) != A32_MMU_PTE_PERMIT_NONE)
    {
        *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_READABLE;

        if (0 == (pte & A32_PTE_EXEC_NEVER))
        {
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_EXEC;
        }

        pte &= A32_MMU_PTE_PERMIT_MASK;

        if ((pte != A32_MMU_PTE_PERMIT_KERN_RO_USER_NONE) &&
            (pte != A32_MMU_PTE_PERMIT_KERN_RO_USER_RO))
        {
            *apRetMemPageAttr |= K2OS_MEMPAGE_ATTR_WRITEABLE;
        }
    }

    return pPTE;
}

BOOL KernArch_VerifyPteKernHasAccessAttr(UINT32 aPTE, UINT32 aMemPageAttr)
{
    UINT32 chk;

    if (aMemPageAttr & K2OS_MEMPAGE_ATTR_EXEC)
    {
        if (aPTE & A32_PTE_EXEC_NEVER)
        {
            return FALSE;
        }
    }

    chk = aPTE & A32_MMU_PTE_PERMIT_MASK;

    if (aMemPageAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
    {
        if ((chk != A32_MMU_PTE_PERMIT_KERN_RW_USER_NONE) &&
            (chk != A32_MMU_PTE_PERMIT_KERN_RW_USER_RO) &&
            (chk != A32_MMU_PTE_PERMIT_KERN_RW_USER_RW))
            return FALSE;
    }
    else
    {
        if ((chk == A32_MMU_PTE_PERMIT_KERN_RW_USER_NONE) ||
            (chk == A32_MMU_PTE_PERMIT_KERN_RW_USER_RO) ||
            (chk == A32_MMU_PTE_PERMIT_KERN_RW_USER_RW))
            return FALSE;
    }

    chk = aPTE & (A32_PTE_TEX_MASK | A32_PTE_C | A32_PTE_B);

    if (aMemPageAttr & K2OS_MEMPAGE_ATTR_DEVICEIO)
    {
        if ((chk != A32_MMU_PTE_REGIONTYPE_STRONG) &&
            (chk != A32_MMU_PTE_REGIONTYPE_SHAREABLE_DEVICE) &&
            (chk != A32_MMU_PTE_REGIONTYPE_NONSHAREABLE_DEVICE))
            return FALSE;
    }
    else if (aMemPageAttr & K2OS_MEMPAGE_ATTR_UNCACHED)
    {
        if (chk != A32_MMU_PTE_REGIONTYPE_UNCACHED)
            return FALSE;
    }
    else if (aMemPageAttr & K2OS_MEMPAGE_ATTR_WRITE_THRU)
    {
        if (chk != A32_MMU_PTE_REGIONTYPE_CACHED_WRITETHRU)
            return FALSE;

    }
    else
    {
        if (chk != A32_MMU_PTE_REGIONTYPE_CACHED_WRITEBACK)
            return FALSE;
    }

    return TRUE;
}

void KernArch_BreakMapTransitionPageTable(UINT32 *apRetVirtAddrPT, UINT32 *apRetPhysAddrPT)
{
    UINT32          virtAddrPT;
    A32_TRANSTBL *  pTrans;
    A32_TTBEQUAD *  pQuad;
    UINT32 *        pPTE;
    UINT32          pdePTAddr;

    pTrans = (A32_TRANSTBL *)K2OS_KVA_TRANSTAB_BASE;

    pQuad = &pTrans->QuadEntry[gData.mpShared->LoadInfo.mTransitionPageAddr / K2_VA32_PAGETABLE_MAP_BYTES];

    K2_ASSERT(pQuad->Quad[0].PTBits.mPresent != 0);

    pdePTAddr = pQuad->Quad[0].mAsUINT32 & K2_VA32_PAGEFRAME_MASK;

    pQuad->Quad[0].mAsUINT32 = 0;
    pQuad->Quad[1].mAsUINT32 = 0;
    pQuad->Quad[2].mAsUINT32 = 0;
    pQuad->Quad[3].mAsUINT32 = 0;

    *apRetVirtAddrPT = virtAddrPT = K2OS_KVA_TO_PT_ADDR(gData.mpShared->LoadInfo.mTransitionPageAddr);

    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(virtAddrPT);
    *apRetPhysAddrPT = (*pPTE) & K2_VA32_PAGEFRAME_MASK;
    K2_ASSERT((*apRetPhysAddrPT) == pdePTAddr);
    *pPTE = 0;

    K2_CpuWriteBarrier();
}

void KernArch_InvalidateTlbPageOnThisCore(UINT32 aVirtAddr)
{
    BOOL intState;

    intState = K2OSKERN_SetIntr(FALSE);

    if (gA32Kern_IsMulticoreCapable)
    {
        A32_TLBInvalidateMVA_MP_AllASID(aVirtAddr);
    }
    else
    {
        A32_TLBInvalidateMVA_UP_AllASID(aVirtAddr);
    }
    A32_ISB();

    K2OSKERN_SetIntr(intState);
}

