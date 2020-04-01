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
#define VIRT_AUDIT      1
#define DUMP_HEAP       0

#if 0

static char const * sgK2TypeNames[] =
{
    "PAGING          ",
    "TEXT            ",
    "READ            ",
    "DATA            ",
    "PROC0           ",
    "DLX             ",
    "LOADER          ",
    "ZERO            ",
    "CORES           ",
    "PHYS_TRACK      ",
    "TRANSITION      ",
    "EFI_MAP         ",
    "FW_TABLES       ",
    "ARCH_SPEC       "
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

static
void
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

static
void
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

static
void
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
    entCount = gData.mpShared->LoadInfo.mEfiMapSize / gData.mpShared->LoadInfo.mEfiMemDescSize;
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

        pWork += gData.mpShared->LoadInfo.mEfiMemDescSize;
        ix++;
    } while (--entCount > 0);

    K2OSKERN_Debug("----\n");
}

#endif

static void sRemoveTransitionPage(void)
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
    transPageAddr = gData.mpShared->LoadInfo.mTransitionPageAddr;

    pTrack = (K2OS_PHYSTRACK_UEFI *)K2OS_PHYS32_TO_PHYSTRACK(transPageAddr);
    K2_ASSERT(pTrack->mType == K2OS_EFIMEMTYPE_TRANSITION);

    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(transPageAddr);
    K2_ASSERT(((*pPTE) & K2_VA32_PAGEFRAME_MASK) == transPageAddr);

    *pPTE = 0;
    KernArch_InvalidateTlbPageOnThisCore(transPageAddr);

    //
    // see if we need to unmap the pagetable that held the transition page
    //
    pPTPageCount = (UINT32 *)K2OS_KVA_PTPAGECOUNT_BASE;
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
        entCount = gData.mpShared->LoadInfo.mEfiMapSize / gData.mpShared->LoadInfo.mEfiMemDescSize;
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
            pWork += gData.mpShared->LoadInfo.mEfiMemDescSize;
        } while (--entCount > 0);
    }

    //
    // find entry in EFI memory map and change its type to Boot Data
    // so it will get reclaimed like all the other boot data
    //
    entCount = gData.mpShared->LoadInfo.mEfiMapSize / gData.mpShared->LoadInfo.mEfiMemDescSize;
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
        pWork += gData.mpShared->LoadInfo.mEfiMemDescSize;
    } while (--entCount > 0);
    K2_ASSERT(entCount != 0);

    //
    // update track structure
    //
    pTrack->mType = K2EFI_MEMTYPE_Boot_Data;

    //
    // kill page address in loader data
    //
    gData.mpShared->LoadInfo.mTransitionPageAddr = 0xFFFFFFFF;
}

static
void 
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

#define DESC_BUFFER_BYTES   (((sizeof(K2EFI_MEMORY_DESCRIPTOR) * 2) + 4) & ~3)

static void sCompactEfiMap(void)
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

    K2_ASSERT(gData.mpShared->LoadInfo.mEfiMemDescSize <= DESC_BUFFER_BYTES);

    entCount = gData.mpShared->LoadInfo.mEfiMapSize / gData.mpShared->LoadInfo.mEfiMemDescSize;
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
                K2MEM_Copy(pOut, pIn, gData.mpShared->LoadInfo.mEfiMemDescSize);
            }
            K2MEM_Copy(pInDesc, pIn, gData.mpShared->LoadInfo.mEfiMemDescSize);

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

            pIn += gData.mpShared->LoadInfo.mEfiMemDescSize;
            ixIn++;
            pOut += gData.mpShared->LoadInfo.mEfiMemDescSize;
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
        K2MEM_Copy(pOutDesc, pInDesc, gData.mpShared->LoadInfo.mEfiMemDescSize);
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
                pIn += gData.mpShared->LoadInfo.mEfiMemDescSize;
                ixIn++;

                K2MEM_Copy(pInDesc, pIn, gData.mpShared->LoadInfo.mEfiMemDescSize);

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

        K2MEM_Copy(pOut, pOutDesc, gData.mpShared->LoadInfo.mEfiMemDescSize);
        pOut += gData.mpShared->LoadInfo.mEfiMemDescSize;
        outCount++;

        lastEnd = physEnd;

    } while (entCount > 0);

    K2MEM_Zero(pOut, (ixIn + 1 - outCount) * gData.mpShared->LoadInfo.mEfiMemDescSize);
    
    gData.mpShared->LoadInfo.mEfiMapSize = outCount * gData.mpShared->LoadInfo.mEfiMemDescSize;
}

static UINT32 sMemTypeToPageList(UINT32 aMemType)
{
    if (aMemType & 0x80000000)
    {
        switch (aMemType)
        {
        case K2OS_EFIMEMTYPE_PAGING:
            return KernPhysPageList_Paging;

        case K2OS_EFIMEMTYPE_TEXT:
            return KernPhysPageList_Non_KText;

        case K2OS_EFIMEMTYPE_READ:
            return KernPhysPageList_Non_KRead;

        case K2OS_EFIMEMTYPE_PROC0:
            return KernPhysPageList_Proc;

        case K2OS_EFIMEMTYPE_DATA:
        case K2OS_EFIMEMTYPE_DLX:
        case K2OS_EFIMEMTYPE_FW_TABLES:
            return KernPhysPageList_Non_KData;

        case K2OS_EFIMEMTYPE_LOADER:
        case K2OS_EFIMEMTYPE_EFI_MAP:
        case K2OS_EFIMEMTYPE_ZERO:
        case K2OS_EFIMEMTYPE_CORES:
        case K2OS_EFIMEMTYPE_PHYS_TRACK:
        case K2OS_EFIMEMTYPE_ARCH_SPEC:
            return KernPhysPageList_KOver;

        case K2OS_EFIMEMTYPE_TRANSITION:
            return KernPhysPageList_Trans;

        default:
            break;
        }
    }
    else
    {
        K2_ASSERT(aMemType != K2EFI_MEMTYPE_Conventional);
        switch (aMemType)
        {
        case K2EFI_MEMTYPE_Run_Code:          // 5
            return KernPhysPageList_Non_KText;

        case K2EFI_MEMTYPE_Run_Data:          // 6
            return KernPhysPageList_Non_KData;

        case K2EFI_MEMTYPE_MappedIO:          // 11
            return KernPhysPageList_DeviceU;

        default:
            return KernPhysPageList_Unusable;
        }
    }

    return KernPhysPageList_Error;
}

static
void
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

    entCount = gData.mpShared->LoadInfo.mEfiMapSize / gData.mpShared->LoadInfo.mEfiMemDescSize;
    pWork = (UINT8 *)K2OS_KVA_EFIMAP_BASE;
    do
    {
        K2MEM_Copy(pDesc, pWork, gData.mpShared->LoadInfo.mEfiMemDescSize);

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

        pWork += gData.mpShared->LoadInfo.mEfiMemDescSize;
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

#define INIT_HEAP_NODE_COUNT    64

static K2OSKERN_VMNODE  sgInitHeapNodes[INIT_HEAP_NODE_COUNT];
static K2LIST_ANCHOR    sgInitHeapNodeList;

static
K2HEAP_NODE *
sInitHeap_AcquireNode(
    K2HEAP_ANCHOR *apHeap
)
{
    K2LIST_LINK *pRet;

    K2_ASSERT(sgInitHeapNodeList.mNodeCount > 0);

    pRet = sgInitHeapNodeList.mpHead;
    K2LIST_Remove(&sgInitHeapNodeList, pRet);

    return (K2HEAP_NODE *)pRet;
}

static
void
sInitHeap_ReleaseNode(
    K2HEAP_ANCHOR * apHeap,
    K2HEAP_NODE *   apNode
)
{
    K2LIST_AddAtTail(&sgInitHeapNodeList, (K2LIST_LINK *)apNode);
}

#if DUMP_HEAP

static
void 
sDumpHeapNode(
    K2HEAP_ANCHOR * apHeap, 
    K2HEAP_NODE *   apNode
)
{
    K2OSKERN_Debug(
        "%08X %08X %s\n", 
        apNode->AddrTreeNode.mUserVal, 
        apNode->SizeTreeNode.mUserVal, 
        K2HEAP_NodeIsFree(apNode) ? "FREE" : "USED"
    );
}

#endif

static BOOL sCheckPhysPageOnList(UINT32 aPhysAddr, KernPhysPageList aList)
{
    K2LIST_LINK *               pLink;
    K2OSKERN_PHYSTRACK_PAGE *   pTrackPage;

    K2_ASSERT(gData.PhysPageList[aList].mNodeCount != 0);

    pLink = gData.PhysPageList[aList].mpHead;
    while (pLink != NULL)
    {
        pTrackPage = K2_GET_CONTAINER(K2OSKERN_PHYSTRACK_PAGE, pLink, ListLink);
        if (aPhysAddr == K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTrackPage))
            break;
        pLink = pLink->mpNext;
    }
    K2_ASSERT(pLink != NULL);
    return (pLink == NULL) ? FALSE : TRUE;
}

static BOOL sCheckPteRange(UINT32 aVirtAddr, UINT32 aPageCount, UINT32 aMapAttr, KernPhysPageList aList)
{
    UINT32 *    pPTE;
    UINT32      pte;

    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(aVirtAddr);
    do
    {
        pte = *pPTE;
        K2_ASSERT((pte & K2OSKERN_PTE_PRESENT_BIT) != 0);
        if (!KernArch_VerifyPteKernAccessAttr(pte, aMapAttr))
        {
            K2_ASSERT(0);
        }
        if (!sCheckPhysPageOnList(pte & K2_VA32_PAGEFRAME_MASK, aList))
            return FALSE;
        pPTE++;
        aVirtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--aPageCount);
    return TRUE;
}

static void sAddSegmentToTree(K2OSKERN_OBJ_SEGMENT *apSeg)
{
    K2OSKERN_PHYSTRACK_PAGE *   pPhysPage;
    UINT32                      virtAddr;
    UINT32                      pageCount;
    BOOL                        ptPresent;
    UINT32                      pte;
    UINT32                      accessAttr;
    UINT32 *                    pPTE;
    K2STAT                      stat;

    K2TREE_Insert(&gpProc0->SegTree, apSeg->SegTreeNode.mUserVal, &apSeg->SegTreeNode);
    stat = KernObj_Add(&apSeg->Hdr, NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    virtAddr = apSeg->SegTreeNode.mUserVal;
    pageCount = apSeg->mPagesBytes / K2_VA32_MEMPAGE_BYTES;

    if ((apSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK) == K2OS_SEG_ATTR_TYPE_THREAD)
    {
        // bottom-most page is a guard page
        pPTE = KernArch_Translate(gpProc0, virtAddr, &ptPresent, &pte, &accessAttr);
        K2_ASSERT(ptPresent);
        K2_ASSERT(!(pte & K2OSKERN_PTE_PRESENT_BIT));
        if (!(pte & K2OSKERN_PTE_NP_BIT))
            *pPTE = (pte | K2OSKERN_PTE_NP_BIT | K2OSKERN_PTE_NP_TYPE_GUARD);
        virtAddr += K2_VA32_MEMPAGE_BYTES;
        pageCount--;
    }

    do {
        KernArch_Translate(gpProc0, virtAddr, &ptPresent, &pte, &accessAttr);
        K2_ASSERT(ptPresent);
        K2_ASSERT(pte & K2OSKERN_PTE_PRESENT_BIT);
        pPhysPage = (K2OSKERN_PHYSTRACK_PAGE *)K2OS_PHYS32_TO_PHYSTRACK(pte & K2_VA32_PAGEFRAME_MASK);
        K2_ASSERT((pPhysPage->mFlags & K2OSKERN_PHYSTRACK_UNALLOC_FLAG) == 0);
        pPhysPage->mpOwnerObject = (void *)apSeg;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--pageCount);
}

static void sInitMapDlxSegments(K2OSKERN_OBJ_DLX *apDlxObj)
{
    K2STAT                  stat;
    UINT32                  ix;
    UINT32                  pagesBytes;
    K2OSKERN_OBJ_SEGMENT *  pDlxPartSeg;
    K2OSKERN_VMNODE *       pNode;
    KernPhysPageList        pageList;

    K2_ASSERT(apDlxObj->PageSeg.Hdr.mObjType == K2OS_Obj_Segment);
    K2_ASSERT(apDlxObj->PageSeg.Hdr.mObjType = K2OS_Obj_Segment);
    K2_ASSERT(apDlxObj->PageSeg.Hdr.mObjFlags == K2OSKERN_OBJ_FLAG_PERMANENT);
    K2_ASSERT(apDlxObj->PageSeg.Hdr.mRefCount == 0x7FFFFFFF);
    K2_ASSERT(apDlxObj->PageSeg.mPagesBytes == K2_VA32_MEMPAGE_BYTES);
    K2_ASSERT(apDlxObj->PageSeg.mSegAndMemPageAttr == (K2OS_MAPTYPE_KERN_DATA | K2OS_SEG_ATTR_TYPE_DLX_PAGE));
    K2_ASSERT(apDlxObj->PageSeg.Info.DlxPage.mpDlxObj == apDlxObj);
    sCheckPteRange(apDlxObj->PageSeg.SegTreeNode.mUserVal, 1, K2OS_MAPTYPE_KERN_DATA, KernPhysPageList_Non_KData);
    sAddSegmentToTree(&apDlxObj->PageSeg);

    for (ix = 0; ix < DlxSeg_Count; ix++)
    {
        pDlxPartSeg = &apDlxObj->SegObj[ix];

        if ((pDlxPartSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK) == K2OS_SEG_ATTR_TYPE_DLX_PART)
        {
            K2_ASSERT(pDlxPartSeg->Info.DlxPart.DlxSegmentInfo.mLinkAddr != 0);
            K2_ASSERT(pDlxPartSeg->Info.DlxPart.DlxSegmentInfo.mMemActualBytes != 0);
            pagesBytes = K2_ROUNDUP(pDlxPartSeg->Info.DlxPart.DlxSegmentInfo.mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
            stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, pDlxPartSeg->Info.DlxPart.DlxSegmentInfo.mLinkAddr, pagesBytes, (K2HEAP_NODE **)&pNode);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
            pNode->mpSegment = pDlxPartSeg;
            K2_ASSERT((pDlxPartSeg->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_MASK) != 0);
            if (pDlxPartSeg->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_EXEC)
            {
                pageList = KernPhysPageList_Non_KText;
            }
            else if (pDlxPartSeg->mSegAndMemPageAttr & K2OS_MEMPAGE_ATTR_WRITEABLE)
            {
                pageList = KernPhysPageList_Non_KData;
            }
            else
            {
                pageList = KernPhysPageList_Non_KRead;
            }
            sCheckPteRange(
                pDlxPartSeg->Info.DlxPart.DlxSegmentInfo.mLinkAddr,
                pagesBytes / K2_VA32_MEMPAGE_BYTES, 
                pDlxPartSeg->mSegAndMemPageAttr, pageList
            );

            pDlxPartSeg->Hdr.mObjType = K2OS_Obj_Segment;
            pDlxPartSeg->Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
            pDlxPartSeg->Hdr.mRefCount = 0x7FFFFFFF;
            pDlxPartSeg->SegTreeNode.mUserVal = pDlxPartSeg->Info.DlxPart.DlxSegmentInfo.mLinkAddr;
            pDlxPartSeg->mPagesBytes = pagesBytes;
            sAddSegmentToTree(pDlxPartSeg);
        }
        else
        {
            K2_ASSERT((pDlxPartSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK) == 0);
        }
    }
}

static BOOL sCheckPteRangeUnmapped(UINT32 aVirtAddr, UINT32 aPageCount)
{
    UINT32 *    pPTE;
    UINT32      pte;

    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(aVirtAddr);
    do
    {
        pte = *pPTE;

        if (pte & K2OSKERN_PTE_PRESENT_BIT)
        {
            K2OSKERN_Debug("%08X should be unmapped but PTE  is %08X\n", aVirtAddr, pte);
            K2_ASSERT((pte & K2OSKERN_PTE_PRESENT_BIT) == 0);
        }
        if (pte & K2OSKERN_PTE_NP_BIT)
        {
            K2OSKERN_Debug("%08X should be unmapped but PTE2 is %08X\n", aVirtAddr, pte);
            K2_ASSERT((pte & K2OSKERN_PTE_NP_BIT) == 0);
        }
        pPTE++;
        aVirtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--aPageCount);
    return TRUE;
}

static void sAddEfiRuntimeMappings(void)
{
    UINT32                  entCount;
    K2EFI_MEMORY_DESCRIPTOR desc;
    UINT8 const *           pWork;
    K2OSKERN_VMNODE *       pNode;
    K2STAT                  stat;
    KernPhysPageList            list;
    UINT32                  attr;

    entCount = gData.mpShared->LoadInfo.mEfiMapSize / gData.mpShared->LoadInfo.mEfiMemDescSize;
    pWork = (UINT8 const *)K2OS_KVA_EFIMAP_BASE;
    do
    {
        K2MEM_Copy(&desc, pWork, sizeof(K2EFI_MEMORY_DESCRIPTOR));

        if (desc.Attribute & K2EFI_MEMORYFLAG_RUNTIME)
        {
            K2_ASSERT(desc.VirtualStart != 0);

            stat = K2HEAP_AllocNodeAt(
                &gData.KernVirtHeap,
                (UINT32)(desc.VirtualStart & 0xFFFFFFFFull),
                K2_VA32_MEMPAGE_BYTES * desc.NumberOfPages,
                (K2HEAP_NODE **)&pNode);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));

            pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;

            if (desc.Type == K2EFI_MEMTYPE_Run_Code)
            {
                pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_EFI_RUN_TEXT;
                list = KernPhysPageList_Non_KText;
                attr = K2OS_MAPTYPE_KERN_TEXT;
            }
            else if (desc.Type == K2EFI_MEMTYPE_MappedIO)
            {
                pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_EFI_MAPPED_IO;
                list = KernPhysPageList_DeviceU;
                attr = K2OS_MAPTYPE_KERN_DEVICEIO;
            }
            else
            {
                pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_EFI_RUN_DATA;
                list = KernPhysPageList_Non_KData;
                attr = K2OS_MAPTYPE_KERN_DATA;
            }

            sCheckPteRange(K2HEAP_NodeAddr(&pNode->HeapNode), desc.NumberOfPages, attr, list);
        }
        else
        {
            K2_ASSERT(desc.VirtualStart == 0);
        }
        pWork += gData.mpShared->LoadInfo.mEfiMemDescSize;
    } while (--entCount > 0);
}

static BOOL sVerifyPhysTrack(void)
{
    UINT32 *pPTE;
    UINT32  pte;
    UINT32  physPageAddr;
    UINT32  pageCount;

    pageCount = K2OS_KVA_PHYSTRACKAREA_SIZE / K2_VA32_MEMPAGE_BYTES;
    pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(K2OS_KVA_PHYSTRACKAREA_BASE);
    do {
        pte = *pPTE;
        K2_ASSERT((pte & K2OSKERN_PTE_PRESENT_BIT) != 0);
        physPageAddr = (pte & K2_VA32_PAGEFRAME_MASK);
        if (physPageAddr != gData.mpShared->LoadInfo.mZeroPagePhys)
        {
            if (!sCheckPhysPageOnList(physPageAddr, KernPhysPageList_KOver))
            {
                return FALSE;
            }
        }
        pPTE++;
    } while (--pageCount);
    return TRUE;
}

#if 0

static void sDumpSegmentTree(void)
{
    K2TREE_NODE * pTreeNode;

    K2OSKERN_Debug("\nKernel Segment Tree:\n-----\n");
    pTreeNode = K2TREE_FirstNode(&gpProc0->SegTree);
    if (pTreeNode != NULL)
    {
        do {
            KernSegment_Dump(K2_GET_CONTAINER(K2OSKERN_OBJ_SEGMENT, pTreeNode, SegTreeNode));
            pTreeNode = K2TREE_NextNode(&gpProc0->SegTree, pTreeNode);
        } while (pTreeNode != NULL);
    }
    K2OSKERN_Debug("-----\n");
}

#endif

#if K2_TARGET_ARCH_IS_ARM
static void sA32Init_BeforeVirt(void)
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

    if (gData.mCpuCount > 1)
    {
        //
        // allocate physical pages to start secondary cores
        //
        pTreeNode = K2TREE_FirstNode(&gData.PhysFreeTree);
        K2_ASSERT(pTreeNode != NULL);

        do {
            userVal = pTreeNode->mUserVal;
            if ((userVal >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL) >= K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT)
                break;

            pTreeNode = K2TREE_NextNode(&gData.PhysFreeTree, pTreeNode);

        } while (pTreeNode != NULL);

        K2_ASSERT(pTreeNode != NULL);

        K2TREE_Remove(&gData.PhysFreeTree, pTreeNode);

        pTrackPage = (K2OSKERN_PHYSTRACK_PAGE*)pTreeNode;

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

        //
        // put all the pages on the overhead list
        //
        count = K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT;
        do {
            pTrackPage->mFlags = (KernPhysPageList_KOver << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | (userVal & K2OSKERN_PHYSTRACK_PROP_MASK);
            pTrackPage->mpOwnerObject = NULL;
            K2LIST_AddAtTail(&gData.PhysPageList[KernPhysPageList_KOver], &pTrackPage->ListLink);
            pTrackPage++;
        } while (--count > 0);
    }
}
#endif

static void sInit_BeforeVirt(void)
{
    K2STAT              stat;
    UINT32              ix;
    K2OSKERN_VMNODE *   pNode;
#if VIRT_AUDIT
    UINT32              virtAddr;
    UINT32              pageCount;
    BOOL                ptIsPresent;
    UINT32              pte;
    UINT32              accessAttr;
    UINT32              lastEnd;
    K2HEAP_NODE *       pHeapNode;
#endif

    //
    // physical memory (PHYSTRACK)
    //
    K2OSKERN_SeqIntrInit(&gData.PhysMemSeqLock);
    K2TREE_Init(&gData.PhysFreeTree, NULL);
    for (ix = 0; ix < KernPhysPageList_Count; ix++)
    {
        K2LIST_Init(&gData.PhysPageList[ix]);
    }

    //
    // virtual memory (allocation, not mapping)
    //
    K2OS_CritSecInit(&gData.KernVirtHeapSec);

    K2LIST_Init(&sgInitHeapNodeList);
    K2MEM_Zero(sgInitHeapNodes, sizeof(K2OSKERN_VMNODE) * INIT_HEAP_NODE_COUNT);
    for (ix = 0; ix < INIT_HEAP_NODE_COUNT; ix++)
    {
        K2LIST_AddAtTail(&sgInitHeapNodeList, (K2LIST_LINK *)&sgInitHeapNodes[ix]);
    }
    K2HEAP_Init(&gData.KernVirtHeap, sInitHeap_AcquireNode, sInitHeap_ReleaseNode);
    stat = K2HEAP_AddFreeSpaceNode(&gData.KernVirtHeap, K2OS_KVA_KERN_BASE, (~K2OS_KVA_KERN_BASE) + 1, NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    K2OSKERN_SeqIntrInit(&gData.KernVirtMapLock);

    K2OSKERN_SeqIntrInit(&gpProc0->SegTreeSeqLock);
    K2TREE_Init(&gpProc0->SegTree, NULL);

    //
    // init physical stuff first
    //
    sRemoveTransitionPage();
    sCompactEfiMap();
    sInitPhysTrack();

//    sDumpEfiMap();

    //
    // init virtual stuff
    //

    // top down
    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_PUBLICAPI_BASE, K2OS_KVA_PUBLICAPI_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_PUBLICAPI;
    sCheckPteRangeUnmapped(K2OS_KVA_PUBLICAPI_BASE, K2OS_KVA_PUBLICAPI_SIZE / K2_VA32_MEMPAGE_BYTES);

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_ARCHSPEC_BASE, K2OS_KVA_ARCHSPEC_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_ARCHSPEC;
    sCheckPteRangeUnmapped(K2OS_KVA_ARCHSPEC_BASE, K2OS_KVA_ARCHSPEC_SIZE / K2_VA32_MEMPAGE_BYTES);

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_COREPAGES_BASE, K2OS_KVA_COREPAGES_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_COREPAGES;
    sCheckPteRange(K2OS_KVA_COREPAGES_BASE, gData.mCpuCount, K2OS_MAPTYPE_KERN_DATA, KernPhysPageList_KOver);
    sCheckPteRangeUnmapped(K2OS_KVA_COREPAGES_BASE + (gData.mCpuCount * K2_VA32_MEMPAGE_BYTES), K2OS_MAX_CPU_COUNT - gData.mCpuCount);

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_CORECLEARPAGES_BASE, K2OS_KVA_CORECLEARPAGES_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_CORECLEARPAGES;
    sCheckPteRangeUnmapped(K2OS_KVA_CORECLEARPAGES_BASE, K2OS_MAX_CPU_COUNT);

    gpProc0->SegObjPrimaryThreadStack.Hdr.mObjType = K2OS_Obj_Segment;
    gpProc0->SegObjPrimaryThreadStack.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
    gpProc0->SegObjPrimaryThreadStack.Hdr.mRefCount = 0x7FFFFFFF;
    gpProc0->SegObjPrimaryThreadStack.mSegAndMemPageAttr = K2OS_SEG_ATTR_TYPE_THREAD | K2OS_MAPTYPE_KERN_DATA;
    gpProc0->SegObjPrimaryThreadStack.Info.ThreadStack.mpThread = gpThread0;
    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_THREAD0_STACK_LOW_GUARD, K2OS_KVA_THREAD0_AREA_HIGH - K2OS_KVA_THREAD0_STACK_LOW_GUARD, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->mpSegment = &gpProc0->SegObjPrimaryThreadStack;
    K2_ASSERT((pNode->mpSegment->mSegAndMemPageAttr & (K2OS_SEG_ATTR_TYPE_MASK | K2OS_MEMPAGE_ATTR_MASK)) == (K2OS_SEG_ATTR_TYPE_THREAD | K2OS_MAPTYPE_KERN_DATA));
    K2_ASSERT(pNode->NonSeg.mType == K2OSKERN_VMNODE_TYPE_SEGMENT);
    sCheckPteRange(K2OS_KVA_THREAD0_AREA_LOW, K2OS_KVA_THREAD0_PHYS_BYTES / K2_VA32_MEMPAGE_BYTES, K2OS_MAPTYPE_KERN_DATA, KernPhysPageList_Proc);
    K2_ASSERT(((*((UINT32 *)K2OS_KVA_TO_PTE_ADDR(K2OS_KVA_THREAD0_STACK_LOW_GUARD))) & K2OSKERN_PTE_PRESENT_BIT) == 0);
    sCheckPteRangeUnmapped(K2OS_KVA_THREAD0_STACK_LOW_GUARD, 1);
    *((UINT32 *)K2OS_KVA_TO_PTE_ADDR(K2OS_KVA_THREAD0_STACK_LOW_GUARD)) = K2OSKERN_PTE_NP_STACK_GUARD;
    K2_ASSERT(((*((UINT32 *)K2OS_KVA_TO_PTE_ADDR(K2OS_KVA_THREAD0_STACK_LOW_GUARD))) & K2OSKERN_PTE_PRESENT_BIT) == 0);
    K2_ASSERT(((*((UINT32 *)K2OS_KVA_TO_PTE_ADDR(K2OS_KVA_THREAD0_STACK_LOW_GUARD))) & K2OSKERN_PTE_NP_MASK) == K2OSKERN_PTE_NP_STACK_GUARD);
    gpProc0->SegObjPrimaryThreadStack.SegTreeNode.mUserVal = K2OS_KVA_THREAD0_STACK_LOW_GUARD;
    gpProc0->SegObjPrimaryThreadStack.mPagesBytes = K2OS_KVA_THREAD0_AREA_HIGH - K2OS_KVA_THREAD0_STACK_LOW_GUARD;
    sAddSegmentToTree(&gpProc0->SegObjPrimaryThreadStack);

    // bottom up
    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_KERNVAMAP_BASE, K2OS_KVA_KERNVAMAP_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_VAMAP;
    // check for unallocated VM happens below after everything else

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_PHYSTRACKAREA_BASE, K2OS_KVA_PHYSTRACKAREA_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_PHYSTRACK;
    sVerifyPhysTrack();

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_TRANSTAB_BASE, K2_VA32_TRANSTAB_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_TRANSTAB;
    sCheckPteRange(K2OS_KVA_TRANSTAB_BASE, K2_VA32_TRANSTAB_SIZE / K2_VA32_MEMPAGE_BYTES, K2OS_MAPTYPE_KERN_DATA, KernPhysPageList_Paging);
    // IA32 one page, must be mapped to transtab base
    // A32 four pages, first must be mapped to transtab base
    // all pages must be on KernPhysPageList_Paging

    if (K2OS_KVA_EFIMAP_BASE != (K2OS_KVA_TRANSTAB_BASE + K2_VA32_TRANSTAB_SIZE))
    {
        // this area will not be mapped 
        stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_TRANSTAB_BASE + K2_VA32_TRANSTAB_SIZE, K2OS_KVA_EFIMAP_BASE - (K2OS_KVA_TRANSTAB_BASE + K2_VA32_TRANSTAB_SIZE), (K2HEAP_NODE **)&pNode);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
        pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
        pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_TRANSTAB_HOLE;
        sCheckPteRangeUnmapped(K2OS_KVA_TRANSTAB_BASE + K2_VA32_TRANSTAB_SIZE, K2OS_KVA_EFIMAP_BASE - (K2OS_KVA_TRANSTAB_BASE + K2_VA32_TRANSTAB_SIZE));
    }

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_EFIMAP_BASE, K2OS_KVA_EFIMAP_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_EFIMAP;
    sCheckPteRange(K2OS_KVA_EFIMAP_BASE, K2OS_KVA_EFIMAP_SIZE / K2_VA32_MEMPAGE_BYTES, K2OS_MAPTYPE_KERN_DATA, KernPhysPageList_KOver);

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_PTPAGECOUNT_BASE, K2_VA32_MEMPAGE_BYTES, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_PTPAGECOUNT;
    sCheckPteRange(K2OS_KVA_PTPAGECOUNT_BASE, 1, K2OS_MAPTYPE_KERN_DATA, KernPhysPageList_Paging);

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_LOADERPAGE_BASE, K2_VA32_MEMPAGE_BYTES, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_LOADER;
    sCheckPteRange(K2OS_KVA_LOADERPAGE_BASE, 1, K2OS_MAPTYPE_KERN_DATA, KernPhysPageList_KOver);

    gpProc0->SegObjSelf.Hdr.mObjType = K2OS_Obj_Segment;
    gpProc0->SegObjSelf.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
    gpProc0->SegObjSelf.Hdr.mRefCount = 0x7FFFFFFF;
    gpProc0->SegObjSelf.mSegAndMemPageAttr = K2OS_MAPTYPE_KERN_DATA | K2OS_SEG_ATTR_TYPE_PROCESS;
    gpProc0->SegObjSelf.Info.Process.mpProc = gpProc0;
    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, K2OS_KVA_PROC0_BASE, K2OS_KVA_PROC0_SIZE, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->mpSegment = &gpProc0->SegObjSelf;
    K2_ASSERT((pNode->mpSegment->mSegAndMemPageAttr & (K2OS_SEG_ATTR_TYPE_MASK | K2OS_MEMPAGE_ATTR_MASK)) == (K2OS_SEG_ATTR_TYPE_PROCESS | K2OS_MAPTYPE_KERN_DATA));
    K2_ASSERT(pNode->NonSeg.mType == K2OSKERN_VMNODE_TYPE_SEGMENT);
    sCheckPteRange(K2OS_KVA_PROC0_BASE, K2OS_KVA_PROC0_SIZE / K2_VA32_MEMPAGE_BYTES, K2OS_MAPTYPE_KERN_DATA, KernPhysPageList_Proc);
    gpProc0->SegObjSelf.SegTreeNode.mUserVal = K2OS_KVA_PROC0_BASE;
    gpProc0->SegObjSelf.mPagesBytes = K2OS_KVA_PROC0_SIZE;
    sAddSegmentToTree(&gpProc0->SegObjSelf);

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, gData.mpShared->LoadInfo.mFwTabPagesVirt, gData.mpShared->LoadInfo.mFwTabPageCount * K2_VA32_MEMPAGE_BYTES, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
    pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_FW_TABLES;
    sCheckPteRange(gData.mpShared->LoadInfo.mFwTabPagesVirt, gData.mpShared->LoadInfo.mFwTabPageCount, K2OS_MAPTYPE_KERN_READ, KernPhysPageList_Non_KData);

    if (gData.mpShared->LoadInfo.mDebugPageVirt != 0)
    {
        stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, gData.mpShared->LoadInfo.mDebugPageVirt, K2_VA32_MEMPAGE_BYTES, (K2HEAP_NODE **)&pNode);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
        pNode->NonSeg.mType = K2OSKERN_VMNODE_TYPE_RESERVED;
        pNode->NonSeg.mNodeInfo = K2OSKERN_VMNODE_RESERVED_DEBUG;
        // these are not checked for pagelist since they should technically not exist
    }

    //
    // now do already-loaded DLX
    //
    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, gData.DlxCrt.PageSeg.SegTreeNode.mUserVal, K2_VA32_MEMPAGE_BYTES, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    sInitMapDlxSegments(&gData.DlxCrt);
    pNode->mpSegment = &gData.DlxCrt.PageSeg;

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, gData.DlxHal.PageSeg.SegTreeNode.mUserVal, K2_VA32_MEMPAGE_BYTES, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    sInitMapDlxSegments(&gData.DlxHal);
    pNode->mpSegment = &gData.DlxHal.PageSeg;

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, gData.DlxAcpi.PageSeg.SegTreeNode.mUserVal, K2_VA32_MEMPAGE_BYTES, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    sInitMapDlxSegments(&gData.DlxAcpi);
    pNode->mpSegment = &gData.DlxAcpi.PageSeg;

    stat = K2HEAP_AllocNodeAt(&gData.KernVirtHeap, gpProc0->PrimaryModule.PageSeg.SegTreeNode.mUserVal, K2_VA32_MEMPAGE_BYTES, (K2HEAP_NODE **)&pNode);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
    sInitMapDlxSegments(&gpProc0->PrimaryModule);
    pNode->mpSegment = &gpProc0->PrimaryModule.PageSeg;

    sAddEfiRuntimeMappings();

    // now audit before hal runs
//    K2HEAP_Dump(&gData.KernVirtHeap, sDumpHeapNode);

    //
    // unallocated VM must not be mapped. all VM must be accounted for
    //
#if VIRT_AUDIT
    lastEnd = K2OS_KVA_KERN_BASE;
    pHeapNode = K2HEAP_GetFirstNode(&gData.KernVirtHeap);
    K2_ASSERT(pHeapNode != NULL);
    do {
        virtAddr = K2HEAP_NodeAddr(pHeapNode);
        pageCount = K2HEAP_NodeSize(pHeapNode);
        K2_ASSERT((pageCount & K2_VA32_MEMPAGE_OFFSET_MASK) == 0);
        K2_ASSERT(virtAddr == lastEnd);
        if (K2HEAP_NodeIsFree(pHeapNode))
        {
//            K2OSKERN_Debug("FREE %08X %8d\n", virtAddr, pageCount);
            // every page in this range must not be mapped
            pageCount /= K2_VA32_MEMPAGE_BYTES;
            do {
                KernArch_Translate(gpProc0, virtAddr, &ptIsPresent, &pte, &accessAttr);
                if (ptIsPresent)
                {
                    if (pte & (K2OSKERN_PTE_PRESENT_BIT | K2OSKERN_PTE_NP_BIT))
                    {
                        K2OSKERN_Debug("v %08X PTE %08X", virtAddr, pte);
                        K2_ASSERT(0);
                    }
                    virtAddr += K2_VA32_MEMPAGE_BYTES;
                    pageCount--;
                }
                else
                {
                    //
                    // virtAddr must be on a pagetable map bytes boundary
                    //
                    K2_ASSERT((virtAddr & (K2_VA32_PAGETABLE_MAP_BYTES - 1)) == 0);
                    K2_ASSERT(pageCount >= K2_VA32_ENTRIES_PER_PAGETABLE);
                    virtAddr += K2_VA32_PAGETABLE_MAP_BYTES;
                    pageCount -= K2_VA32_ENTRIES_PER_PAGETABLE;
                }
            } while (pageCount > 0);
        }
        else
        {
//            K2OSKERN_Debug("USED %08X %8d\n", virtAddr, pageCount);
            virtAddr += pageCount;
        }
        lastEnd = virtAddr;
        pHeapNode = K2HEAP_GetNextNode(&gData.KernVirtHeap, pHeapNode);
    } while (pHeapNode != NULL);
    K2_ASSERT(lastEnd == 0);
#endif

#if K2_TARGET_ARCH_IS_ARM
    sA32Init_BeforeVirt();
#endif
}

static void sInit_AfterHal(void)
{
    K2OSKERN_Debug("%d/%d nodes used in InitHeapNodes\n", INIT_HEAP_NODE_COUNT - sgInitHeapNodeList.mNodeCount, INIT_HEAP_NODE_COUNT);
}

static void sInit_Threaded(void)
{
    KernMem_Start();
}

void KernInit_Mem(void)
{
    switch (gData.mKernInitStage)
    {
    case KernInitStage_Before_Virt:
        sInit_BeforeVirt();
        break;
    case KernInitStage_After_Hal:
        sInit_AfterHal();
        break;
    case KernInitStage_Threaded:
        sInit_Threaded();
        break;
    default:
        break;
    }
}


