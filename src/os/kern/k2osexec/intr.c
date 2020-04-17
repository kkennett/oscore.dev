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

UINT32      gIntr_GlobalTotalLineCount;
INTR_LINE * gpIntr_Lines;
#if !K2_TARGET_ARCH_IS_ARM
BOOL        gIntr_LegacyPic;
#endif

static
void
sIntr_SetupLineTable(
    void
)
{
    UINT8 *     pScan;
    UINT32      bytesLeft;
    UINT32      virtDevAddr;
    UINT32      lineBase;
    UINT32      lineCount;
    UINT32      globalStartLineIndex;
    INTR_LINE * pIntrLine;

    globalStartLineIndex = 255;
    gIntr_GlobalTotalLineCount = 0;

    //
    // calculate the maximum # of interrupt lines coming into the
    // interrupt controller
    //
#if K2_TARGET_ARCH_IS_ARM
    K2_ASSERT(gpMADT != NULL);

    //
    // find the GICD, of which there must be one 
    //
    pScan = ((UINT8 *)gpMADT) + sizeof(ACPI_TABLE_MADT);
    bytesLeft = gpMADT->Header.Length - sizeof(ACPI_TABLE_MADT);
    K2_ASSERT(bytesLeft > 0);
    do {
        if (*pScan == ACPI_MADT_SUB_TYPE_GICD)
        {
            K2_ASSERT(virtDevAddr = 0);
            lineBase = ((ACPI_MADT_SUB_GIC_DISTRIBUTOR *)pScan)->SystemVectorBase;
            if (globalStartLineIndex > lineBase)
                globalStartLineIndex = lineBase;
            //
            // read GICD_TYPER to get ITLinesNumber field (4:0).  Number of lines = 32(ITLinesNumber+1)
            //
            lineCount = (*((UINT32 *)(K2OS_KVA_A32_GICD + A32_PERIF_GICD_OFFSET_ICDICTR))) & 0x1F;
            lineCount = 32 * (lineCount + 1);
            if (lineBase + lineCount > gIntr_GlobalTotalLineCount)
                gIntr_GlobalTotalLineCount = lineBase + lineCount;
            //
            // should only be one GICD in the system.  a GICD handles up to 1024 interrupts.  we only support one GICD
            //
            break;
        }
        K2_ASSERT(bytesLeft >= pScan[1]);
        bytesLeft -= pScan[1];
        pScan += pScan[1];
    } while (bytesLeft > 0);
#else
    if (gpMADT != NULL)
    {
        //
        // Find IOAPICs and the highest line # on them all 
        //
        pScan = ((UINT8 *)gpMADT) + sizeof(ACPI_TABLE_MADT);
        bytesLeft = gpMADT->Header.Length - sizeof(ACPI_TABLE_MADT);
        K2_ASSERT(bytesLeft > 0);
        virtDevAddr = K2OS_KVA_X32_IOAPICS;
        do {
            if (*pScan == ACPI_MADT_SUB_TYPE_IO_APIC)
            {
                //
                // found an IOAPIC. Need to get its base and line count
                //
                lineBase = ((ACPI_MADT_IO_APIC *)pScan)->GlobalIrqBase;
                if (globalStartLineIndex > lineBase)
                    globalStartLineIndex = lineBase;
                MMREG_WRITE32(virtDevAddr, X32_IOAPIC_OFFSET_IOREGSEL, X32_IOAPIC_REGIX_IOAPICVER);
                lineCount = MMREG_READ32(virtDevAddr, X32_IOAPIC_OFFSET_IOWIN);
                lineCount = ((lineCount & 0x00FF0000) >> 16) + 1;
                if (lineBase + lineCount > gIntr_GlobalTotalLineCount)
                    gIntr_GlobalTotalLineCount = lineBase + lineCount;
                virtDevAddr += K2_VA32_MEMPAGE_BYTES;
                break;
            }
            K2_ASSERT(bytesLeft >= pScan[1]);
            bytesLeft -= pScan[1];
            pScan += pScan[1];
        } while (bytesLeft > 0);
    }
    else
    {
        //
        // no MADT, so no IOAPICs, so use old PIC
        globalStartLineIndex = 0;
        gIntr_GlobalTotalLineCount = 15;
        gIntr_LegacyPic = TRUE;
    }
#endif

    gpIntr_Lines = (INTR_LINE *)K2OS_HeapAlloc(sizeof(INTR_LINE) * gIntr_GlobalTotalLineCount);
    K2_ASSERT(gpIntr_Lines != NULL);
    K2MEM_Zero(gpIntr_Lines, sizeof(INTR_LINE) * gIntr_GlobalTotalLineCount);
    pIntrLine = gpIntr_Lines;
    for (bytesLeft = globalStartLineIndex; bytesLeft < gIntr_GlobalTotalLineCount; bytesLeft++)
    {
        //
        // legacy interrupts are level, active low, shareable
        //
        pIntrLine->mLineIndex = bytesLeft;
        pIntrLine->LineConfig.mIsActiveLow = TRUE;
        pIntrLine->LineConfig.mShareConfig = TRUE;
        K2LIST_Init(&pIntrLine->DevList);
        pIntrLine++;
    }
}

static
void 
sAttachViaPci_NoAcpiInfo(
    DEV_NODE *  apDevNode
)
{
    DEV_NODE_PCI *  pPci;
    UINT32          lineIndex;
    INTR_LINE *     pIntrLine;

    pPci = apDevNode->mpPci;
    K2_ASSERT(pPci != NULL);

//    K2OSKERN_Debug("PCI(%d/%d)\n", pPci->Id.Device, pPci->Id.Function);
    
    lineIndex = pPci->PciCfg.AsTypeX.mInterruptLine;
    K2_ASSERT(0xFF != lineIndex);

    pIntrLine = &gpIntr_Lines[lineIndex];

    //
    // line configuration overrides are taken care of by the kernel
    //
//    K2OSKERN_Debug("Attaching Non-ACPI PCI DEV(%08X) to intr line %d\n", apDevNode, pIntrLine->mLineIndex);
    apDevNode->mpIntrLine = pIntrLine;
    K2LIST_AddAtTail(&pIntrLine->DevList, &apDevNode->IntrLineDevListLink);
}

static
void
sRes_DiscoveredAcpiIrq(
    DEV_NODE *          apDevNode,
    ACPI_RESOURCE_IRQ * apAcpiIrq,
    UINT32              aResLength
)
{
    UINT8       entryIndex;
    UINT32      lineIndex;
    INTR_LINE * pIntrLine;

    if (apAcpiIrq->InterruptCount == 0)
        return;

    entryIndex = aResLength - (sizeof(ACPI_RESOURCE_IRQ) - 1);

    K2_ASSERT(apAcpiIrq->InterruptCount <= entryIndex);

//    K2OSKERN_Debug("  Trigger %s, Polarity %s, Shareable %s, WakeCapable %s\n",
//        apAcpiIrq->Triggering ? "EDGE" : "LEVEL",
//        apAcpiIrq->Polarity ? "LOW" : "HIGH",
//        apAcpiIrq->Shareable ? "YES" : "NO",
//        apAcpiIrq->WakeCapable ? "YES" : "NO");

    for (entryIndex = 0; entryIndex < apAcpiIrq->InterruptCount; entryIndex++)
    {
        lineIndex = apAcpiIrq->Interrupts[entryIndex];
//        K2OSKERN_Debug("  Configured for line %d\n", lineIndex);

        if (lineIndex < gIntr_GlobalTotalLineCount)
        {
            pIntrLine = &gpIntr_Lines[lineIndex];

            if (pIntrLine->DevList.mNodeCount > 0)
            {
                if ((!pIntrLine->LineConfig.mShareConfig) ||
                    (!apAcpiIrq->Shareable))
                {
                    K2OSKERN_Debug("*** IRQ Conflict - DEV(%08X) collided on non-shareable IRQ.\n", apDevNode);
                    continue;
                }

                if (pIntrLine->LineConfig.mIsEdgeTriggered)
                {
                    if (apAcpiIrq->Triggering != 0)
                    {
                        K2OSKERN_Debug("*** IRQ Conflict - DEV(%08X) needs edge triggered, but some other device already configured line as level.\n", apDevNode);
                        continue;
                    }
                }
                else
                {
                    if (apAcpiIrq->Triggering == 0)
                    {
                        K2OSKERN_Debug("*** IRQ Conflict - DEV(%08X) needs level triggered, but some other device already configured line as edge.\n", apDevNode);
                        continue;
                    }
                }

                if (pIntrLine->LineConfig.mIsActiveLow)
                {
                    if (apAcpiIrq->Polarity == 0)
                    {
                        K2OSKERN_Debug("*** IRQ Conflict - DEV(%08X) needs active high, but some other device already configured line as active low.\n", apDevNode);
                        continue;
                    }
                }
                else
                {
                    if (apAcpiIrq->Polarity != 0)
                    {
                        K2OSKERN_Debug("*** IRQ Conflict - DEV(%08X) needs active low, but some other device already configured line as active high.\n", apDevNode);
                        continue;
                    }
                }

                if (apAcpiIrq->WakeCapable)
                    pIntrLine->LineConfig.mWakeConfig = TRUE;


            }
            else
            {
                pIntrLine->LineConfig.mIsEdgeTriggered = apAcpiIrq->Triggering ? TRUE : FALSE;
                pIntrLine->LineConfig.mIsActiveLow = apAcpiIrq->Polarity ? TRUE : FALSE;
                pIntrLine->LineConfig.mShareConfig = apAcpiIrq->Shareable ? TRUE : FALSE;
                pIntrLine->LineConfig.mWakeConfig = apAcpiIrq->WakeCapable ? TRUE : FALSE;
            }

            //
            // attach device to this interrupt line
            //
//            K2OSKERN_Debug("Attaching Non-PCI ACPI DEV(%08X) to intr line %d\n", apDevNode, pIntrLine->mLineIndex);
            apDevNode->mpIntrLine = pIntrLine;
            K2LIST_AddAtTail(&pIntrLine->DevList, &apDevNode->IntrLineDevListLink);

        }
        else
        {
            K2OSKERN_Debug("*** device tries to use global interrupt %d, which does not exist\n", apAcpiIrq->Interrupts[entryIndex]);
        }
    }
}

static
ACPI_STATUS
sIntr_AcpiResEnumCallback(
    ACPI_RESOURCE * Resource,
    void *          Context
)
{
    if (Resource->Type == ACPI_RESOURCE_TYPE_IRQ)
        sRes_DiscoveredAcpiIrq((DEV_NODE *)Context, (ACPI_RESOURCE_IRQ *)&Resource->Data, Resource->Length);
    return AE_OK;
}

static
void
sIntr_AttachPciAcpi(
    DEV_NODE *      apBusDevNode,
    DEV_NODE *      apDevNode,
    DEV_NODE_PCI *  apPci
)
{
    UINT32 lineIndex;

    //
    // enumerated PCI device defined in ACPI tables
    //
    if (apDevNode->Res.Device.CurrentAcpiRes.Pointer != NULL)
    {
        //
        // has current resources assigned which may include IRQ(s)
        //
        AcpiWalkResourceBuffer(&apDevNode->Res.Device.CurrentAcpiRes, sIntr_AcpiResEnumCallback, apDevNode);
        if (apDevNode->mpIntrLine != NULL)
        {
            //
            // some interrupt line was discovered in the _CRS settings
            //
            return;
        }
    }

    //
    // has no _CRS resources assigned, or _CRS did not mention an IRQ entry
    //
    lineIndex = apPci->PciCfg.AsTypeX.mInterruptLine;
    if (lineIndex == 0xFF)
        return;

    //
    // has an interrupt line configured in its PCI configuration area
    //
    sAttachViaPci_NoAcpiInfo(apDevNode);
}

static
void
sIntr_CheckAttachViaAcpiInfo(
    DEV_NODE *  apDevNode
)
{
    DEV_NODE_PCI *  pPci;
    DEV_NODE *      pBusDevNode;

    K2_ASSERT(apDevNode->mpAcpiInfo != NULL);

    pPci = apDevNode->mpPci;
    if (pPci != NULL)
    {
        //
        // PCI device - should appear in the bus routing table
        //
        pBusDevNode = apDevNode->mpParent;
        K2_ASSERT(pBusDevNode != NULL);
        do {
            if (pBusDevNode->mResFlags & DEV_NODE_RESFLAGS_IS_BUS)
                break;
            if (pBusDevNode == gpDev_RootNode)
                break;
            pBusDevNode = pBusDevNode->mpParent;
        } while (1);

        sIntr_AttachPciAcpi(pBusDevNode, apDevNode, pPci);
    }
    else
    {
        //
        // non-PCI ACPI device
        //
        if (apDevNode->Res.Device.CurrentAcpiRes.Pointer != NULL)
        {
            do {
                if (apDevNode->mpAcpiInfo->Valid & ACPI_VALID_HID)
                {
                    //
                    // PCI LNK devices are ignored for interrupt routing
                    // as UEFI should have already done all the routing
                    //
                    if (0 == K2ASC_Comp("PNP0C0F", apDevNode->mpAcpiInfo->HardwareId.String))
                        break;
                }
                AcpiWalkResourceBuffer(&apDevNode->Res.Device.CurrentAcpiRes, sIntr_AcpiResEnumCallback, apDevNode);
            } while (0);
        }
    }
}

void Intr_Init(void)
{
    K2TREE_NODE *   pTreeNode;
    DEV_NODE *      pDevNode;

    sIntr_SetupLineTable();

    pTreeNode = K2TREE_FirstNode(&gDev_Tree);
    K2_ASSERT(pTreeNode != NULL);
    do {
        pDevNode = K2_GET_CONTAINER(DEV_NODE, pTreeNode, DevTreeNode);
        pTreeNode = K2TREE_NextNode(&gDev_Tree, pTreeNode);
        do {
            K2_ASSERT(pDevNode->mpIntrLine == NULL);

            if (pDevNode == gpDev_RootNode)
                break;

            if (pDevNode->mResFlags & DEV_NODE_RESFLAGS_IS_BUS)
                break;

            if (NULL != pDevNode->mpAcpiInfo)
            {
                sIntr_CheckAttachViaAcpiInfo(pDevNode);
            }
            else if (NULL != pDevNode->mpPci)
            {
                if (0xFF != pDevNode->mpPci->PciCfg.AsTypeX.mInterruptLine)
                    sAttachViaPci_NoAcpiInfo(pDevNode);
            }
        } while (0);

    } while (pTreeNode != NULL);
}
