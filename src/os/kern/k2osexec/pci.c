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

typedef DEV_NODE_PCI * (*pf_GetPciDev)(ACPI_PCI_ID * PciId);

#if !K2_TARGET_ARCH_IS_ARM

static ACPI_MCFG_ALLOCATION sgCAMAlloc;
static PCI_SEGMENT          sgCAMSegment;
static BOOL                 sgUseECAM;

#else

#define sgUseECAM   TRUE

#endif

static pf_GetPciDev sgfGetPciDevFromId = NULL;

//static 
DEV_NODE_PCI *
sECAM_GetPciDevFromId(
    ACPI_PCI_ID *   PciId
)
{
    PCI_SEGMENT *   pPciSeg;
    K2LIST_LINK *   pListLink;
    UINT32          funcIndexInSegment;
    K2TREE_NODE *   pTreeNode;
    DEV_NODE_PCI *  pPciDev;
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
        {
            pPciDev = K2_GET_CONTAINER(DEV_NODE_PCI, pTreeNode, PciTreeNode);
        }

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

        //
        // some device is present here
        //

        pPciDev = (DEV_NODE_PCI *)K2OS_HeapAlloc(sizeof(DEV_NODE_PCI));
        if (pPciDev == NULL)
        {
            stat = K2STAT_ERROR_OUT_OF_MEMORY;
            break;
        }

        K2MEM_Zero(pPciDev, sizeof(DEV_NODE_PCI));

        pPciDev->Id = *PciId;
        pPciDev->mVenLo_DevHi = first32;
        pPciDev->mpSeg = pPciSeg;
        pPciDev->mVirtConfigAddr = virtAddr;
        pPciDev->PciTreeNode.mUserVal = funcIndexInSegment;

        disp = K2OSKERN_SeqIntrLock(&gPci_SeqLock);

        K2TREE_Insert(&pPciSeg->PciDevTree, funcIndexInSegment, &pPciDev->PciTreeNode);

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

//static
ACPI_STATUS
sECAM_ReadPciConfiguration(
    ACPI_PCI_ID *   PciId,
    UINT32          Reg,
    UINT64 *        Value,
    UINT32          Width
)
{
    DEV_NODE_PCI *  pPciDev;
    UINT32          val32;

    pPciDev = sgfGetPciDevFromId(PciId);
    if (pPciDev == NULL)
    {
        *Value = 0xFFFFFFFFFFFFFFFFull;
        return AE_OK;
    }

    K2_ASSERT(pPciDev->Id.Bus == PciId->Bus);
    K2_ASSERT(pPciDev->Id.Device == PciId->Device);
    K2_ASSERT(pPciDev->Id.Function == PciId->Function);

    val32 = (*((UINT32 *)(pPciDev->mVirtConfigAddr + (Reg & 0xFC)))) >> ((Reg & 3) * 8);

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

//static
ACPI_STATUS
sECAM_WritePciConfiguration(
    ACPI_PCI_ID *   PciId,
    UINT32          Reg,
    UINT64          Value,
    UINT32          Width)
{
    DEV_NODE_PCI *  pPciDev;
    UINT32          val32;
    UINT32          mask32;
    UINT32          reg;

    pPciDev = sgfGetPciDevFromId(PciId);
    if (pPciDev == NULL)
        return AE_ERROR;

    K2_ASSERT(pPciDev->Id.Bus == PciId->Bus);
    K2_ASSERT(pPciDev->Id.Device == PciId->Device);
    K2_ASSERT(pPciDev->Id.Function == PciId->Function);

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

    reg = *((UINT32 *)(pPciDev->mVirtConfigAddr + (Reg & 0xFC)));
    reg = (reg & ~mask32) | val32;
    *((UINT32 *)(pPciDev->mVirtConfigAddr + (Reg & 0xFC))) = reg;

    return AE_OK;
}

#if !K2_TARGET_ARCH_IS_ARM

//static
DEV_NODE_PCI *
sCAM_GetPciDevFromId(
    ACPI_PCI_ID *   PciId
)
{
    UINT32          devIndexOnBus;
    K2TREE_NODE *   pTreeNode;
    DEV_NODE_PCI *  pPciDev;
    UINT64          first32;
    ACPI_STATUS     acpiStatus;
    BOOL            disp;

    devIndexOnBus =
        (PciId->Device * 8) +
        PciId->Function;

    K2_ASSERT(devIndexOnBus < (32 * 8));

    pTreeNode = K2TREE_Find(&sgCAMSegment.PciDevTree, devIndexOnBus);

    if (pTreeNode != NULL)
        return K2_GET_CONTAINER(DEV_NODE_PCI, pTreeNode, PciTreeNode);

    acpiStatus = AcpiOsReadPciConfiguration(PciId, 0, &first32, 32);
    if (ACPI_FAILURE(acpiStatus))
        return NULL;

    if ((first32 & 0xFFFFFFFFull) == 0xFFFFFFFFull)
        return NULL;

    //
    // something is there
    //
    pPciDev = (DEV_NODE_PCI *)K2OS_HeapAlloc(sizeof(DEV_NODE_PCI));
    if (pPciDev == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    K2MEM_Zero(pPciDev, sizeof(DEV_NODE_PCI));

    pPciDev->Id = *PciId;
    pPciDev->mVenLo_DevHi = (UINT32)first32;
    pPciDev->mpSeg = &sgCAMSegment;
    pPciDev->mVirtConfigAddr = 0;
    pPciDev->PciTreeNode.mUserVal = devIndexOnBus;

    disp = K2OSKERN_SeqIntrLock(&gPci_SeqLock);

    K2TREE_Insert(&sgCAMSegment.PciDevTree, devIndexOnBus, &pPciDev->PciTreeNode);

    K2OSKERN_SeqIntrUnlock(&gPci_SeqLock, disp);

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
    ACPI_TABLE_MCFG *       pMCFG;
    ACPI_STATUS             acpiStatus;

    K2LIST_Init(&gPci_SegList);
    K2OSKERN_SeqIntrInit(&gPci_SeqLock);

    pMCFG = NULL;
    acpiStatus = AcpiGetTable(ACPI_SIG_MCFG, 0, (ACPI_TABLE_HEADER **)&pMCFG);
    if (ACPI_FAILURE(acpiStatus))
    {
        pMCFG = NULL;
    }
    else
    {
        K2_ASSERT(pMCFG != NULL);
    }

#if !K2_TARGET_ARCH_IS_ARM
    K2MEM_Zero(&sgCAMAlloc, sizeof(sgCAMAlloc));
    K2MEM_Zero(&sgCAMSegment, sizeof(sgCAMSegment));
    sgUseECAM = FALSE;

    if ((pMCFG != NULL) &&
        (pMCFG->Header.Length > sizeof(ACPI_TABLE_MCFG)))
    {
        //
        // ECAM possible - check for existence of buses
        //
        sizeEnt = pMCFG->Header.Length - sizeof(ACPI_TABLE_MCFG);
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
    if ((pMCFG) && (pMCFG->Header.Length > sizeof(ACPI_TABLE_MCFG)))
    {
        sizeEnt = pMCFG->Header.Length - sizeof(ACPI_TABLE_MCFG);
        entCount = sizeEnt / sizeof(ACPI_MCFG_ALLOCATION);
        if (entCount > 0)
        {
            pAlloc = (ACPI_MCFG_ALLOCATION *)(((UINT8 *)pMCFG) + sizeof(ACPI_TABLE_MCFG));
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
    DEV_NODE * apDevNode
);


//static
void
sDiscoveredChildPciDevice(
    DEV_NODE *      apParentDev,
    DEV_NODE_PCI *  apChildPci
)
{
    K2LIST_LINK *   pListLink;
    DEV_NODE *      pChildDev;
    ACPI_STATUS     acpiStatus;
    UINT64          val64;
    PCI_SEGMENT *   pPciSeg;
    UINT32          subBus;

    pListLink = apParentDev->ChildList.mpHead;
    if (pListLink != NULL)
    {
        do {
            pChildDev = K2_GET_CONTAINER(DEV_NODE, pListLink, ChildListLink);
            pListLink = pListLink->mpNext;
            if (pChildDev->mpAcpiInfo != NULL)
            {
                // 
                // high word of address is device
                // low word of device is function
                //
                if ((((pChildDev->mpAcpiInfo->Address >> 16) & 0xFFFF) == (UINT32)apChildPci->Id.Device) &&
                    ((pChildDev->mpAcpiInfo->Address & 0xFFFF) == (UINT32)apChildPci->Id.Function))
                {
                    //
                    // match!
                    //
//                    K2OSKERN_Debug("Matched PCI Device at %d/%d (VID %04X PID %04X) with Acpi Dev %08X\n",
//                        apChildPci->Id.Device, apChildPci->Id.Function, 
//                        apChildPci->mVenLo_DevHi & 0xFFFF, (apChildPci->mVenLo_DevHi >> 16) & 0xFFFF,
//                        pChildDev);
                    K2_ASSERT(pChildDev->mpPci == NULL);
                    pChildDev->mpPci = apChildPci;
                    break;
                }
            }
        } while (pListLink != NULL);
    }

    if (pListLink == NULL)
    {
//        K2OSKERN_Debug("Did not match %d/%d with any known ACPI node\n",
//            apChildPci->Id.Device, apChildPci->Id.Function);

        pChildDev = (DEV_NODE *)K2OS_HeapAlloc(sizeof(DEV_NODE));
        K2_ASSERT(pChildDev != NULL);
        K2MEM_Zero(pChildDev, sizeof(DEV_NODE));

        pChildDev->mpPci = apChildPci;

        pChildDev->mpParent = apParentDev;
        K2LIST_Init(&pChildDev->ChildList);
        K2LIST_AddAtTail(&apParentDev->ChildList, &pChildDev->ChildListLink);

        K2TREE_Insert(&gDev_Tree, pChildDev->DevTreeNode.mUserVal, &pChildDev->DevTreeNode);
    }

    K2_ASSERT(pChildDev != NULL);

    //
    // ok - now is this new thing we just created or latched onto a bus bridge?
    //
    acpiStatus = AcpiOsReadPciConfiguration(&apChildPci->Id, 0xE, &val64, 8);
    if (ACPI_FAILURE(acpiStatus))
        return;

    if (0 == (val64 & 0x1))
        return;

    acpiStatus = AcpiOsReadPciConfiguration(&apChildPci->Id, 0x18, &val64, 32);
    if (ACPI_FAILURE(acpiStatus))
        return;

    subBus = (UINT32)((val64 >> 8) & 0xFF);

    K2_ASSERT((val64 & 0xFF) == apChildPci->Id.Bus);
    K2_ASSERT(subBus != apChildPci->Id.Bus);

    //
    // we have a PCI-PCI bus bridge
    //
    pChildDev->mPciBusBridgeFlags =
        DEV_NODE_PCIBUSBRIDGE_ISBUSROOT |
        ((subBus << DEV_NODE_PCIBUSBRIDGE_BUSID_SHIFT) & DEV_NODE_PCIBUSBRIDGE_BUSID_MASK) |
        ((((UINT32)apChildPci->Id.Segment) << DEV_NODE_PCIBUSBRIDGE_SEGID_SHIFT) & DEV_NODE_PCIBUSBRIDGE_SEGID_MASK);

    pListLink = gPci_SegList.mpHead;
    K2_ASSERT(pListLink != NULL);
    do {
        pPciSeg = K2_GET_CONTAINER(PCI_SEGMENT, pListLink, PciSegListLink);
        if ((subBus >= pPciSeg->mpMcfgAlloc->StartBusNumber) &&
            (subBus <= pPciSeg->mpMcfgAlloc->EndBusNumber))
            break;
        pListLink = pListLink->mpNext;
    } while (pListLink != NULL);

    // if this fires pci Bus index specified is not listed in MCFG
    // or is not zero when there is no MCFG
    K2_ASSERT(pListLink != NULL);

    sDiscoverOneBus(pChildDev);
}

static
void
sDiscoverOneBus(
    DEV_NODE * apDevNode
)
{
    UINT32          deviceIx;
    UINT32          functionIx;
    ACPI_PCI_ID     pciId;
    DEV_NODE_PCI *  pPciDev;

    K2_ASSERT(0 != (apDevNode->mPciBusBridgeFlags & DEV_NODE_PCIBUSBRIDGE_ISBUSROOT));

    pciId.Segment = (apDevNode->mPciBusBridgeFlags & DEV_NODE_PCIBUSBRIDGE_SEGID_MASK) >> DEV_NODE_PCIBUSBRIDGE_SEGID_SHIFT;
    pciId.Bus = (apDevNode->mPciBusBridgeFlags & DEV_NODE_PCIBUSBRIDGE_BUSID_MASK) >> DEV_NODE_PCIBUSBRIDGE_BUSID_SHIFT;

    for (deviceIx = 0; deviceIx < 32; deviceIx++)
    {
        pciId.Device = deviceIx;
        pciId.Function = functionIx = 0;
        pPciDev = sgfGetPciDevFromId(&pciId);
        if (pPciDev != NULL)
        {
            //
            // if function 0 exists we have to check all of them
            //
            do {
                sDiscoveredChildPciDevice(apDevNode, pPciDev);
                while (++functionIx < 8)
                {
                    pciId.Function = functionIx;
                    pPciDev = sgfGetPciDevFromId(&pciId);
                    if (pPciDev != NULL)
                        break;
                }
            } while (functionIx < 8);
        }
    }
}

void Pci_DiscoverBridgeFromAcpi(DEV_NODE *apDevNode)
{
    K2LIST_LINK *   pListLink;
    PCI_SEGMENT *   pPciSeg;
    ACPI_STATUS     acpiStatus;
    UINT64          Value64;
    UINT32          busIx;
    UINT32          segIx;

    //
    // device is a PCI or PCI EXPRESS bridge
    // 
    K2_ASSERT(apDevNode->mpAcpiInfo != NULL);

    //
    // what segment is this bridge on?
    //
    acpiStatus = K2OSACPI_RunDeviceNumericMethod(
        (ACPI_HANDLE)apDevNode->DevTreeNode.mUserVal,
        "_SEG",
        &Value64);
    if (!ACPI_FAILURE(acpiStatus))
    {
        segIx = (UINT32)(Value64 & 0xFFFF);
    }
    else
    {
        segIx = 0;
    }

    //
    // what is the bridge's bus number
    //
    acpiStatus = K2OSACPI_RunDeviceNumericMethod(
        (ACPI_HANDLE)apDevNode->DevTreeNode.mUserVal,
        "_BBN",
        &Value64);
    if (!ACPI_FAILURE(acpiStatus))
    {
        busIx = (UINT32)(Value64 & 0xFFFF);
    }
    else
    {
        busIx = 0;
    }

    apDevNode->mPciBusBridgeFlags = 
        DEV_NODE_PCIBUSBRIDGE_ISBUSROOT |
        ((busIx << DEV_NODE_PCIBUSBRIDGE_BUSID_SHIFT) & DEV_NODE_PCIBUSBRIDGE_BUSID_MASK) |
        ((segIx << DEV_NODE_PCIBUSBRIDGE_SEGID_SHIFT) & DEV_NODE_PCIBUSBRIDGE_SEGID_MASK);

    pListLink = gPci_SegList.mpHead;
    K2_ASSERT(pListLink != NULL);
    do {
        pPciSeg = K2_GET_CONTAINER(PCI_SEGMENT, pListLink, PciSegListLink);
        if ((busIx >= pPciSeg->mpMcfgAlloc->StartBusNumber) &&
            (busIx <= pPciSeg->mpMcfgAlloc->EndBusNumber))
            break;
        pListLink = pListLink->mpNext;
    } while (pListLink != NULL);

    // if this fires pci Bus index specified is not listed in MCFG
    // or is not zero when there is no MCFG
    K2_ASSERT(pListLink != NULL);   

#if !K2_TARGET_ARCH_IS_ARM
    if (!sgUseECAM)
    {
        K2_ASSERT(segIx == 0);
        K2_ASSERT(busIx == 0);
    }
#endif

    sDiscoverOneBus(apDevNode);
}

