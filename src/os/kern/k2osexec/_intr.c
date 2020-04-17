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

#define MAX_INTR_PER_DEVICE     8

typedef struct _WALK_CTX WALK_CTX;
struct _WALK_CTX
{
    DEV_NODE *                  mpDevNode;
    DEV_NODE_PCI *              mpPci;
    UINT32                      mNumLines;
    UINT8                       mPin[MAX_INTR_PER_DEVICE];
    UINT8                       mLine[MAX_INTR_PER_DEVICE];
    K2OSKERN_IRQ_LINE_CONFIG    LineConfig[MAX_INTR_PER_DEVICE];
};

ACPI_STATUS
sWalkFindIrqs(
    ACPI_RESOURCE * Resource,
    void *          Context
)
{
    WALK_CTX *          pCtx;
    UINT8               ix;
    ACPI_RESOURCE_IRQ * pIrqRes;

    if (Resource->Type != ACPI_RESOURCE_TYPE_IRQ)
        return AE_OK;

    K2OSKERN_Debug("  ACPI_IRQ\n");

    pCtx = (WALK_CTX *)Context;

    if (Resource->Length < sizeof(ACPI_RESOURCE_IRQ))
    {
        return AE_OK;
    }

    pIrqRes = (ACPI_RESOURCE_IRQ *)&Resource->Data;

    if ((pIrqRes->InterruptCount == 0) ||
        (pIrqRes->Interrupts[0] == 0xFF))
    {
        return AE_OK;
    }

    ix = pCtx->mNumLines;
    K2_ASSERT(ix < (MAX_INTR_PER_DEVICE-1));

    K2OSKERN_Debug("    LINE %d\n", pIrqRes->Interrupts[0]);
    K2OSKERN_Debug("    TRIGGER-%d, POLARITY-%d, SHAREABLE-%d, WAKECAPABLE-%d\n",
        pIrqRes->Triggering,
        pIrqRes->Polarity,
        pIrqRes->Shareable,
        pIrqRes->WakeCapable);

    pCtx->mPin[ix] = 0xFF;
    pCtx->mLine[ix] = pIrqRes->Interrupts[0];
    pCtx->LineConfig[ix].mIsEdgeTriggered = pIrqRes->Triggering;
    pCtx->LineConfig[ix].mIsActiveLow = pIrqRes->Polarity;
    pCtx->LineConfig[ix].mSharing = pIrqRes->Shareable;
    pCtx->LineConfig[ix].mWaking = pIrqRes->WakeCapable;
    pCtx->mNumLines++;

    return AE_OK;
}

void Intr_Init(void)
{
    K2TREE_NODE *   pTreeNode;
    WALK_CTX        walkCtx;
    UINT8           val8;
    UINT32          ix;

    //
    // discover 'fixed' interrupts from PCI devices unknown to ACPI
    //
    K2OSKERN_Debug("INTERRUPTS:\n");

    pTreeNode = K2TREE_FirstNode(&gDev_Tree);
    K2_ASSERT(pTreeNode != NULL);
    do {
        K2MEM_Zero(&walkCtx, sizeof(walkCtx));
        walkCtx.mpDevNode = K2_GET_CONTAINER(DEV_NODE, pTreeNode, DevTreeNode);
        K2OSKERN_Debug("DEV_NODE(%08X)\n", walkCtx.mpDevNode);

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
        pTreeNode = K2TREE_NextNode(&gDev_Tree, pTreeNode);
    } while (pTreeNode != NULL);


    //
    // calculate devices needing interrupt assignment
    //


}
