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

typedef struct _DEVICE_ROUTING_ENTRY DEVICE_ROUTING_ENTRY;
struct _DEVICE_ROUTING_ENTRY
{
    ACPI_PCI_ROUTING_TABLE *    mpPrtEntry;
    K2LIST_LINK                 ListLink;
};

void Intr_Init(void)
{
    K2TREE_NODE *               pTreeNode;
    DEV_NODE *                  pDevNode;
    DEV_NODE *                  pBusDevNode;
    DEV_NODE_PCI *              pPci;
    K2LIST_ANCHOR               routingEntryList;
    UINT8 *                     pEntRaw;
    UINT32                      left;
    UINT16                      deviceNum;
    ACPI_PCI_ROUTING_TABLE *    pPrtEntry;

    //
    // for each device, find its bus and then find its entries in the bus' PRT
    // if the bus has no PRT
    //

    pTreeNode = K2TREE_FirstNode(&gDev_Tree);
    K2_ASSERT(pTreeNode != NULL);
    do {
        pDevNode = K2_GET_CONTAINER(DEV_NODE, pTreeNode, DevTreeNode);
        pTreeNode = K2TREE_NextNode(&gDev_Tree, pTreeNode);
        K2OSKERN_Debug("DEV_NODE(%08X)\n", pDevNode);

        do {
            if (pDevNode == gpDev_RootNode)
            {
                pDevNode->mInputIntrIndexToIntrController = 0xFF;
                break;
            }

            if (pDevNode->mResFlags & DEV_NODE_RESFLAGS_IS_BUS)
            {
                pDevNode->mInputIntrIndexToIntrController = 0xFF;
                break;
            }

            pPci = pDevNode->mpPci;
            if (pPci != NULL)
            {
                if (pPci->PciCfg.AsTypeX.mInterruptLine == 0xFF)
                {
                    pDevNode->mInputIntrIndexToIntrController = 0xFF;
                    break;
                }
                deviceAddr = (((UINT32)pPci->Id.Device) << 16) | 0xFFFF;
            }
            else
            {
                if (NULL == pDevNode->mpAcpiInfo)
                    break;


            }

            pBusDevNode = pDevNode->mpParent;
            K2_ASSERT(pBusDevNode != NULL);
            do {
                if (pBusDevNode->mResFlags & DEV_NODE_RESFLAGS_IS_BUS)
                    break;
                if (pBusDevNode == gpDev_RootNode)
                    break;
                pBusDevNode = pBusDevNode->mpParent;
            } while (1);

            pEntRaw = (UINT8 *)pBusDevNode->Res.Bus.IntRoutingTable.Pointer;
            if (pEntRaw != NULL)
            {
                //
                // routing table entries that match device number in routing table
                // should point to link objects that have a current resource selection
                // specifying the interrupt line 
                //
                left = pBusDevNode->Res.Bus.IntRoutingTable.Length;
                K2_ASSERT(left >= sizeof(ACPI_PCI_ROUTING_TABLE));
                K2LIST_Init(&routingEntryList);
                do {
                    pPrtEntry = (ACPI_PCI_ROUTING_TABLE *)pEntRaw;
                    pEntRaw += pPrtEntry->Length;

                    //
                    // follow this entry
                    //
                    if ((UINT32)((pPrtEntry->Address >> 16) & 0xFFFF)) == pDevNode->

                    if ((pPrtEntry->Length == 0) ||
                        (left <= pPrtEntry->Length))
                        break;
                    left -= pPrtEntry->Length;
                } while (pPrtEntry->Length > 0);


            }
            else
            {
                //
                // PCI bus has no routing table.
                //
                K2_ASSERT(pPci != NULL);
                pDevNode->Res.Device.mInputIntrIndexToIntrController = pPci->PciCfg.AsTypeX.mInterruptLine;
            }

        } while (0);


        //
        // parse for ACPI IRQ info. 
        // ORDER IS IMPORTANT IN THIS as PCI routing table may refer to interrupts in this node by index
        //
        if ((NULL != walkCtx.mpDevNode->mpAcpiInfo) &&
            (0 == (walkCtx.mpDevNode->mResFlags & DEV_NODE_RESFLAGS_IS_BUS)) &&
            (walkCtx.mpDevNode->Res.Device.CurrentAcpiRes.Pointer != NULL))
        {
            //
            // parse for IRQ info
            //
            (void)AcpiWalkResourceBuffer(&walkCtx.mpDevNode->Res.Device.CurrentAcpiRes, sWalkFindIrqs, &walkCtx);
        }

        walkCtx.mpPci = walkCtx.mpDevNode->mpPci;
        if (walkCtx.mpPci != NULL)
        {
            //
            // PCI device
            //
            K2OSKERN_Debug("  PCI %d/%d\n", walkCtx.mpPci->Id.Device, walkCtx.mpPci->Id.Function);
            val8 = walkCtx.mpPci->PciCfg.AsTypeX.mInterruptLine;
            if (val8 != 0xFF)
            {
                for (ix = 0; ix < walkCtx.mNumLines; ix++)
                {
                    if (walkCtx.mLine[ix] == val8)
                        break;
                }
                if (ix == walkCtx.mNumLines)
                {
                    K2_ASSERT(ix < (MAX_INTR_PER_DEVICE - 1));
                    walkCtx.mLine[ix] = val8;
                    walkCtx.mPin[ix] = walkCtx.mpPci->PciCfg.AsTypeX.mInterruptPin;
                    walkCtx.LineConfig[ix].mIsEdgeTriggered = FALSE;
                    walkCtx.LineConfig[ix].mIsActiveLow = FALSE;
                    walkCtx.LineConfig[ix].mSharing = FALSE;
                    walkCtx.LineConfig[ix].mWaking = FALSE;
                    walkCtx.mNumLines = 1;
                    K2OSKERN_Debug("    LINE %d, PIN %c\n", walkCtx.mLine[ix], 'A' + walkCtx.mPin[ix]);
                }
            }
        }
        //
        // now that pci interrupts are known for this device, we can 
        // scan for ACPI IRQ definitions, of which there can be more than one
        //
    } while (pTreeNode != NULL);


    //
    // calculate devices needing interrupt assignment
    //


}
