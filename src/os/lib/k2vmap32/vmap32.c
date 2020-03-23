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

#include <lib/k2vmap32.h>
#include <lib/k2mem.h>

#if K2_TARGET_ARCH_IS_ARM

UINT32
K2VMAP32_ReadPDE(
    K2VMAP32_CONTEXT *  apContext,
    UINT32              aIndex
    )
{
    return *(((UINT32 *)apContext->mTransBasePhys) + ((aIndex & 0x3FF) * 4));
}

void
K2VMAP32_WritePDE(
    K2VMAP32_CONTEXT *  apContext,
    UINT32              aIndex,
    UINT32              aPDE
    )
{
    UINT32 *pPDE;

    if (apContext->mFlags & K2VMAP32_FLAG_REALIZED)
        return;

    pPDE = (((UINT32 *)apContext->mTransBasePhys) + ((aIndex & 0x3FF) * 4));
    pPDE[0] = aPDE;
    pPDE[1] = (aPDE + 0x400);
    pPDE[2] = (aPDE + 0x800);
    pPDE[3] = (aPDE + 0xC00);
}

#else

UINT32
K2VMAP32_ReadPDE(
    K2VMAP32_CONTEXT *  apContext,
    UINT32              aIndex
    )
{
    return *(((UINT32 *)apContext->mTransBasePhys) + (aIndex & 0x3FF));
}

void
K2VMAP32_WritePDE(
    K2VMAP32_CONTEXT *  apContext,
    UINT32              aIndex,
    UINT32              aPDE
    )
{
    if (apContext->mFlags & K2VMAP32_FLAG_REALIZED)
        return;
    *(((UINT32 *)apContext->mTransBasePhys) + (aIndex & 0x3FF)) = aPDE;
}

#endif

K2STAT
K2VMAP32_Init(
    K2VMAP32_CONTEXT *  apContext,
    UINT32              aVirtMapBase,
    UINT32              aTransBasePhys,
    UINT32              aTransBaseVirt,
    BOOL                aIsMultiproc,
    pfK2VMAP32_PT_Alloc afPT_Alloc,
    pfK2VMAP32_PT_Free  afPT_Free
    )
{
    UINT32  virtWork;
    UINT32  physWork;
    UINT32  physEnd;
    K2STAT  status;

    if ((aTransBasePhys & (K2_VA32_TRANSTAB_SIZE - 1)) != 0)
        return K2STAT_ERROR_BAD_ARGUMENT;
    if ((aTransBaseVirt & K2_VA32_MEMPAGE_OFFSET_MASK) != 0)
        return K2STAT_ERROR_BAD_ARGUMENT;
    if ((aVirtMapBase & K2_VA32_MEMPAGE_OFFSET_MASK) != 0)
        return K2STAT_ERROR_BAD_ARGUMENT;

    if ((afPT_Alloc == NULL) ||
        (afPT_Free == NULL))
        return K2STAT_ERROR_BAD_ARGUMENT;

    apContext->mTransBasePhys = aTransBasePhys;
    apContext->mVirtMapBase = aVirtMapBase;
    if (aIsMultiproc)
        apContext->mFlags = K2VMAP32_FLAG_MULTIPROC;
    else
        apContext->mFlags = 0;
    apContext->PT_Alloc = afPT_Alloc;
    apContext->PT_Free = afPT_Free;

    virtWork = aTransBaseVirt;
    physWork = aTransBasePhys;
    physEnd = physWork + K2_VA32_TRANSTAB_SIZE;
    do
    {
        status = K2VMAP32_MapPage(apContext, virtWork, physWork, K2OS_MAPTYPE_KERN_PAGEDIR);
        if (K2STAT_IS_ERROR(status))
            return status;
        virtWork += K2_VA32_MEMPAGE_BYTES;
        physWork += K2_VA32_MEMPAGE_BYTES;
    } while (physWork != physEnd);

    return 0;
}

UINT32      
K2VMAP32_VirtToPTE(
    K2VMAP32_CONTEXT *  apContext,
    UINT32              aVirtAddr,
    BOOL *              apRetFaultPDE,
    BOOL *              apRetFaultPTE
    )
{
    UINT32  entry;
    UINT32  ixEntry;

    *apRetFaultPDE = TRUE;
    *apRetFaultPTE = TRUE;

    if (apContext->mFlags & K2VMAP32_FLAG_REALIZED)
        return 0;

    ixEntry = aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES;
    entry = K2VMAP32_ReadPDE(apContext, ixEntry);
    if ((entry & K2VMAP32_FLAG_PRESENT) == 0)
        return 0;

    *apRetFaultPDE = FALSE;

    ixEntry = (aVirtAddr / K2_VA32_MEMPAGE_BYTES) & ((1 << K2_VA32_PAGETABLE_PAGES_POW2) - 1);
    entry = *((UINT32 *)((entry & K2VMAP32_PAGEPHYS_MASK) + (ixEntry * sizeof(UINT32))));
    if ((entry & K2VMAP32_FLAG_PRESENT) == 0)
        return 0;

    *apRetFaultPTE = FALSE;

    return entry;
}

K2STAT 
K2VMAP32_MapPage(
    K2VMAP32_CONTEXT *  apContext,
    UINT32              aVirtAddr,
    UINT32              aPhysAddr,
    UINT32              aMapType
    )
{
    UINT32      entry;
    UINT32      ixEntry;
    UINT32      virtPT;
    UINT32      physPT;
    UINT32 *    pPTE;
    K2STAT      status;

    if (apContext->mFlags & K2VMAP32_FLAG_REALIZED)
        return K2STAT_ERROR_API_ORDER;

    if ((aMapType & K2OS_MEMPAGE_ATTR_MASK) == K2OS_MEMPAGE_ATTR_NONE)
        return K2STAT_ERROR_BAD_ARGUMENT;

    ixEntry = aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES;
    entry = K2VMAP32_ReadPDE(apContext, ixEntry);
    if ((entry & K2VMAP32_FLAG_PRESENT) == 0)
    {
        virtPT = K2_VA32_TO_PT_ADDR(apContext->mVirtMapBase, aVirtAddr);
        if (virtPT != aVirtAddr)
        {
            status = apContext->PT_Alloc(&physPT);
            if (K2STAT_IS_ERROR(status))
                return status;

            K2MEM_Zero((void *)physPT, K2_VA32_MEMPAGE_BYTES);

            status = K2VMAP32_MapPage(apContext, virtPT, physPT, K2OS_MAPTYPE_KERN_PAGETABLE);
            if (K2STAT_IS_ERROR(status))
            {
                apContext->PT_Free(physPT);
                return status;
            }
        }
        else
            physPT = aPhysAddr;

        entry = physPT | K2VMAP32_FLAG_PRESENT;

        K2VMAP32_WritePDE(apContext, ixEntry, entry);
    }

    ixEntry = (aVirtAddr / K2_VA32_MEMPAGE_BYTES) & ((1 << K2_VA32_PAGETABLE_PAGES_POW2) - 1);

    pPTE = (UINT32 *)((entry & K2VMAP32_PAGEPHYS_MASK) + (ixEntry * sizeof(UINT32)));
    if (((*pPTE) & K2VMAP32_FLAG_PRESENT) != 0)
        return K2STAT_ERROR_BAD_ARGUMENT;

    *pPTE = (aPhysAddr & K2VMAP32_PAGEPHYS_MASK) | aMapType | K2VMAP32_FLAG_PRESENT;

    return 0;
}

K2STAT
K2VMAP32_VerifyOrMapPageTableForAddr(
    K2VMAP32_CONTEXT *    apContext,
    UINT32              aVirtAddr
    )
{
    //
    // only creates a pagetable if one does not already exist for the specified address
    //
    UINT32  entry;
    UINT32  ixEntry;
    UINT32  physPT;
    K2STAT  status;

    if (apContext->mFlags & K2VMAP32_FLAG_REALIZED)
        return K2STAT_ERROR_API_ORDER;

    ixEntry = aVirtAddr / K2_VA32_PAGETABLE_MAP_BYTES;
    entry = K2VMAP32_ReadPDE(apContext, ixEntry);
    if ((entry & K2VMAP32_FLAG_PRESENT) != 0)
        return 0;

    status = apContext->PT_Alloc(&physPT);
    if (K2STAT_IS_ERROR(status))
        return status;

    K2MEM_Zero((void *)physPT, K2_VA32_MEMPAGE_BYTES);

    aVirtAddr = K2_VA32_TO_PT_ADDR(apContext->mVirtMapBase, aVirtAddr);

    status = K2VMAP32_MapPage(apContext, aVirtAddr, physPT, K2OS_MAPTYPE_KERN_PAGETABLE);
    if (K2STAT_IS_ERROR(status))
    {
        apContext->PT_Free(physPT);
        return status;
    }

    entry = physPT | K2VMAP32_FLAG_PRESENT;

    K2VMAP32_WritePDE(apContext, ixEntry, entry);

    return 0;
}

K2STAT
K2VMAP32_FindUnmappedVirtualPage(
    K2VMAP32_CONTEXT *  apContext,
    UINT32 *            apVirtAddr,
    UINT32              aVirtEnd
    )
{
    BOOL    faultPDE;
    BOOL    faultPTE;

    if (apContext->mFlags & K2VMAP32_FLAG_REALIZED)
        return K2STAT_ERROR_API_ORDER;

    *apVirtAddr = (*apVirtAddr) & K2_VA32_PAGEFRAME_MASK;
    aVirtEnd &= K2_VA32_PAGEFRAME_MASK;

    do
    {
        K2VMAP32_VirtToPTE(apContext, *apVirtAddr, &faultPDE, &faultPTE);
        if ((faultPDE) || (faultPTE))
            return 0;

        (*apVirtAddr) += K2_VA32_MEMPAGE_BYTES;

    } while ((*apVirtAddr) != aVirtEnd);

    return K2STAT_ERROR_NOT_FOUND;
}

void
K2VMAP32_RealizeArchMappings(
    K2VMAP32_CONTEXT * apContext
    )
{
    UINT32      ixPDE;
    UINT32      pde;
    UINT32      ixPTE;
    UINT32 *    pPT;
    UINT32      pte;
    UINT32      mapType;
    UINT32      pteProto;

    if (apContext->mFlags & K2VMAP32_FLAG_REALIZED)
        return;

    //
    // prototype PTE that everybody uses the bits in
    //
#if K2_TARGET_ARCH_IS_ARM
    pteProto = A32_PTE_PRESENT;
    if (apContext->mFlags & K2VMAP32_FLAG_MULTIPROC)
        pteProto |= A32_PTE_SHARED;
#else
    pteProto = X32_PTE_PRESENT;
#endif
    
    //
    // ram through entire address space
    //
    for (ixPDE = 0; ixPDE < K2_VA32_ENTRIES_PER_PAGETABLE; ixPDE++)
    {
        pde = K2VMAP32_ReadPDE(apContext, ixPDE);
        if ((pde & K2VMAP32_FLAG_PRESENT) != 0)
        {
            pPT = (UINT32 *)(pde & K2VMAP32_PAGEPHYS_MASK);
            for (ixPTE = 0;ixPTE < K2_VA32_ENTRIES_PER_PAGETABLE; ixPTE++)
            {
                pte = pPT[ixPTE];
                if (pte & K2VMAP32_FLAG_PRESENT)
                {
                    mapType = pte & K2VMAP32_MAPTYPE_MASK;
                    
                    if (mapType & K2OS_MEMPAGE_ATTR_GUARD)
                    {
                        // mark as not present
                        pte = 0;
                    }
                    else
                    {
                        pte = (pte & K2VMAP32_PAGEPHYS_MASK) | pteProto;

#if K2_TARGET_ARCH_IS_ARM
                        if (!(mapType & K2OS_MEMPAGE_ATTR_EXEC))
                        {
                            pte |= A32_PTE_EXEC_NEVER;
                        }

                        if (mapType & K2OS_MEMPAGE_ATTR_DEVICEIO)
                        {
                            if (apContext->mFlags & K2VMAP32_FLAG_MULTIPROC)
                                pte |= A32_MMU_PTE_REGIONTYPE_SHAREABLE_DEVICE;
                            else
                                pte |= A32_MMU_PTE_REGIONTYPE_NONSHAREABLE_DEVICE;
                        }
                        else if (mapType & K2OS_MEMPAGE_ATTR_WRITE_THRU)
                        {
                            pte |= A32_MMU_PTE_REGIONTYPE_CACHED_WRITETHRU;
                        }
                        else
                        {
                            pte |= A32_MMU_PTE_REGIONTYPE_CACHED_WRITEBACK;
                        }

                        if (!(mapType & K2OS_MEMPAGE_ATTR_KERNEL))
                        {
                            pte |= A32_PTE_NOT_GLOBAL;
                            if (mapType & K2OS_MEMPAGE_ATTR_WRITEABLE)
                                pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_RW;
                            else
                                pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_RO;
                        }
                        else
                        {
                            if (mapType & K2OS_MEMPAGE_ATTR_WRITEABLE)
                                pte |= A32_MMU_PTE_PERMIT_KERN_RW_USER_NONE;
                            else
                                pte |= A32_MMU_PTE_PERMIT_KERN_RO_USER_NONE;
                        }
#else
                        if (mapType & K2OS_MEMPAGE_ATTR_WRITEABLE)
                            pte |= X32_PTE_WRITEABLE;

                        if (mapType & K2OS_MEMPAGE_ATTR_UNCACHED)
                            pte |= X32_PTE_CACHEDISABLE;

                        if (mapType & K2OS_MEMPAGE_ATTR_WRITE_THRU)
                            pte |= X32_PTE_WRITETHROUGH;
 
                        if (!(mapType & K2OS_MEMPAGE_ATTR_KERNEL))
                            pte |= X32_PTE_USER;
                        else
                            pte |= X32_PTE_GLOBAL;
#endif
                    }
                }
                else
                    pte = 0;
                pPT[ixPTE] = pte;
            }

#if K2_TARGET_ARCH_IS_ARM
            pde = (pde & K2VMAP32_PAGEPHYS_MASK) | A32_TTBE_PAGETABLE_PROTO;
#else
            pde = (pde & K2VMAP32_PAGEPHYS_MASK) | X32_KERN_PAGETABLE_PROTO;
#endif
        }
        else
            pde = 0;
        K2VMAP32_WritePDE(apContext, ixPDE, pde);
    }

    apContext->mFlags |= K2VMAP32_FLAG_REALIZED;
}