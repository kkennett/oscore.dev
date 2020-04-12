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

static 
PCI_DEVICE *
sGetPciDevFromId(
    ACPI_PCI_ID *   PciId
)
{
    PCI_SEGMENT *   pPciSeg;
    K2LIST_LINK *   pListLink;
    UINT32          devIndexInSegment;
    K2TREE_NODE *   pTreeNode;
    PCI_DEVICE *    pPciDev;
    K2STAT          stat;
    UINT32          physAddr;
    UINT32          virtAddr;

    pListLink = gPciSegList.mpHead;
    if (pListLink == NULL)
        return NULL;

    do {
        pPciSeg = K2_GET_CONTAINER(PCI_SEGMENT, pListLink, PciSegListLink);
        if ((PciId->Bus >= pPciSeg->mpMcfgAlloc->StartBusNumber) &&
            (PciId->Bus <= pPciSeg->mpMcfgAlloc->EndBusNumber))
            break;
        pListLink = pListLink->mpNext;
    } while (pListLink != NULL);

    if (pListLink == NULL)
        return NULL;

    devIndexInSegment =
        ((PciId->Bus - pPciSeg->mpMcfgAlloc->StartBusNumber) * 256) +
        (PciId->Device * 8) +
        PciId->Function;

    K2_ASSERT(devIndexInSegment < ((pPciSeg->mpMcfgAlloc->EndBusNumber - pPciSeg->mpMcfgAlloc->StartBusNumber) * 32 * 8));

    pTreeNode = K2TREE_Find(&pPciSeg->PciDevTree, devIndexInSegment);

    if (pTreeNode != NULL)
        return K2_GET_CONTAINER(PCI_DEVICE, pTreeNode, PciDevTreeNode);

    physAddr = (devIndexInSegment * K2_VA32_MEMPAGE_BYTES) + (UINT32)pPciSeg->mpMcfgAlloc->Address;

    stat = K2OSKERN_MapDevice(physAddr, 1, &virtAddr);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    pPciDev = (PCI_DEVICE *)K2OS_HeapAlloc(sizeof(PCI_DEVICE));
    if (pPciDev == NULL)
    {
        stat = K2OSKERN_UnmapDevice(virtAddr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
        K2OS_ThreadSetStatus(K2STAT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    K2MEM_Zero(pPciDev, sizeof(PCI_DEVICE));

    pPciDev->PciId = *PciId;
    pPciDev->CfgPageMapping = virtAddr;
    pPciDev->mpPciSeg = pPciSeg;
    pPciDev->PciDevTreeNode.mUserVal = devIndexInSegment;
    K2TREE_Insert(&pPciSeg->PciDevTree, devIndexInSegment, &pPciDev->PciDevTreeNode);

    return pPciDev;
}

static
ACPI_STATUS
sReadPciConfiguration(
    ACPI_PCI_ID *   PciId,
    UINT32          Reg,
    UINT64 *        Value,
    UINT32          Width
)
{
    PCI_DEVICE *    pPciDev;
    UINT32          val32;

    pPciDev = sGetPciDevFromId(PciId);

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
sWritePciConfiguration(
    ACPI_PCI_ID *   PciId,
    UINT32          Reg,
    UINT64          Value,
    UINT32          Width)
{

    PCI_DEVICE *    pPciDev;
    UINT32          val32;
    UINT32          mask32;
    UINT32          reg;

    pPciDev = sGetPciDevFromId(PciId);

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

void SetupPciConfig(void)
{
    K2STAT      stat;

    if (gpMCFG == NULL)
        return;

    stat = K2OSACPI_SetPciConfigOverride(
        sReadPciConfiguration,
        sWritePciConfiguration
    );

    K2_ASSERT(!K2STAT_IS_ERROR(stat));
}

