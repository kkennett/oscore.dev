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

#include "ik2osexec.h"

K2HEAP_ANCHOR gRes_IoSpaceHeap;
K2OS_CRITSEC  gRes_IoSpaceSec;

void 
sRes_QueryPciDeviceBars(
    DEV_NODE *      apDevNode, 
    DEV_NODE_PCI *  apPci
)
{
    ACPI_STATUS         acpiStatus;
    UINT8               val8;
    UINT32              barRegOffset;
    UINT32              numBars;
    UINT32              barSave;
    UINT32              barVal;
    UINT64              val64;
    UINT32              ix;
    UINT32              maskVal;
    BOOL                disp;
    K2STAT              stat;
    UINT32 *            pBar;
    RES_IO_HEAPNODE *   pResIoHeapNode;
    PHYS_HEAPNODE *     pPhysHeapNode;
    K2TREE_NODE *       pTreeNode;

    barRegOffset = K2_FIELDOFFSET(PCICFG, AsType0.mBar0);

    disp = K2OSKERN_SetIntr(FALSE);

    //
    // get resources described by BARs
    //
    val8 = apPci->PciCfg.AsTypeX.mHeaderType & 0x3;
    if (val8 < 2)
    {
        if (val8 == 0)
            numBars = 6;
        else
            numBars = 2;
        for (ix = 0; ix < numBars; ix++)
        {
            acpiStatus = AcpiOsReadPciConfiguration(&apPci->Id, barRegOffset, &val64, 32);
            if (!ACPI_FAILURE(acpiStatus))
            {
                barSave = (UINT32)(val64 & 0xFFFFFFFFull);
                if (barSave == 0)
                    break;

                apPci->mBarsFound++;
                if (barSave & PCI_BAR_TYPE_IO_BIT)
                    maskVal = PCI_BAR_BASEMASK_IO;
                else
                    maskVal = PCI_BAR_BASEMASK_MEMORY;
                val64 = maskVal | (barSave & ~maskVal);
                acpiStatus = AcpiOsWritePciConfiguration(&apPci->Id, barRegOffset, val64, 32);
                if (!ACPI_FAILURE(acpiStatus))
                {
                    acpiStatus = AcpiOsReadPciConfiguration(&apPci->Id, barRegOffset, &val64, 32);
                    if (!ACPI_FAILURE(acpiStatus))
                    {
                        barVal = ((UINT32)val64) & maskVal;
                        acpiStatus = AcpiOsWritePciConfiguration(&apPci->Id, barRegOffset, (UINT64)barSave, 32);
                        if (!ACPI_FAILURE(acpiStatus))
                        {
                            apPci->mBarSize[ix] = barVal = (~barVal) + 1;
                        }
                    }
                }
            }
            barRegOffset += sizeof(UINT32);
        }
    }

    K2OSKERN_SetIntr(disp);

    pBar = &apPci->PciCfg.AsType0.mBar0;
    for (ix = 0; ix < apPci->mBarsFound; ix++)
    {
        barSave = pBar[ix];
        barVal = apPci->mBarSize[ix];

        K2_ASSERT(barSave != 0);

        if (barSave & PCI_BAR_TYPE_IO_BIT)
        {
            K2_ASSERT((barSave & 0xFFFF0000) == 0);
//            K2OSKERN_Debug("Adding PCI IO space reservation - %04X, %04X\n",
//                barSave & PCI_BAR_BASEMASK_IO, barVal);
            stat = K2HEAP_AllocNodeAt(&gRes_IoSpaceHeap, barSave & PCI_BAR_BASEMASK_IO, barVal, (K2HEAP_NODE **)&pResIoHeapNode);
            if (K2STAT_IS_ERROR(stat))
            {
                K2OSKERN_Debug("***PCI IO space reservation failed\n");
            }
            else
            {
                pResIoHeapNode->mpDevNode = apDevNode;
                K2LIST_AddAtTail(&apDevNode->IoList, &pResIoHeapNode->DevNodeIoListLink);
            }
        }
        else
        {
            stat = K2HEAP_AllocNodeAt(&gPhys_SpaceHeap, barSave & PCI_BAR_BASEMASK_MEMORY, barVal, (K2HEAP_NODE **)&pPhysHeapNode);
            if (K2STAT_IS_ERROR(stat))
            {
                pPhysHeapNode = NULL;
                pTreeNode = K2TREE_Find(&gPhys_SpaceHeap.AddrTree, barSave & PCI_BAR_BASEMASK_MEMORY);
                if (pTreeNode != NULL)
                {
                    pPhysHeapNode = K2_GET_CONTAINER(PHYS_HEAPNODE, pTreeNode, HeapNode.AddrTreeNode);
                    if (pPhysHeapNode->HeapNode.SizeTreeNode.mUserVal != barVal)
                    {
                        pPhysHeapNode = NULL;
                    }
                    else if (pPhysHeapNode->mpDevNode != NULL)
                    {
                        pPhysHeapNode = NULL;
                    }
                }
            }
            if (pPhysHeapNode == NULL)
            {
                K2OSKERN_Debug("FAILED Adding PCI MEM space reservation - %08X, %08X\n",
                    barSave & PCI_BAR_BASEMASK_MEMORY, barVal);
            }
            else
            {
                pPhysHeapNode->mpDevNode = apDevNode;
                pPhysHeapNode->mDisp = PHYS_DISP_DEVNODE;
                K2LIST_AddAtTail(&apDevNode->PhysList, &pPhysHeapNode->DevNodePhysListLink);
            }
        }
    }
}

void
sRes_DiscoveredAcpiIo(
    DEV_NODE *      apDevNode,
    ACPI_RESOURCE * Resource
)
{
    K2STAT              stat;
    UINT32              base;
    UINT32              length;
    RES_IO_HEAPNODE *   pResIoHeapNode;

    if (Resource->Type == ACPI_RESOURCE_TYPE_IO)
    {
        base = Resource->Data.Io.Minimum;
        length = Resource->Data.Io.AddressLength;
//        K2OSKERN_Debug("Adding ACPI IO space reservation - %04X, %04X\n", base, length);
    }
    else
    {
        base = Resource->Data.FixedIo.Address;
        length = Resource->Data.FixedIo.AddressLength;
//        K2OSKERN_Debug("Adding ACPI IO space reservation - %04X, %04X\n", base, length);
    }
    stat = K2HEAP_AllocNodeAt(&gRes_IoSpaceHeap, base, length, (K2HEAP_NODE **)&pResIoHeapNode);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OSKERN_Debug("***ACPI IO space reservation failed\n");
    }
    else
    {
        pResIoHeapNode->mpDevNode = apDevNode;
        K2LIST_AddAtTail(&apDevNode->IoList, &pResIoHeapNode->DevNodeIoListLink);
    }
}

void
sRes_DiscoveredAcpiDMA(
    DEV_NODE *      apDevNode,
    ACPI_RESOURCE * Resource
)
{
#if 0
    // ignore DMA defines
    if (Resource->Type == ACPI_RESOURCE_TYPE_DMA)
    {
        K2OSKERN_Debug("Discovered DMA\n");
        K2OSKERN_Debug("  Type          %02X\n", Resource->Data.Dma.Type);
        K2OSKERN_Debug("  BusMaster     %02X\n", Resource->Data.Dma.BusMaster);
        K2OSKERN_Debug("  Transfer      %02X\n", Resource->Data.Dma.Transfer);
        K2OSKERN_Debug("  ChannelsCount %d\n", Resource->Data.Dma.ChannelCount);
        K2OSKERN_Debug("  Channels[0]   %02X\n", Resource->Data.Dma.Channels[0]);
    }
    else
    {
        K2OSKERN_Debug("Discovered FIXED_DMA - %d\n", Resource->Type);
    }
#endif
}

void
sRes_DiscoveredAcpiPhys(
    DEV_NODE *      apDevNode,
    ACPI_RESOURCE * Resource
)
{
    K2STAT          stat;
    UINT32          base;
    UINT32          length;
    BOOL            readOnly;
    PHYS_HEAPNODE * pPhysHeapNode;
    K2TREE_NODE *   pTreeNode;

    switch (Resource->Type)
    {
    case ACPI_RESOURCE_TYPE_MEMORY24:
//        K2OSKERN_Debug("ACPI_RESOURCE_TYPE_MEMORY24 \n");
        base = Resource->Data.Memory24.Minimum;
        length = Resource->Data.Memory24.AddressLength;
        readOnly = Resource->Data.Memory24.WriteProtect ? TRUE : FALSE;
        break;
    case ACPI_RESOURCE_TYPE_MEMORY32:
//        K2OSKERN_Debug("Discovered ACPI_RESOURCE_TYPE_MEMORY32\n");
        base = Resource->Data.Memory32.Minimum;
        length = Resource->Data.Memory32.AddressLength;
        readOnly = Resource->Data.Memory32.WriteProtect ? TRUE : FALSE;
        break;
    case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
//        K2OSKERN_Debug("Discovered ACPI_RESOURCE_TYPE_FIXED_MEMORY32\n");
        base = Resource->Data.FixedMemory32.Address;
        length = Resource->Data.FixedMemory32.AddressLength;
        readOnly = Resource->Data.FixedMemory32.WriteProtect ? TRUE : FALSE;
        break;
    default:
        K2_ASSERT(0);
    }

//    K2OSKERN_Debug("Adding ACPI MEM space reservation - %08X, %08X, %s\n",
//        base, length, readOnly ? "READONLY" : "READ/WRITE");
    stat = K2HEAP_AllocNodeAt(&gPhys_SpaceHeap, base, length, (K2HEAP_NODE **)&pPhysHeapNode);
    if (K2STAT_IS_ERROR(stat))
    {
        pPhysHeapNode = NULL;
        pTreeNode = K2TREE_Find(&gPhys_SpaceHeap.AddrTree, base);
        if (pTreeNode != NULL)
        {
            pPhysHeapNode = K2_GET_CONTAINER(PHYS_HEAPNODE, pTreeNode, HeapNode.AddrTreeNode);
            if (pPhysHeapNode->HeapNode.SizeTreeNode.mUserVal != length)
            {
                pPhysHeapNode = NULL;
            }
            else if (pPhysHeapNode->mpDevNode != NULL)
            {
                pPhysHeapNode = NULL;
            }
        }
    }
    if (pPhysHeapNode == NULL)
    {
        K2OSKERN_Debug("FAILED Adding ACPI MEM space reservation - %08X, %08X\n",
            base, length);
    }
    else
    {
        pPhysHeapNode->mpDevNode = apDevNode;
        pPhysHeapNode->mDisp = PHYS_DISP_DEVNODE;
        pPhysHeapNode->mIsReadOnly = readOnly;
        K2LIST_AddAtTail(&apDevNode->PhysList, &pPhysHeapNode->DevNodePhysListLink);
    }
}

void
sRes_DiscoveredAcpiAddr(
    DEV_NODE *      apDevNode,
    ACPI_RESOURCE * Resource
)
{
#if 0
    // ignore address defines
    switch (Resource->Type)
    {
    case ACPI_RESOURCE_TYPE_ADDRESS16:
        K2OSKERN_Debug("Discovered ACPI_RESOURCE_TYPE_ADDRESS16\n");
        K2OSKERN_Debug("  Address.Minimum               %08X\n", Resource->Data.Address16.Address.Minimum);
        K2OSKERN_Debug("  Address.Maximum               %08X\n", Resource->Data.Address16.Address.Maximum);
        K2OSKERN_Debug("  Address.AddressLength         %08X\n", Resource->Data.Address16.Address.AddressLength);
        break;
    case ACPI_RESOURCE_TYPE_ADDRESS32:
        K2OSKERN_Debug("Discovered ACPI_RESOURCE_TYPE_ADDRESS32\n");
        K2OSKERN_Debug("  Address.Minimum               %08X\n", Resource->Data.Address32.Address.Minimum);
        K2OSKERN_Debug("  Address.Maximum               %08X\n", Resource->Data.Address32.Address.Maximum);
        K2OSKERN_Debug("  Address.AddressLength         %08X\n", Resource->Data.Address32.Address.AddressLength);
        break;
    case ACPI_RESOURCE_TYPE_ADDRESS64:
        K2OSKERN_Debug("Discovered ACPI_RESOURCE_TYPE_ADDRESS64\n");
        break;
    case ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64:
        K2OSKERN_Debug("Discovered ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64\n");
        break;
    default:
        K2_ASSERT(0);
    }
#endif
}

ACPI_STATUS
sRes_AcpiResEnumCallback(
    ACPI_RESOURCE * Resource,
    void *          Context
)
{
    switch (Resource->Type)
    {
    case ACPI_RESOURCE_TYPE_IRQ:
        // taken care of by Intr_
        break;

    case ACPI_RESOURCE_TYPE_DMA:
    case ACPI_RESOURCE_TYPE_FIXED_DMA:
        sRes_DiscoveredAcpiDMA((DEV_NODE *)Context, Resource);
        break;

    case ACPI_RESOURCE_TYPE_IO:
    case ACPI_RESOURCE_TYPE_FIXED_IO:
        sRes_DiscoveredAcpiIo((DEV_NODE *)Context, Resource);
        break;

    case ACPI_RESOURCE_TYPE_MEMORY24:
    case ACPI_RESOURCE_TYPE_MEMORY32:
    case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
        sRes_DiscoveredAcpiPhys((DEV_NODE *)Context, Resource);
        break;

    case ACPI_RESOURCE_TYPE_ADDRESS16:
    case ACPI_RESOURCE_TYPE_ADDRESS32:
    case ACPI_RESOURCE_TYPE_ADDRESS64:
    case ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64:
        sRes_DiscoveredAcpiAddr((DEV_NODE *)Context, Resource);
        break;

    case ACPI_RESOURCE_TYPE_END_TAG:
        // ignore
        break;

    default:
        K2OSKERN_Debug("sRes_AcpiResEnumCallback - %d\n", Resource->Type);
        //
        // dont know what to do with this
        //
        K2_ASSERT(0);
    }

    return AE_OK;
}

void 
sRes_EnumCurrent(
    DEV_NODE *apDevNode
)
{
    ACPI_HANDLE     hAcpi;
    DEV_NODE_PCI *  pPci;
    ACPI_STATUS     acpiStatus;

    //
    // get current resources from ACPI
    //
    hAcpi = (ACPI_HANDLE)apDevNode->DevTreeNode.mUserVal;
    if (0 != hAcpi)
    {
        apDevNode->Res.Device.CurrentAcpiRes.Pointer = NULL;
        apDevNode->Res.Device.CurrentAcpiRes.Length = ACPI_ALLOCATE_BUFFER;
        acpiStatus = AcpiGetCurrentResources(hAcpi, &apDevNode->Res.Device.CurrentAcpiRes);
        //
        // parse the resources now to get IO and MEMORY reservations
        //
        if ((!ACPI_FAILURE(acpiStatus)) &&
            (apDevNode->Res.Device.CurrentAcpiRes.Pointer != NULL))
        {
            //
            // has current resources assigned which may include IRQ(s)
            //
            AcpiWalkResourceBuffer(&apDevNode->Res.Device.CurrentAcpiRes, sRes_AcpiResEnumCallback, apDevNode);
        }
    }

    //
    // get PCI BAR resource sizes (IO, MEMORY)
    //
    pPci = apDevNode->mpPci;
    if (NULL != pPci)
    {
        sRes_QueryPciDeviceBars(apDevNode, pPci);
    }
}

void 
sRes_EnumAll(
    DEV_NODE *apDevNode
)
{
    DEV_NODE *      pChild;
    K2LIST_LINK *   pListLink;

    sRes_EnumCurrent(apDevNode);
    pListLink = apDevNode->ChildList.mpHead;
    if (pListLink == NULL)
        return;
    do {
        pChild = K2_GET_CONTAINER(DEV_NODE, pListLink, ChildListLink);
        pListLink = pListLink->mpNext;
        sRes_EnumAll(pChild);
    } while (pListLink != NULL);
}

K2HEAP_NODE * sRes_AcquireNode(K2HEAP_ANCHOR *apHeap)
{
    RES_IO_HEAPNODE * pNode;

    pNode = (RES_IO_HEAPNODE *)K2OS_HeapAlloc(sizeof(RES_IO_HEAPNODE));
    if (pNode == NULL)
        return NULL;
    K2MEM_Zero(pNode, sizeof(RES_IO_HEAPNODE));
    return &pNode->HeapNode;
}

void sRes_ReleaseNode(K2HEAP_ANCHOR *apHeap, K2HEAP_NODE *apNode)
{
    K2OS_HeapFree(apNode);
}

void Res_Init(void)
{
    BOOL    ok;
    K2STAT  stat;

    K2HEAP_Init(&gRes_IoSpaceHeap, sRes_AcquireNode, &sRes_ReleaseNode);
    ok = K2OS_CritSecInit(&gRes_IoSpaceSec);
    K2_ASSERT(ok);

    stat = K2HEAP_AddFreeSpaceNode(&gRes_IoSpaceHeap, 0, 0x10000, NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    //
    // scan dev tree and enumerate already-allocated resources
    //
    sRes_EnumAll(gpDev_RootNode);
}
