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

static
void 
sRes_QueryPciDeviceBarSizes(
    DEV_NODE *      apDevNode, 
    DEV_NODE_PCI *  apPci
)
{
    ACPI_STATUS acpiStatus;
    UINT8       val8;
    UINT32      barRegOffset;
    UINT32      numBars;
    UINT32      barSave;
    UINT32      barVal;
    UINT64      val64;
    UINT32      ix;
    UINT32      maskVal;
    BOOL        disp;

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
                if (barSave != 0)
                {
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
            }
            barRegOffset += sizeof(UINT32);
        }
    }

    K2OSKERN_SetIntr(disp);
}

static 
void 
sRes_EnumCurrent(
    DEV_NODE *apDevNode
)
{
    ACPI_HANDLE         hAcpi;
    DEV_NODE_PCI *      pPci;

    //
    // get current resources from ACPI
    //
    hAcpi = (ACPI_HANDLE)apDevNode->DevTreeNode.mUserVal;
    if (0 != hAcpi)
    {
        apDevNode->Res.Device.CurrentAcpiRes.Pointer = NULL;
        apDevNode->Res.Device.CurrentAcpiRes.Length = ACPI_ALLOCATE_BUFFER;
        AcpiGetCurrentResources(hAcpi, &apDevNode->Res.Device.CurrentAcpiRes);
    }

    //
    // get PCI BAR resource sizes (IO, MEMORY sizes/alignment)
    //
    pPci = apDevNode->mpPci;
    if (NULL != pPci)
    {
        sRes_QueryPciDeviceBarSizes(apDevNode, pPci);
    }
}

static 
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

void Res_Init(void)
{
    //
    // scan dev tree and enumerate already-allocated resources
    //
    sRes_EnumAll(gpDev_RootNode);
}
