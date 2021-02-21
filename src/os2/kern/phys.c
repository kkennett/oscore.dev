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

#define PHYS_AUDIT      1

#define DESC_BUFFER_BYTES   (((sizeof(K2EFI_MEMORY_DESCRIPTOR) * 2) + 4) & ~3)

#if 0

static char const * sgK2TypeNames[] =
{
    "KERNEL_ELF      ",
    "PAGING          ",
    "PROC1           ",
    "ZERO            ",
    "CORES           ",
    "PHYS_TRACK      ",
    "TRANSITION      ",
    "EFI_MAP         ",
    "FW_TABLES       ",
    "ARCH_SPEC       ",
    "BUILTIN         ",
    "THREADPTRS      ",
};

static char const * sgEfiTypeNames[K2EFI_MEMTYPE_COUNT] =
{
    "EFI_Reserved    ",
    "EFI_Loader_Code ",
    "EFI_Loader_Data ",
    "EFI_Boot_Code   ",
    "EFI_Boot_Data   ",
    "EFI_Run_Code    ",
    "EFI_Run_Data    ",
    "EFI_Conventional",
    "EFI_Unusable    ",
    "EFI_ACPI_Reclaim",
    "EFI_ACPI_NVS    ",
    "EFI_MappedIO    ",
    "EFI_MappedIOPort",
    "EFI_PAL         ",
    "EFI_NV          ",
};

static void
sPrintDescType(
    UINT32 aDescType
)
{
    char const *pName;

    if (aDescType & 0x80000000)
    {
        aDescType -= 0x80000000;
        if (aDescType > K2OS_EFIMEMTYPE_LAST)
            pName = "*UNKNOWN K2*    ";
        else
            pName = sgK2TypeNames[aDescType];
    }
    else
    {
        if (aDescType < K2EFI_MEMTYPE_COUNT)
            pName = sgEfiTypeNames[aDescType];
        else
            pName = "*UNKNOWN EFI*   ";
    }

    K2OSKERN_Debug("%s", pName);
}

static void
sPrintDesc(
    UINT32                      aIx,
    K2EFI_MEMORY_DESCRIPTOR *   apDesc,
    BOOL                        aIsFree
)
{
    K2OSKERN_Debug("%03d: ", aIx);

    sPrintDescType(apDesc->Type);

    K2OSKERN_Debug(": p%08X v%08X z%08X (%6d) a%08X%08X %c\n",
        (UINT32)(((UINT64)apDesc->PhysicalStart) & 0x00000000FFFFFFFFull),
        (UINT32)(((UINT64)apDesc->VirtualStart) & 0x00000000FFFFFFFFull),
        (UINT32)(apDesc->NumberOfPages * K2_VA32_MEMPAGE_BYTES),
        (UINT32)apDesc->NumberOfPages,
        (UINT32)(((UINT64)apDesc->Attribute) >> 32), (UINT32)(((UINT64)apDesc->Attribute) & 0x00000000FFFFFFFFull),
        aIsFree ? '*' : ' ');
}

static void
sDumpEfiMap(void)
{
    UINT32                  entCount;
    K2EFI_MEMORY_DESCRIPTOR desc;
    UINT8 const *           pWork;
    BOOL                    reuse;
    UINT32                  ix;
    UINT32                  physStart;
    UINT32                  lastEnd;

    K2OSKERN_Debug("\nDumpEfiMap:\n----\n");
    entCount = gData.LoadInfo.mEfiMapSize / gData.LoadInfo.mEfiMemDescSize;
    pWork = (UINT8 const *)K2OS_KVA_EFIMAP_BASE;
    ix = 0;
    lastEnd = 0;
    do
    {
        K2MEM_Copy(&desc, pWork, sizeof(K2EFI_MEMORY_DESCRIPTOR));

        physStart = (UINT32)(UINT64)desc.PhysicalStart;

        if (physStart != lastEnd)
        {
            K2OSKERN_Debug("---:  HOLE             : p%08X           z%08X (%6d)\n",
                lastEnd,
                (physStart - lastEnd),
                (physStart - lastEnd) / K2_VA32_MEMPAGE_BYTES);
        }

        lastEnd = physStart + (desc.NumberOfPages * K2_VA32_MEMPAGE_BYTES);

        if (desc.Attribute & K2EFI_MEMORYFLAG_RUNTIME)
            reuse = FALSE;
        else
        {
            if (desc.Type & 0x80000000)
                reuse = FALSE;
            else
            {
                if ((desc.Type != K2EFI_MEMTYPE_Reserved) &&
                    (desc.Type != K2EFI_MEMTYPE_Unusable) &&
                    (desc.Type < K2EFI_MEMTYPE_ACPI_NVS))
                    reuse = TRUE;
                else
                    reuse = FALSE;
            }
        }

        sPrintDesc(ix, &desc, reuse);

        pWork += gData.LoadInfo.mEfiMemDescSize;
        ix++;
    } while (--entCount > 0);

    K2OSKERN_Debug("----\n");
}

#endif

static void 
sRemoveTransitionPage(
    void
)
{
    UINT32 *                pPTE;
    UINT32 *                pPTPageCount;
    UINT32                  virtPT;
    UINT32                  physPT;
    UINT32                  transPageAddr;
    K2OS_PHYSTRACK_UEFI *   pTrack;
    K2OS_PHYSTRACK_UEFI *   pTrackPT;
    UINT32                  entCount;
    K2EFI_MEMORY_DESCRIPTOR desc;
    UINT8 *                 pWork;

    //
    // unmap transition page as we are done with it
    //
    transPageAddr = gData.LoadInfo.mTransitionPageAddr;

    pTrack = (K2OS_PHYSTRACK_UEFI *)K2OS_PHYS32_TO_PHYSTRACK(transPageAddr);
    K2_ASSERT(pTrack->mType == K2OS_EFIMEMTYPE_TRANSITION);

    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(transPageAddr);
    K2_ASSERT(((*pPTE) & K2_VA32_PAGEFRAME_MASK) == transPageAddr);

    *pPTE = 0;

    //
    // see if we need to unmap the pagetable that held the transition page
    //
    pPTPageCount = (UINT32 *)K2OS_KVA_PROC1_PAGECOUNT;
    if (0 == --pPTPageCount[transPageAddr / K2_VA32_PAGETABLE_MAP_BYTES])
    {
        //
        // pagetable for transition page is now empty and should be unmapped
        //
        KernArch_BreakMapTransitionPageTable(&virtPT, &physPT);

        KernArch_InvalidateTlbPageOnThisCore(virtPT);

        //
        // virtPt / pagetable map bytes is page table page table. if this one hits zero we don't care
        //
        pPTPageCount[virtPT / K2_VA32_PAGETABLE_MAP_BYTES]--;

        //
        // must return the physical page used for pagetable to usable memory type
        // or it will get lost
        //
        pTrackPT = (K2OS_PHYSTRACK_UEFI *)K2OS_PHYS32_TO_PHYSTRACK(physPT);
        K2_ASSERT(pTrackPT->mType == K2OS_EFIMEMTYPE_PAGING);

        //
        // find allocation entry for this PT in the efi map and convert it to boot data
        // so it will get reclaimed like all the other boot data
        //
        entCount = gData.LoadInfo.mEfiMapSize / gData.LoadInfo.mEfiMemDescSize;
        pWork = (UINT8 *)K2OS_KVA_EFIMAP_BASE;
        do
        {
            K2MEM_Copy(&desc, pWork, sizeof(K2EFI_MEMORY_DESCRIPTOR));
            if ((desc.Type == K2OS_EFIMEMTYPE_PAGING) &&
                (((UINT32)(UINT64)desc.PhysicalStart) == physPT))
            {
                if (desc.NumberOfPages == 1)
                {
                    desc.Type = K2EFI_MEMTYPE_Boot_Data;
                    K2MEM_Copy(pWork, &desc, sizeof(K2EFI_MEMORY_DESCRIPTOR));
                    pTrackPT->mType = K2EFI_MEMTYPE_Boot_Data;
                }
                // else page is lost boo hoo
                break;
            }
            pWork += gData.LoadInfo.mEfiMemDescSize;
        } while (--entCount > 0);
    }

    //
    // find entry in EFI memory map and change its type to Boot Data
    // so it will get reclaimed like all the other boot data
    //
    entCount = gData.LoadInfo.mEfiMapSize / gData.LoadInfo.mEfiMemDescSize;
    pWork = (UINT8 *)K2OS_KVA_EFIMAP_BASE;
    do
    {
        K2MEM_Copy(&desc, pWork, sizeof(K2EFI_MEMORY_DESCRIPTOR));
        if (desc.Type == K2OS_EFIMEMTYPE_TRANSITION)
        {
            desc.Type = K2EFI_MEMTYPE_Boot_Data;
            K2MEM_Copy(pWork, &desc, sizeof(K2EFI_MEMORY_DESCRIPTOR));
            break;
        }
        pWork += gData.LoadInfo.mEfiMemDescSize;
    } while (--entCount > 0);
    K2_ASSERT(entCount != 0);

    //
    // update track structure
    //
    pTrack->mType = K2EFI_MEMTYPE_Boot_Data;

    //
    // kill page address in loader data
    //
    gData.LoadInfo.mTransitionPageAddr = 0xFFFFFFFF;
}

static void 
sChangePhysTrackToConventional(
    UINT32  aPhysAddr,
    UINT32  aPageCount
)
{
    K2OS_PHYSTRACK_UEFI * pTrack;

    K2_ASSERT(aPageCount > 0);

    pTrack = (K2OS_PHYSTRACK_UEFI *)K2OS_PHYS32_TO_PHYSTRACK(aPhysAddr);
    do {
        pTrack->mType = K2EFI_MEMTYPE_Conventional;
        pTrack++;
    } while (--aPageCount);
}

static void 
sCompactEfiMap(
    void
)
{
    UINT32                      entCount;
    UINT32                      outCount;
    UINT8                       inBuffer[DESC_BUFFER_BYTES];
    K2EFI_MEMORY_DESCRIPTOR *   pInDesc = (K2EFI_MEMORY_DESCRIPTOR *)inBuffer;
    UINT8                       outBuffer[DESC_BUFFER_BYTES];
    K2EFI_MEMORY_DESCRIPTOR *   pOutDesc = (K2EFI_MEMORY_DESCRIPTOR *)outBuffer;
    UINT8 *                     pIn;
    UINT8 *                     pOut;
    UINT32                      ixIn;
    UINT32                      lastEnd;
    UINT32                      physStart;
    UINT32                      physEnd;

    K2_ASSERT(gData.LoadInfo.mEfiMemDescSize <= DESC_BUFFER_BYTES);

    entCount = gData.LoadInfo.mEfiMapSize / gData.LoadInfo.mEfiMemDescSize;
    outCount = 0;

    pIn = (UINT8 *)K2OS_KVA_EFIMAP_BASE;
    pOut = pIn;

    ixIn = 0;
    lastEnd = 0;
    do {
        //
        // find next free memory entry
        //
        do
        {
            //
            // copy into 4-byte aligned buffer
            //
            if (pOut != pIn)
            {
                K2MEM_Copy(pOut, pIn, gData.LoadInfo.mEfiMemDescSize);
            }
            K2MEM_Copy(pInDesc, pIn, gData.LoadInfo.mEfiMemDescSize);

            physStart = (UINT32)(UINT64)pInDesc->PhysicalStart;
            physEnd = physStart + (pInDesc->NumberOfPages * K2_VA32_MEMPAGE_BYTES);
            if (physStart != lastEnd)
            {
                //
                // HOLE IN EFI MAP
                //
                //K2OSKERN_Debug("HOLE IN EFI MAP %08X for %08X\n", lastEnd, physStart - lastEnd);
            }

            if ((!(pInDesc->Attribute & K2EFI_MEMORYFLAG_RUNTIME)) &&
                (0 == (pInDesc->Type & 0x80000000)) &&
                (pInDesc->Type != K2EFI_MEMTYPE_Reserved) &&
                (pInDesc->Type != K2EFI_MEMTYPE_Unusable) &&
                (pInDesc->Type < K2EFI_MEMTYPE_ACPI_NVS))
                break;

            lastEnd = physEnd;

            pIn += gData.LoadInfo.mEfiMemDescSize;
            ixIn++;
            pOut += gData.LoadInfo.mEfiMemDescSize;
            outCount++;
            
        } while (--entCount > 0);
        if (entCount == 0)
            break;

        //
        // pIn points to a descriptor of free memory
        // pInDesc points to a 4-byte aligned copy of the pIn descriptor on the stack
        // pOut points to identical descriptor to the one at pIn at its final destination
        // pOutDesc points to a 4-byte aligned buffer of garbage on the stack
        //
        K2MEM_Copy(pOutDesc, pInDesc, gData.LoadInfo.mEfiMemDescSize);
        pOutDesc->Type = K2EFI_MEMTYPE_Conventional;
        
        //
        // convert outdesc range of phystrack to conventional memory type
        //
        sChangePhysTrackToConventional(
            (UINT32)((UINT64)pOutDesc->PhysicalStart),
            (UINT32)pOutDesc->NumberOfPages);

        if (--entCount > 0)
        {
            do {
                pIn += gData.LoadInfo.mEfiMemDescSize;
                ixIn++;

                K2MEM_Copy(pInDesc, pIn, gData.LoadInfo.mEfiMemDescSize);

                physStart = (UINT32)(UINT64)pInDesc->PhysicalStart;

                if (physStart != physEnd)
                {
                    // HOLE
                    break;
                }

                if ((pInDesc->Attribute & K2EFI_MEMORYFLAG_RUNTIME) ||
                    (0 != (pInDesc->Type & 0x80000000)) ||
                    (pInDesc->Type == K2EFI_MEMTYPE_Reserved) ||
                    (pInDesc->Type == K2EFI_MEMTYPE_Unusable) || 
                    (pInDesc->Type >= K2EFI_MEMTYPE_ACPI_NVS))
                {
                    break;
                }

                pOutDesc->NumberOfPages += pInDesc->NumberOfPages;
                physEnd = physStart + (pInDesc->NumberOfPages * K2_VA32_MEMPAGE_BYTES);

                //
                // convert indesc range of phystrack to conventional memory type
                //
                sChangePhysTrackToConventional(
                    (UINT32)((UINT64)pInDesc->PhysicalStart),
                    (UINT32)pInDesc->NumberOfPages);

            } while (--entCount > 0);
        }

        K2MEM_Copy(pOut, pOutDesc, gData.LoadInfo.mEfiMemDescSize);
        pOut += gData.LoadInfo.mEfiMemDescSize;
        outCount++;

        lastEnd = physEnd;

    } while (entCount > 0);

    K2MEM_Zero(pOut, (ixIn + 1 - outCount) * gData.LoadInfo.mEfiMemDescSize);
    
    gData.LoadInfo.mEfiMapSize = outCount * gData.LoadInfo.mEfiMemDescSize;
}

static UINT32 
sMemTypeToPageList(
    UINT32 aMemType
)
{
    if (aMemType & 0x80000000)
    {
        switch (aMemType)
        {
        case K2OS_EFIMEMTYPE_PAGING:
            return KernPhysPageList_Paging;

        case K2OS_EFIMEMTYPE_PROC1:
            return KernPhysPageList_Proc;

        case K2OS_EFIMEMTYPE_FW_TABLES:
        case K2OS_EFIMEMTYPE_BUILTIN:
        case K2OS_EFIMEMTYPE_THREADPTRS:
            return KernPhysPageList_KData;

        case K2OS_EFIMEMTYPE_KERNEL_ELF:
        case K2OS_EFIMEMTYPE_EFI_MAP:
        case K2OS_EFIMEMTYPE_ZERO:
        case K2OS_EFIMEMTYPE_CORES:
        case K2OS_EFIMEMTYPE_PHYS_TRACK:
        case K2OS_EFIMEMTYPE_ARCH_SPEC:
            return KernPhysPageList_KOver;

        case K2OS_EFIMEMTYPE_TRANSITION:
            return KernPhysPageList_Trans;

        default:
            K2OSKERN_Debug("%08X Memtype\n", aMemType);
            K2_ASSERT(0);
            break;
        }
    }
    else
    {
        K2_ASSERT(aMemType != K2EFI_MEMTYPE_Conventional);
        switch (aMemType)
        {
        case K2EFI_MEMTYPE_Run_Code:
            return KernPhysPageList_KText;

        case K2EFI_MEMTYPE_Run_Data:
            return KernPhysPageList_KData;

        case K2EFI_MEMTYPE_MappedIO:
            return KernPhysPageList_DeviceU;

        default:
            return KernPhysPageList_Unusable;
        }
    }

    return KernPhysPageList_Error;
}

static void
sInitPhysTrack(
    void
)
{
    UINT32                      entCount;
    UINT8                       descBuffer[DESC_BUFFER_BYTES];
    K2EFI_MEMORY_DESCRIPTOR *   pDesc = (K2EFI_MEMORY_DESCRIPTOR *)descBuffer;
    UINT8 *                     pWork;
    K2TREE_NODE *               pTreeNode;
    K2OS_PHYSTRACK_UEFI *       pTrackEFI;
    K2OSKERN_PHYSTRACK_PAGE *   pTrackPage;
    UINT32                      memProp;
    UINT32                      pageCount;
    UINT32                      listIx;
#ifdef PHYS_AUDIT
    UINT32                      pageListCount[KernPhysPageList_Count];
#endif

    //
    // make sure tree node userval and PHYSTRACK_FREE.mFlags are in same location 
    //
    K2_ASSERT(K2_FIELDOFFSET(K2TREE_NODE, mUserVal) == K2_FIELDOFFSET(K2OSKERN_PHYSTRACK_FREE, mFlags));

    entCount = gData.LoadInfo.mEfiMapSize / gData.LoadInfo.mEfiMemDescSize;
    pWork = (UINT8 *)K2OS_KVA_EFIMAP_BASE;
    do
    {
        K2MEM_Copy(pDesc, pWork, gData.LoadInfo.mEfiMemDescSize);

        pTrackEFI = (K2OS_PHYSTRACK_UEFI *)K2OS_PHYS32_TO_PHYSTRACK((UINT32)((UINT64)pDesc->PhysicalStart));
        memProp = pTrackEFI->mProp;
        memProp = ((memProp & 0x00007000) >> 3) | ((memProp & 0x0000001F) << 4);

        pTrackPage = (K2OSKERN_PHYSTRACK_PAGE *)pTrackEFI;
        pageCount = pDesc->NumberOfPages;

        if ((!(pDesc->Attribute & K2EFI_MEMORYFLAG_RUNTIME)) &&
            (pDesc->Type == K2EFI_MEMTYPE_Conventional))
        {
            memProp |= K2OSKERN_PHYSTRACK_UNALLOC_FLAG;
            //
            // tree node userval and PHYSTRACK_FREE.mFlags are in same location 
            //
            pTreeNode = (K2TREE_NODE *)pTrackEFI;
            pTreeNode->mUserVal = 
                (((UINT32)pDesc->NumberOfPages) << K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL) |
                memProp;
            K2TREE_Insert(&gData.PhysFreeTree, pTreeNode->mUserVal, pTreeNode);

            pTreeNode++;
            if (--pageCount > 0)
            {
                //
                // mark all pages in this range with the same flags, with pagecount= 0
                //
                do {
                    K2_ASSERT(pDesc->Type == ((K2OS_PHYSTRACK_UEFI *)pTreeNode)->mType);
                    pTreeNode->mUserVal = memProp;
                    pTreeNode++;
                } while (--pageCount);
            }
        }
        else
        {
            //
            // every page in the region goes onto a list
            //
            listIx = sMemTypeToPageList(pTrackEFI->mType);
            do {
                K2_ASSERT(pDesc->Type == ((K2OS_PHYSTRACK_UEFI *)pTrackPage)->mType);
                pTrackPage->mFlags = (listIx << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | memProp;
                pTrackPage->mpOwnerObject = NULL;
                K2LIST_AddAtTail(&gData.PhysPageList[listIx], &pTrackPage->ListLink);
                pTrackPage++;
            } while (--pageCount);
        }

        pWork += gData.LoadInfo.mEfiMemDescSize;
    } while (--entCount > 0);

#ifdef PHYS_AUDIT
    K2MEM_Zero(pageListCount, sizeof(UINT32)*KernPhysPageList_Count);
    entCount = K2_VA32_PAGEFRAMES_FOR_4G;
    pTrackPage = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(0);
    do {
        if (pTrackPage->mFlags & K2OSKERN_PHYSTRACK_UNALLOC_FLAG)
        {
            pageCount = (pTrackPage->mFlags & K2OSKERN_PHYSTRACK_PAGE_COUNT_MASK) >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL;
            K2_ASSERT(pageCount > 0);
            K2_ASSERT(pageCount < entCount);
            //
            // all other pages in the range should have identical properties and a zero pagecount
            //
            memProp = (pTrackPage->mFlags & K2OSKERN_PHYSTRACK_PROP_MASK) | K2OSKERN_PHYSTRACK_UNALLOC_FLAG;
            pTrackPage++;
            entCount--;
            if (--pageCount > 0)
            {
                do {
                    K2_ASSERT((pTrackPage->mFlags & (K2OSKERN_PHYSTRACK_PROP_MASK | K2OSKERN_PHYSTRACK_UNALLOC_FLAG | K2OSKERN_PHYSTRACK_CONTIG_ALLOC_FLAG)) == memProp);
                    pTrackPage++;
                    --entCount;
                } while (--pageCount);
            }
        }
        else
        {
            listIx = K2OSKERN_PHYSTRACK_PAGE_FLAGS_GET_LIST(pTrackPage->mFlags);
            if (listIx > 0)
            {
                K2_ASSERT(listIx < KernPhysPageList_Count);
                pageListCount[listIx]++;
            }
            pTrackPage++;
            entCount--;
        }
    } while (entCount > 0);
    for (listIx = 0; listIx < KernPhysPageList_Count; listIx++)
    {
        K2_ASSERT(gData.PhysPageList[listIx].mNodeCount == pageListCount[listIx]);
    }
    //
    // if we get here physical memory space is ok
    //
#endif
}

#if K2_TARGET_ARCH_IS_ARM

static void 
sA32Init_PhysExtra(
    void
)
{
    K2TREE_NODE *               pTreeNode;
    UINT32                      userVal;
    K2OSKERN_PHYSTRACK_PAGE *   pTrackPage;
    UINT32                      count;

    //
    // allocate a physical page for CPU vectors
    //
    pTreeNode = K2TREE_FirstNode(&gData.PhysFreeTree);
    K2_ASSERT(pTreeNode != NULL);

    userVal = pTreeNode->mUserVal;

    K2TREE_Remove(&gData.PhysFreeTree, pTreeNode);

    pTrackPage = (K2OSKERN_PHYSTRACK_PAGE*)pTreeNode;

    if ((userVal >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL) > 1)
    {
        //
        // some space was left in the smallest node in the tree so
        // put that space back on the physical memory tree
        //
        pTreeNode++;
        pTreeNode->mUserVal = userVal - (1 << K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL);
        K2TREE_Insert(&gData.PhysFreeTree, pTreeNode->mUserVal, pTreeNode);
    }

    //
    // put the page on the overhead list
    //
    pTrackPage->mFlags = (KernPhysPageList_KOver << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | (userVal & K2OSKERN_PHYSTRACK_PROP_MASK);
    pTrackPage->mpOwnerObject = NULL;
    K2LIST_AddAtTail(&gData.PhysPageList[KernPhysPageList_KOver], &pTrackPage->ListLink);

    gData.mA32VectorPagePhys = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTrackPage);
    K2OSKERN_Debug("A32 Vectors Physical Page --> %08X\n", gData.mA32VectorPagePhys);

    if (gData.mCpuCount > 1)
    {
        //
        // allocate physical pages to start secondary cores
        // transition page must be BELOW 2GB (0x80000000)
        //
        pTreeNode = K2TREE_FirstNode(&gData.PhysFreeTree);
        K2_ASSERT(pTreeNode != NULL);

        do {
            pTrackPage = (K2OSKERN_PHYSTRACK_PAGE*)pTreeNode;
            count = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTrackPage);
            if (count < (0x80000000 - (K2_VA32_MEMPAGE_BYTES * K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT)))
            {
                userVal = pTreeNode->mUserVal;
                if ((userVal >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL) >= K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT)
                    break;
            }
            pTreeNode = K2TREE_NextNode(&gData.PhysFreeTree, pTreeNode);

        } while (pTreeNode != NULL);

        K2_ASSERT(pTreeNode != NULL);

        K2TREE_Remove(&gData.PhysFreeTree, pTreeNode);

        if ((userVal >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL) > K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT)
        {
            //
            // some space was left in the node in the tree so
            // put that space back on the physical memory tree
            //
            pTreeNode += K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT;
            pTreeNode->mUserVal = userVal - (K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT << K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL);
            K2TREE_Insert(&gData.PhysFreeTree, pTreeNode->mUserVal, pTreeNode);
        }

        gData.mA32StartAreaPhys = K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTrackPage);
        K2OSKERN_Debug("A32StartAreaPhys = %08X, %d pages\n", gData.mA32StartAreaPhys, K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT);

        //
        // put all the pages on the paging list
        //
        count = K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT;
        do {
            pTrackPage->mFlags = (KernPhysPageList_Paging << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | (userVal & K2OSKERN_PHYSTRACK_PROP_MASK);
            pTrackPage->mpOwnerObject = NULL;
            K2LIST_AddAtTail(&gData.PhysPageList[KernPhysPageList_Paging], &pTrackPage->ListLink);
            pTrackPage++;
        } while (--count > 0);
    }
}
#endif

void 
KernPhys_Init(
    void
)
{
    UINT32 ix;

    K2OSKERN_SeqInit(&gData.PhysMemSeqLock);

    K2TREE_Init(&gData.PhysFreeTree, NULL);
    for (ix = 0; ix < KernPhysPageList_Count; ix++)
    {
        K2LIST_Init(&gData.PhysPageList[ix]);
    }

    sRemoveTransitionPage();

    sCompactEfiMap();

    sInitPhysTrack();

//    sDumpEfiMap();

#if K2_TARGET_ARCH_IS_ARM
    sA32Init_PhysExtra();
#endif
}
