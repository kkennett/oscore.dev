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

#include "k2osexec.h"

#define DESC_BUFFER_BYTES   (((sizeof(K2EFI_MEMORY_DESCRIPTOR) * 2) + 4) & ~3)

void
InitPhys(
    K2OSEXEC_INIT_INFO * apInitInfo
)
{
    UINT32                      entCount;
    UINT8                       descBuffer[DESC_BUFFER_BYTES];
    K2EFI_MEMORY_DESCRIPTOR *   pDesc = (K2EFI_MEMORY_DESCRIPTOR *)descBuffer;
    UINT8 *                     pScan;
    UINT32                      ix;
    K2STAT                      stat;
    PHYS_HEAPNODE *             pPhysNode;
    PHYS_HEAPNODE *             pPhysNext;
    UINT32                      prevEnd;
    UINT32                      sizeEnt;
    ACPI_MCFG_ALLOCATION *      pAlloc;
    PCI_SEGMENT *               pPciSeg;

    K2_ASSERT(apInitInfo->mEfiMemDescSize <= DESC_BUFFER_BYTES);
    entCount = apInitInfo->mEfiMapSize / apInitInfo->mEfiMemDescSize;
    pScan = (UINT8 *)K2OS_KVA_EFIMAP_BASE;

    for (ix = 0; ix < entCount; ix++)
    {
        K2MEM_Copy(descBuffer, pScan, sizeof(K2EFI_MEMORY_DESCRIPTOR));
        pScan += apInitInfo->mEfiMemDescSize;

        //
        // evaluate descriptor at pDesc
        //
        stat = K2HEAP_AddFreeSpaceNode(
            &gPhysSpaceHeap,
            (UINT32)(((UINT64)pDesc->PhysicalStart) & 0x00000000FFFFFFFFull),
            (UINT32)(pDesc->NumberOfPages * K2_VA32_MEMPAGE_BYTES),
            NULL);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));

        if ((pDesc->Type == K2EFI_MEMTYPE_MappedIO) ||
            (pDesc->Type == K2EFI_MEMTYPE_MappedIOPort))
        {
            stat = K2HEAP_AllocNodeAt(
                &gPhysSpaceHeap, 
                (UINT32)(((UINT64)pDesc->PhysicalStart) & 0x00000000FFFFFFFFull),
                (UINT32)(pDesc->NumberOfPages * K2_VA32_MEMPAGE_BYTES),
                (K2HEAP_NODE **)&pPhysNode);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
            if (pDesc->Type == K2EFI_MEMTYPE_MappedIO)
                pPhysNode->mDisp = PHYS_DISP_MAPPEDIO;
            else
                pPhysNode->mDisp = PHYS_DISP_MAPPEDPORT;
        }
    }

    //
    // now convert free space entries to contiguous allocations
    //
    pPhysNode = (PHYS_HEAPNODE *)K2HEAP_GetFirstNode(&gPhysSpaceHeap);
    K2_ASSERT(pPhysNode != NULL);
    do {
        pPhysNext = (PHYS_HEAPNODE *)K2HEAP_GetNextNode(&gPhysSpaceHeap, &pPhysNode->HeapNode);
        if (K2HEAP_NodeIsFree(&pPhysNode->HeapNode))
        {
            stat = K2HEAP_AllocNodeAt(
                &gPhysSpaceHeap,
                K2HEAP_NodeAddr(&pPhysNode->HeapNode),
                K2HEAP_NodeSize(&pPhysNode->HeapNode),
                (K2HEAP_NODE **)&pPhysNode);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
            pPhysNode->mDisp = PHYS_DISP_MEMORY;
        }
        pPhysNode = pPhysNext;
    } while (pPhysNode != NULL);

    //
    // now insert free space everywhere there is holes
    //
    prevEnd = 0;
    pPhysNode = (PHYS_HEAPNODE *)K2HEAP_GetFirstNode(&gPhysSpaceHeap);
    K2_ASSERT(pPhysNode != NULL);
    do {
        pPhysNext = (PHYS_HEAPNODE *)K2HEAP_GetNextNode(&gPhysSpaceHeap, &pPhysNode->HeapNode);
        if (K2HEAP_NodeAddr(&pPhysNode->HeapNode) != prevEnd)
        {
            //
            // insert free space between prevEnd and pPhysNode
            //
            stat = K2HEAP_AddFreeSpaceNode(
                &gPhysSpaceHeap,
                prevEnd,
                K2HEAP_NodeAddr(&pPhysNode->HeapNode) - prevEnd,
                NULL);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
        }
        prevEnd = K2HEAP_NodeAddr(&pPhysNode->HeapNode) + K2HEAP_NodeSize(&pPhysNode->HeapNode);
        if (pPhysNext == NULL)
            break;
        pPhysNode = pPhysNext;
    } while (1);
    //
    // pPhysNode points to last node in the space,  prevEnd to the end of it
    //
    if (prevEnd != 0)
    {
        stat = K2HEAP_AddFreeSpaceNode(
            &gPhysSpaceHeap,
            prevEnd,
            0 - prevEnd,
            NULL);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }

    //
    // we have our physical space map now, with free space available to be allocated
    //
    if ((gpMCFG) && (gpMCFG->Header.Length > sizeof(ACPI_TABLE_MCFG)))
    {
        //
        // find memory mapped io segments for PCI bridges
        // before pci configuration accesses happen since on non-x32 
        // platforms the 'old' configuration mechanism doesnt exist
        //
        sizeEnt = gpMCFG->Header.Length - sizeof(ACPI_TABLE_MCFG);
        entCount = sizeEnt / sizeof(ACPI_MCFG_ALLOCATION);
        if (entCount > 0)
        {
            pAlloc = (ACPI_MCFG_ALLOCATION *)(((UINT8 *)gpMCFG) + sizeof(ACPI_TABLE_MCFG));
            prevEnd = pAlloc->EndBusNumber - pAlloc->StartBusNumber + 1;
            if (prevEnd > 0)
            {
                do {
                    stat = K2HEAP_AllocNodeAt(&gPhysSpaceHeap,
                        (UINT32)(pAlloc->Address & 0xFFFFFFFF),
                        prevEnd * 0x100000,
                        (K2HEAP_NODE **)&pPhysNode
                    );
                    pPhysNode->mDisp = PHYS_DISP_PCI_SEGMENT;
                    K2_ASSERT(!K2STAT_IS_ERROR(stat));

                    pPciSeg = (PCI_SEGMENT *)K2OS_HeapAlloc(sizeof(PCI_SEGMENT));
                    K2MEM_Zero(pPciSeg, sizeof(PCI_SEGMENT));
                    K2_ASSERT(pPciSeg != NULL);
                    pPciSeg->mpMcfgAlloc = pAlloc;
                    pPciSeg->mpPhysHeapNode = pPhysNode;
                    K2TREE_Init(&pPciSeg->PciDevTree, NULL);
                    K2LIST_AddAtTail(&gPciSegList, &pPciSeg->PciSegListLink);

                    pAlloc = (ACPI_MCFG_ALLOCATION *)(((UINT8 *)pAlloc) + sizeof(ACPI_MCFG_ALLOCATION));
                } while (--entCount);
            }
        }
    }
}
