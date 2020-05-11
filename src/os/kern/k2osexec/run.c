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

static K2OS_TOKEN                   sgTokWorkEvent;
static SERWORK_ITEM_HDR * volatile  sgpWorkList;

void 
Run_AddSerializedWork(
    SERWORK_ITEM_HDR *  apItem, 
    SERWORKITEM_pf_Exec afExec
)
{
    UINT32 old;

    apItem->mfExec = afExec;
    do {
        old = (UINT32)sgpWorkList;
        apItem->mpNext = (SERWORK_ITEM_HDR *)old;
        if (old == K2ATOMIC_CompareExchange((UINT32 volatile *)&sgpWorkList, (UINT32)apItem, old))
            break;
    } while (1);
    K2OS_EventSet(sgTokWorkEvent);
}

UINT32 
K2_CALLCONV_REGS 
sHalThread(
    void *apParam
)
{
    K2OSHAL_pf_OnSystemReady fReady;
    fReady = (K2OSHAL_pf_OnSystemReady)apParam;
    return fReady();
}

void
sStartHalThread(
    K2OSHAL_pf_OnSystemReady afReady
)
{
    K2OS_TOKEN          tokThread;
    K2OS_THREADCREATE   cret;

    K2MEM_Zero(&cret, sizeof(cret));
    cret.mStructBytes = sizeof(cret);
    cret.mEntrypoint = sHalThread;
    cret.mpArg = afReady;

    tokThread = K2OS_ThreadCreate(&cret);
    K2_ASSERT(NULL != tokThread);

    K2OS_TokenDestroy(tokThread);
}

void
K2OSEXEC_Run(
    K2OSHAL_pf_OnSystemReady afReady
)
{
    SERWORK_ITEM_HDR *  pFix;
    SERWORK_ITEM_HDR *  pHold;
    SERWORK_ITEM_HDR *  pWorkList;
    UINT32              waitResult;

    //
    // set up worker thread queue
    //
    sgpWorkList = NULL;
    sgTokWorkEvent = K2OS_EventCreate(NULL, TRUE, FALSE);
    K2_ASSERT(NULL != sgTokWorkEvent);

    //
    // this has to happen after fsprov init and drvstore init
    //
    Msg_Init();

    //
    // run the HAL, which will provide a driver store interface
    // which we can use to find drivers for devices that need them
    //
    if (NULL != afReady)
    {
        //
        // afReady will have been provided by the init info
        //
        sStartHalThread(afReady);
//        K2OSKERN_Debug("HAL thread started\n");
    }

    //
    // this is now the worker thread
    // which shoulld actually be thread 0 in the system as well
    //
    do {
        K2OSKERN_Debug("WorkWait\n");
        waitResult = K2OS_ThreadWait(1, &sgTokWorkEvent, FALSE, K2OS_TIMEOUT_INFINITE);
        K2_ASSERT(waitResult == K2OS_WAIT_SIGNALLED_0);

        do {
            pWorkList = (SERWORK_ITEM_HDR *)K2ATOMIC_Exchange((UINT32 volatile *)&sgpWorkList, 0);
            if (pWorkList == NULL)
                break;

            //
            // reverse the list
            //
            pFix = NULL;
            do {
                pHold = pWorkList;
                pWorkList = pWorkList->mpNext;
                pHold->mpNext = pFix;
                pFix = pHold;
            } while (pWorkList != NULL);
            pWorkList = pHold;

            //
            // process the list
            //
            do {
                pHold = pWorkList;
                pWorkList = pWorkList->mpNext;
                K2OSKERN_Debug("Work:%08X\n", pHold);
                pHold->mfExec(pHold);
            } while (pWorkList != NULL);

        } while (1);

    } while (1);
}

#if 0

8086 "Intel"
27B9 "82801GBM (ICH7-M) LPC Interface Bridge"
2415 "82801AA AC'97 Audio Controller"
7113 "82371AB/EB/MB PIIX4 ACPI"
7111 "82371AB/EB/MB PIIX4 IDE"
265C "82801FB/FBM/FR/FW/FRW (ICH6 Family) USB2 EHCI Controller"

80EE    "VirtualBox"
BEEF "VirtualBox Graphics Adapter"
CAFE "VirtualBox Guest Service"

1022     "AMD"
2000 "79c970 [PCnet32 LANCE]"

106B "Apple"
003F "KeyLargo / Intrepid USB"

#endif

#if 0

void
sScanForHw(
    DEV_NODE *  apDevNode,
    UINT32      aLevel
)
{
    K2LIST_LINK *       pListLink;
    ACPI_DEVICE_INFO *  pAcpi;
    DEV_NODE_PCI *      pPci;
    char                nameBuffer[8];
    RES_IO_HEAPNODE *   pResIo;
    PHYS_HEAPNODE *     pPhys;

    if ((apDevNode->mpIntrLine) ||
        (apDevNode->IoList.mNodeCount) ||
        (apDevNode->PhysList.mNodeCount))
    {
        pAcpi = apDevNode->mpAcpiInfo;
        if (NULL != pAcpi)
        {
            K2MEM_Copy(nameBuffer, &pAcpi->Name, sizeof(UINT32));
            nameBuffer[4] = 0;
            K2OSKERN_Debug("HW %d ACPI Name \"%s\" VALID %04X HID(%s)\n",
                aLevel,
                nameBuffer,
                pAcpi->Valid,
                (pAcpi->Valid & ACPI_VALID_HID) ? pAcpi->HardwareId.String : "<NONE>");
            if (apDevNode->mpIntrLine != NULL)
            {
                K2OSKERN_Debug("  INTR_LINE %d\n", apDevNode->mpIntrLine->mLineIndex);
            }
            pListLink = apDevNode->IoList.mpHead;
            if (pListLink != NULL)
            {
                do {
                    pResIo = K2_GET_CONTAINER(RES_IO_HEAPNODE, pListLink, DevNodeIoListLink);
                    K2OSKERN_Debug("  IO   %04X     %04X\n",
                        pResIo->HeapNode.AddrTreeNode.mUserVal,
                        pResIo->HeapNode.SizeTreeNode.mUserVal);
                    pListLink = pListLink->mpNext;
                } while (pListLink != NULL);
            }
            if (apDevNode->PhysList.mNodeCount)
            {
                do {
                    pPhys = K2_GET_CONTAINER(PHYS_HEAPNODE, pListLink, DevNodePhysListLink);
                    K2OSKERN_Debug("  PHYS %08X %08X\n",
                        pPhys->HeapNode.AddrTreeNode.mUserVal,
                        pPhys->HeapNode.SizeTreeNode.mUserVal);
                    pListLink = pListLink->mpNext;
                } while (pListLink != NULL);
            }
        }
        else
        {
            pPci = apDevNode->mpPci;
            if (NULL != pPci)
            {
                K2OSKERN_Debug("HW %d PCI  Vendor %04X Device %04X\n",
                    aLevel,
                    pPci->PciCfg.AsTypeX.mVendorId,
                    pPci->PciCfg.AsTypeX.mDeviceId);
            }
        }
    }
    else
    {
        pListLink = apDevNode->ChildList.mpHead;
        if (pListLink != NULL)
        {
            do {
                sScanForHw(K2_GET_CONTAINER(DEV_NODE, pListLink, ChildListLink), aLevel + 1);
                pListLink = pListLink->mpNext;
            } while (pListLink != NULL);
        }
    }
}

#endif
