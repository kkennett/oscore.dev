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

#define DUMP_DEVICES 0

K2TREE_ANCHOR       gDev_Tree;
K2OSKERN_SEQLOCK    gDev_SeqLock;
DEV_NODE *          gpDev_RootNode;

#define MAX_LEVELS 32

typedef struct _WALKCTX WALKCTX;
struct _WALKCTX
{
    UINT32      mLevel;
    DEV_NODE *  Working[MAX_LEVELS];
};

static
ACPI_STATUS
sInitialDeviceWalkCallback(
    ACPI_HANDLE Object,
    UINT32      NestingLevel,
    void *      Context,
    void **     ReturnValue)
{
    ACPI_STATUS         acpiStatus;
    ACPI_DEVICE_INFO *  pDevInfo;
    DEV_NODE *          pDevNode;
    WALKCTX *           pWalkCtx;

    K2_ASSERT(NestingLevel != 0);

    pWalkCtx = (WALKCTX *)Context;

    acpiStatus = AcpiGetObjectInfo(Object, &pDevInfo);
    if (!ACPI_FAILURE(acpiStatus))
    {
        if (pDevInfo->Type == ACPI_TYPE_DEVICE)
        {
            pDevNode = (DEV_NODE *)K2OS_HeapAlloc(sizeof(DEV_NODE));
            if (pDevNode != NULL)
            {
                if (NestingLevel > pWalkCtx->mLevel)
                {
                    K2_ASSERT(NestingLevel == pWalkCtx->mLevel + 1);
                }
                pWalkCtx->mLevel = NestingLevel;
                pWalkCtx->Working[pWalkCtx->mLevel] = pDevNode;

                K2MEM_Zero(pDevNode, sizeof(DEV_NODE));
                K2LIST_Init(&pDevNode->ChildList);
                pDevNode->mpParent = pWalkCtx->Working[pWalkCtx->mLevel - 1];
                pDevNode->mpAcpiInfo = pDevInfo;
                pDevNode->DevTreeNode.mUserVal = (UINT32)Object;
                K2TREE_Insert(&gDev_Tree, pDevNode->DevTreeNode.mUserVal, &pDevNode->DevTreeNode);
                if (pDevNode->mpParent != NULL)
                    K2LIST_AddAtTail(&pDevNode->mpParent->ChildList, &pDevNode->ChildListLink);
            }
        }
        else
        {
            K2OS_HeapFree(pDevInfo);
        }
    }

    return AE_OK;
}

#if DUMP_DEVICES

static void sPrefix(UINT32 aCount)
{
    if (0 == aCount)
        return;
    do {
        K2OSKERN_Debug("  ");
    } while (--aCount);
}

static void sDumpDevice(DEV_NODE *apNode, UINT32 aLevel)
{
    K2LIST_LINK *               pListLink;
    UINT32                      ix;
    ACPI_DEVICE_INFO *          pDevInfo;
    DEV_NODE_PCI *              pPci;
    K2OSACPI_DEVICE_ID const *  pDevId;

    sPrefix(aLevel);
    K2OSKERN_Debug("NODE(%08X)\n", apNode);

    sPrefix(aLevel + 1);
    pDevInfo = apNode->mpAcpiInfo;
    if (NULL != pDevInfo)
    {
        K2OSKERN_Debug("ACPI(hNode=%08X)", apNode->DevTreeNode.mUserVal);
        if (pDevInfo->Valid & ACPI_VALID_HID)
            K2OSKERN_Debug(" _HID(%s)", pDevInfo->HardwareId.String);
        else
            K2OSKERN_Debug(" NOHID");
        if (pDevInfo->Valid & ACPI_VALID_UID)
            K2OSKERN_Debug(" _UID(%s)", pDevInfo->UniqueId.String);
        else
            K2OSKERN_Debug(" NOUID");
        if (pDevInfo->Valid & ACPI_VALID_CID)
        {
            K2OSKERN_Debug(" _CID(%d: %s", pDevInfo->CompatibleIdList.Count, pDevInfo->CompatibleIdList.Ids[0].String);
            for (ix = 1; ix < pDevInfo->CompatibleIdList.Count; ix++)
            {
                K2OSKERN_Debug(", %s", pDevInfo->CompatibleIdList.Ids[ix].String);
            }
            K2OSKERN_Debug(")\n");
        }
        else
            K2OSKERN_Debug(" NOCID\n");
        if (pDevInfo->Valid & ACPI_VALID_HID)
        {
            pDevId = &AslDeviceIds[0];
            while (pDevId->Name != NULL)
            {
                if (0 == K2ASC_CompIns(pDevId->Name, pDevInfo->HardwareId.String))
                {
                    sPrefix(aLevel + 1);
                    K2OSKERN_Debug("\"%s\"\n", pDevId->Description);
                    break;
                }
                pDevId++;
            }
        }
    }
    else
    {
        K2OSKERN_Debug("ACPI(NONE)\n");
    }

    sPrefix(aLevel + 1);
    pPci = apNode->mpPci;
    if (NULL != pPci)
    {
        K2OSKERN_Debug("PCI(%d/%d/%d/%d) VID %04X PID %04X, Pin %d, Line %d\n",
            pPci->Id.Segment,
            pPci->Id.Bus,
            pPci->Id.Device,
            pPci->Id.Function,
            pPci->PciCfg.AsTypeX.mVendorId,
            pPci->PciCfg.AsTypeX.mDeviceId,
            pPci->PciCfg.AsTypeX.mInterruptPin,
            pPci->PciCfg.AsTypeX.mInterruptLine
            );
    }
    else
    {
        K2OSKERN_Debug("PCI(NONE)\n");
    }

    sPrefix(aLevel + 1);
    K2OSKERN_Debug("%d Children\n", apNode->ChildList.mNodeCount);
    pListLink = apNode->ChildList.mpHead;
    if (pListLink != NULL)
    {
        do {
            sDumpDevice(K2_GET_CONTAINER(DEV_NODE, pListLink, ChildListLink), aLevel + 2);
            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);
    }
}

#endif

void Dev_Init(void)
{
    K2TREE_NODE *   pTreeNode;
    DEV_NODE *      pDevNode;
    void *          pWalkRet;
    WALKCTX         walkCtx;

    K2TREE_Init(&gDev_Tree, NULL);
    K2OSKERN_SeqIntrInit(&gDev_SeqLock);
    gpDev_RootNode = NULL;

    //
    // initial device discovery
    //
    K2MEM_Zero(&walkCtx, sizeof(walkCtx));

    gpDev_RootNode = (DEV_NODE *)K2OS_HeapAlloc(sizeof(DEV_NODE));
    K2_ASSERT(gpDev_RootNode != NULL);
    K2MEM_Zero(gpDev_RootNode, sizeof(DEV_NODE));
    K2LIST_Init(&gpDev_RootNode->ChildList);
    K2TREE_Insert(&gDev_Tree, gpDev_RootNode->DevTreeNode.mUserVal, &gpDev_RootNode->DevTreeNode);

    walkCtx.Working[0] = gpDev_RootNode;

    pWalkRet = NULL;
    AcpiGetDevices(
        NULL,
        sInitialDeviceWalkCallback,
        &walkCtx,
        &pWalkRet
    );

    //
    // iteratively discover PCI bridges
    //
    pTreeNode = K2TREE_FirstNode(&gDev_Tree);
    if (pTreeNode != NULL)
    {
        do {
            pDevNode = K2_GET_CONTAINER(DEV_NODE, pTreeNode, DevTreeNode);
            if (pDevNode->mpAcpiInfo != NULL)
            {
                if (pDevNode->mpAcpiInfo->Valid & ACPI_VALID_HID)
                {
                    if ((0 == K2ASC_CompIns(pDevNode->mpAcpiInfo->HardwareId.String, PCI_ROOT_HID_STRING)) ||
                        (0 == K2ASC_CompIns(pDevNode->mpAcpiInfo->HardwareId.String, PCI_EXPRESS_ROOT_HID_STRING)))
                    {
                        Pci_DiscoverBridgeFromAcpi(pDevNode);
                    }
                }
            }
            pTreeNode = K2TREE_NextNode(&gDev_Tree, pTreeNode);
        } while (pTreeNode != NULL);
    }

    Pci_CheckManualScan();

#if DUMP_DEVICES
    //
    // dump the device tree
    //
    sDumpDevice(gpDev_RootNode,0);
#endif
}

