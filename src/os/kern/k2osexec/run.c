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
                sScanForHw(K2_GET_CONTAINER(DEV_NODE, pListLink, ChildListLink), aLevel+1);
                pListLink = pListLink->mpNext;
            } while (pListLink != NULL);
        }
    }
}

void
K2OSEXEC_Run(
    void
)
{
    Msg_Init();

    K2OSKERN_Debug("\nUnparented Hardware:\n------------------------------------\n");
    sScanForHw(gpDev_RootNode, 0);
    K2OSKERN_Debug("------------------------------------\n\n");

#if 1
    K2OSKERN_Debug("K2OSEXEC: Main Thread Sleep Hang ints on\n");
    while (1)
    {
        K2OSKERN_Debug("Main Tick %d\n", (UINT32)K2OS_SysUpTimeMs());
        K2OS_ThreadSleep(1000);
    }
#else
    UINT64          last, newTick;
    K2OSKERN_Debug("K2OSEXEC: Main Thread Stall Hang ints on\n");
    last = K2OS_SysUpTimeMs();
    while (1)
    {
        do {
            newTick = K2OS_SysUpTimeMs();
        } while (newTick - last < 1000);
        last = newTick;
        K2OSKERN_Debug("Tick %d\n", (UINT32)(newTick & 0xFFFFFFFF));
    }
#endif

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
