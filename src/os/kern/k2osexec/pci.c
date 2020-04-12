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

K2LIST_ANCHOR        gPci_SegList;
K2OSKERN_SEQLOCK     gPci_SeqLock;

typedef PCI_DEVICE * (*pf_GetPciDev)(ACPI_PCI_ID * PciId);

#if !K2_TARGET_ARCH_IS_ARM

static ACPI_MCFG_ALLOCATION sgCAMAlloc;
static PCI_SEGMENT          sgCAMSegment;
static BOOL                 sgUseECAM;

#else

#define sgUseECAM   TRUE

#endif

static pf_GetPciDev sgfGetPciDevFromId = NULL;

static 
PCI_DEVICE *
sECAM_GetPciDevFromId(
    ACPI_PCI_ID *   PciId
)
{
    PCI_SEGMENT *   pPciSeg;
    K2LIST_LINK *   pListLink;
    UINT32          funcIndexInSegment;
    K2TREE_NODE *   pTreeNode;
    PCI_DEVICE *    pPciDev;
    K2STAT          stat;
    UINT32          physAddr;
    UINT32          virtAddr;
    UINT32          first32;
    BOOL            disp;

    pPciSeg = NULL;
    pPciDev = NULL;

    disp = K2OSKERN_SeqIntrLock(&gPci_SeqLock);
    do {
        pListLink = gPci_SegList.mpHead;
        if (pListLink == NULL)
            break;

        do {
            pPciSeg = K2_GET_CONTAINER(PCI_SEGMENT, pListLink, PciSegListLink);
            if ((PciId->Bus >= pPciSeg->mpMcfgAlloc->StartBusNumber) &&
                (PciId->Bus <= pPciSeg->mpMcfgAlloc->EndBusNumber))
                break;
            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);

        if (pListLink == NULL)
            break;

        funcIndexInSegment =
            ((PciId->Bus - pPciSeg->mpMcfgAlloc->StartBusNumber) * (32 * 8)) +
            (PciId->Device * 8) +
            PciId->Function;

        K2_ASSERT(funcIndexInSegment < ((pPciSeg->mpMcfgAlloc->EndBusNumber - pPciSeg->mpMcfgAlloc->StartBusNumber + 1) * 32 * 8));

        pTreeNode = K2TREE_Find(&pPciSeg->PciDevTree, funcIndexInSegment);
        if (pTreeNode != NULL)
            pPciDev = K2_GET_CONTAINER(PCI_DEVICE, pTreeNode, PciDevTreeNode);

    } while (0);

    K2OSKERN_SeqIntrUnlock(&gPci_SeqLock, disp);

    if (pPciDev != NULL)
        return pPciDev;

    //
    // attempt to create the device now
    //

    if (pPciSeg == NULL)
        return NULL;

    physAddr = (funcIndexInSegment * K2_VA32_MEMPAGE_BYTES) + (UINT32)pPciSeg->mpMcfgAlloc->Address;

    stat = K2OSKERN_MapDevice(physAddr, 1, &virtAddr);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OSKERN_Debug("**********Map Device error\n");
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    do {
        first32 = *((UINT32 *)virtAddr);
        if (first32 == 0xFFFFFFFF)
        {
            stat = K2STAT_ERROR_NOT_FOUND;
            break;
        }

        K2OSKERN_Debug("        First32 = %08X\n", first32);

        //
        // some device is present here
        //

        pPciDev = (PCI_DEVICE *)K2OS_HeapAlloc(sizeof(PCI_DEVICE));
        if (pPciDev == NULL)
        {
            stat = K2STAT_ERROR_OUT_OF_MEMORY;
            break;
        }

        K2MEM_Zero(pPciDev, sizeof(PCI_DEVICE));

        pPciDev->PciId = *PciId;
        pPciDev->mVenLo_DevHi = first32;
        pPciDev->CfgPageMapping = virtAddr;
        pPciDev->mpPciSeg = pPciSeg;
        pPciDev->PciDevTreeNode.mUserVal = funcIndexInSegment;

        disp = K2OSKERN_SeqIntrLock(&gPci_SeqLock);

        K2TREE_Insert(&pPciSeg->PciDevTree, funcIndexInSegment, &pPciDev->PciDevTreeNode);

        K2OSKERN_SeqIntrUnlock(&gPci_SeqLock, disp);

    } while (0);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        stat = K2OSKERN_UnmapDevice(virtAddr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }

    return pPciDev;
}

static
ACPI_STATUS
sECAM_ReadPciConfiguration(
    ACPI_PCI_ID *   PciId,
    UINT32          Reg,
    UINT64 *        Value,
    UINT32          Width
)
{
    PCI_DEVICE *    pPciDev;
    UINT32          val32;

    pPciDev = sgfGetPciDevFromId(PciId);
    if (pPciDev == NULL)
    {
        *Value = 0xFFFFFFFFFFFFFFFFull;
        return AE_OK;
    }

    K2_ASSERT(pPciDev->PciId.Bus == PciId->Bus);
    K2_ASSERT(pPciDev->PciId.Device == PciId->Device);
    K2_ASSERT(pPciDev->PciId.Function == PciId->Function);

    val32 = (*((UINT32 *)(pPciDev->CfgPageMapping + (Reg & 0xFC)))) >> ((Reg & 3) * 8);

    switch (Width)
    {
    case 8:
        *Value = (UINT64)(val32 & 0xFF);
        break;
    case 16:
        *Value = (UINT64)(val32 & 0xFFFF);
        break;
    case 32:
        *Value = (UINT64)val32;
        break;
    default:
        *Value = 0;
        K2_ASSERT(0);
    }

    return AE_OK;
}

static
ACPI_STATUS
sECAM_WritePciConfiguration(
    ACPI_PCI_ID *   PciId,
    UINT32          Reg,
    UINT64          Value,
    UINT32          Width)
{
    PCI_DEVICE *    pPciDev;
    UINT32          val32;
    UINT32          mask32;
    UINT32          reg;

    pPciDev = sgfGetPciDevFromId(PciId);
    if (pPciDev == NULL)
        return AE_ERROR;

    K2_ASSERT(pPciDev->PciId.Bus == PciId->Bus);
    K2_ASSERT(pPciDev->PciId.Device == PciId->Device);
    K2_ASSERT(pPciDev->PciId.Function == PciId->Function);

    switch (Width)
    {
    case 8:
        val32 = ((UINT32)Value) & 0xFF;
        mask32 = 0xFF;
        break;
    case 16:
        val32 = ((UINT32)Value) & 0xFFFF;
        mask32 = 0xFFFF;
        break;
    case 32:
        val32 = ((UINT32)Value);
        mask32 = 0xFFFFFFFF;
        break;
    default:
        return AE_ERROR;
    }

    val32 <<= ((Reg & 3) * 8);
    mask32 <<= ((Reg & 3) * 8);

    reg = *((UINT32 *)(pPciDev->CfgPageMapping + (Reg & 0xFC)));
    reg = (reg & ~mask32) | val32;
    *((UINT32 *)(pPciDev->CfgPageMapping + (Reg & 0xFC))) = reg;

    return AE_OK;
}

#if !K2_TARGET_ARCH_IS_ARM

static
PCI_DEVICE *
sCAM_GetPciDevFromId(
    ACPI_PCI_ID *   PciId
)
{
    UINT32          devIndexOnBus;
    K2TREE_NODE *   pTreeNode;
    PCI_DEVICE *    pPciDev;
    UINT64          first32;
    ACPI_STATUS     acpiStatus;

    devIndexOnBus =
        (PciId->Device * 8) +
        PciId->Function;

    K2_ASSERT(devIndexOnBus < (32 * 8));

    pTreeNode = K2TREE_Find(&sgCAMSegment.PciDevTree, devIndexOnBus);

    if (pTreeNode != NULL)
        return K2_GET_CONTAINER(PCI_DEVICE, pTreeNode, PciDevTreeNode);

    acpiStatus = AcpiOsReadPciConfiguration(PciId, 0, &first32, 32);
    if (ACPI_FAILURE(acpiStatus))
        return NULL;

    if ((first32 & 0xFFFFFFFFull) == 0xFFFFFFFFull)
        return NULL;

    //
    // something is there
    //
    pPciDev = (PCI_DEVICE *)K2OS_HeapAlloc(sizeof(PCI_DEVICE));
    if (pPciDev == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    K2MEM_Zero(pPciDev, sizeof(PCI_DEVICE));

    pPciDev->PciId = *PciId;
    pPciDev->mVenLo_DevHi = (UINT32)first32;
    pPciDev->CfgPageMapping = 0;
    pPciDev->mpPciSeg = NULL;
    pPciDev->PciDevTreeNode.mUserVal = devIndexOnBus;
    K2TREE_Insert(&sgCAMSegment.PciDevTree, devIndexOnBus, &pPciDev->PciDevTreeNode);

    return pPciDev;
}
#endif

void Pci_Init(void)
{
    K2STAT                  stat;
    UINT32                  sizeEnt;
    UINT32                  entCount;
    UINT32                  prevEnd;
    PCI_SEGMENT *           pPciSeg;
    ACPI_MCFG_ALLOCATION *  pAlloc;
    PHYS_HEAPNODE *         pPhysNode;

    K2LIST_Init(&gPci_SegList);
    K2OSKERN_SeqIntrInit(&gPci_SeqLock);

#if !K2_TARGET_ARCH_IS_ARM
    K2MEM_Zero(&sgCAMAlloc, sizeof(sgCAMAlloc));
    K2MEM_Zero(&sgCAMSegment, sizeof(sgCAMSegment));
    sgUseECAM = FALSE;

    if ((gpMCFG != NULL) &&
        (gpMCFG->Header.Length > sizeof(ACPI_TABLE_MCFG)))
    {
        //
        // ECAM possible - check for existence of buses
        //
        sizeEnt = gpMCFG->Header.Length - sizeof(ACPI_TABLE_MCFG);
        entCount = sizeEnt / sizeof(ACPI_MCFG_ALLOCATION);
        if (entCount > 0)
        {
            //
            // entries exist in MCFG, so use ECAM
            //
            sgUseECAM = TRUE;
        }
    }

    if (!sgUseECAM)
    {
        sgCAMSegment.mpMcfgAlloc = &sgCAMAlloc;
        K2TREE_Init(&sgCAMSegment.PciDevTree, NULL);
        K2LIST_AddAtTail(&gPci_SegList, &sgCAMSegment.PciSegListLink);

        sgfGetPciDevFromId = sCAM_GetPciDevFromId;

        return;
    }
#endif

    //
    // if we get here, sgUseECAM is TRUE
    //
    K2_ASSERT(sgUseECAM);

    sgfGetPciDevFromId = sECAM_GetPciDevFromId;

    stat = K2OSACPI_SetPciConfigOverride(
        sECAM_ReadPciConfiguration,
        sECAM_WritePciConfiguration
    );
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    //
    // add ECAM address ranges to physical map
    //
    if ((gpMCFG) && (gpMCFG->Header.Length > sizeof(ACPI_TABLE_MCFG)))
    {
        sizeEnt = gpMCFG->Header.Length - sizeof(ACPI_TABLE_MCFG);
        entCount = sizeEnt / sizeof(ACPI_MCFG_ALLOCATION);
        if (entCount > 0)
        {
            pAlloc = (ACPI_MCFG_ALLOCATION *)(((UINT8 *)gpMCFG) + sizeof(ACPI_TABLE_MCFG));
            prevEnd = pAlloc->EndBusNumber - pAlloc->StartBusNumber + 1;
            if (prevEnd > 0)
            {
                do {
                    stat = K2HEAP_AllocNodeAt(&gPhys_SpaceHeap,
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
                    K2LIST_AddAtTail(&gPci_SegList, &pPciSeg->PciSegListLink);

                    pAlloc = (ACPI_MCFG_ALLOCATION *)(((UINT8 *)pAlloc) + sizeof(ACPI_MCFG_ALLOCATION));
                } while (--entCount);
            }
        }
    }
}

static
void
sDiscoverOneBus(
    UINT32 aSegmentIx,
    UINT32 aBusIx
)
{
    UINT32          deviceIx;
    UINT32          functionIx;
    ACPI_PCI_ID     pciId;
    PCI_DEVICE *    pPciDev;

    pciId.Segment = aSegmentIx;
    pciId.Bus = aBusIx;

    for (deviceIx = 0; deviceIx < 32; deviceIx++)
    {
        pciId.Device = deviceIx;
        for (functionIx = 0; functionIx < 8; functionIx++)
        {
            pciId.Function = functionIx;
            pPciDev = sgfGetPciDevFromId(&pciId);
            if (pPciDev != NULL)
            {
                K2OSKERN_Debug("PCI: Seg %3d Bus %3d Dev %2d Func %d  VEN %04X DEV %04x\n",
                    pPciDev->PciId.Segment,
                    pPciDev->PciId.Bus,
                    pPciDev->PciId.Device,
                    pPciDev->PciId.Function,
                    pPciDev->mVenLo_DevHi & 0xFFFF,
                    (pPciDev->mVenLo_DevHi >> 16) & 0xFFFF);
            }
        }
    }
}

static
void
sDiscoverBusRange(
    UINT32 aSegmentIx,
    UINT32 aFirstBus,
    UINT32 aLastBus
)
{
    UINT32 ix;
    for (ix = aFirstBus; ix <= aLastBus; ix++)
    {
        sDiscoverOneBus(aSegmentIx, ix);
    }
}

void Pci_Discover(void)
{
    K2LIST_LINK * pListLink;
    PCI_SEGMENT * pPciSeg;

    //
    // pci discovery - happens before ACPI discovery
    //
    K2OSKERN_Debug("\n+PCI DISCOVERY\n");

#if !K2_TARGET_ARCH_IS_ARM
    if (!sgUseECAM)
    {
        sDiscoverBusRange(0, 0, 255);
        K2OSKERN_Debug("-PCI DISCOVERY\n\n");
        return;
    }
#endif
    
    K2_ASSERT(sgUseECAM);

    pListLink = gPci_SegList.mpHead;
    if (pListLink == NULL)
    {
        K2OSKERN_Debug("-PCI DISCOVERY\n\n");
        return;
    }
    do {
        pPciSeg = K2_GET_CONTAINER(PCI_SEGMENT, pListLink, PciSegListLink);
        sDiscoverBusRange(pPciSeg->mpMcfgAlloc->PciSegment, pPciSeg->mpMcfgAlloc->StartBusNumber, pPciSeg->mpMcfgAlloc->EndBusNumber);
        pListLink = pListLink->mpNext;
    } while (pListLink != NULL);

    K2OSKERN_Debug("-PCI DISCOVERY\n\n");
}

