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

#if 0
    ACPI_STATUS
    AcpiGetIrqRoutingTable(
        ACPI_HANDLE             Device,
        ACPI_BUFFER             *RetBuffer);

    gets PCI Routing Table _PRT of PCI root bus objects
#endif

static
void
sRes_DiscoveredAcpiIrq(
    DEV_NODE *          apDevNode,
    ACPI_RESOURCE_IRQ * apAcpiRes,
    UINT32              aResLength
)
{
    UINT8 ix;

    K2OSKERN_Debug("  Resource->Length is %d\n", aResLength);
    ix = aResLength - (sizeof(ACPI_RESOURCE_IRQ) - 1);
    K2OSKERN_Debug("  This is space for %d interrupt bytes\n", ix);

    K2OSKERN_Debug("  Trigger %s, Polarity %s, Shareable %s, WakeCapable %s\n",
        apAcpiRes->Triggering ? "EDGE" : "LEVEL",
        apAcpiRes->Polarity ? "LOW" : "HIGH",
        apAcpiRes->Shareable ? "YES" : "NO",
        apAcpiRes->WakeCapable ? "YES" : "NO");
    if (apAcpiRes->InterruptCount > 0)
    {
        K2OSKERN_Debug("  InterruptCount is %d\n", apAcpiRes->InterruptCount);
        for (ix = 0; ix < apAcpiRes->InterruptCount; ix++)
        {
            K2OSKERN_Debug("  Interrupts[%d] = %d\n", ix, apAcpiRes->Interrupts[ix]);
        }
    }
    else
    {
        K2OSKERN_Debug("  InterruptCount is zero\n");
    }
}

typedef struct _ENUM_CALLCONTEXT ENUM_CALLCONTEXT;
struct _ENUM_CALLCONTEXT
{
    DEV_NODE *  mpDevNode;
    UINT32      mResCount;
};

ACPI_STATUS
Res_AcpiEnumCallback(
    ACPI_RESOURCE * Resource,
    void *          Context
)
{
    ENUM_CALLCONTEXT *  pCtx;

    pCtx = (ENUM_CALLCONTEXT *)Context;

    if (Resource->Type == ACPI_RESOURCE_TYPE_END_TAG)
    {
        if (pCtx->mResCount > 0)
        {
            //
            // this is the last callback for the device
            //
            K2OSKERN_Debug("%08X - END\n", pCtx->mpDevNode);
        }
        return AE_OK;
    }

    if (++pCtx->mResCount == 1)
    {
        //
        // this is the first callback for the device
        //
        K2OSKERN_Debug("%08X - START\n", pCtx->mpDevNode);
    }

    switch (Resource->Type)
    {
    case ACPI_RESOURCE_TYPE_IRQ:
        K2OSKERN_Debug("IRQ\n");
        sRes_DiscoveredAcpiIrq(pCtx->mpDevNode, (ACPI_RESOURCE_IRQ *)&Resource->Data, Resource->Length);
        break;
    case ACPI_RESOURCE_TYPE_DMA:
        K2OSKERN_Debug("DMA\n");
        break;
    case ACPI_RESOURCE_TYPE_IO:
        K2OSKERN_Debug("IO\n");
        break;
    case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
        K2OSKERN_Debug("FIXED_MEMORY32\n");
        break;
    case ACPI_RESOURCE_TYPE_ADDRESS16:
        K2OSKERN_Debug("ADDRESS16\n");
        break;
    case ACPI_RESOURCE_TYPE_ADDRESS32:
        K2OSKERN_Debug("ADDRESS32\n");
        break;
    case ACPI_RESOURCE_TYPE_ADDRESS64:
        K2OSKERN_Debug("ADDRESS64\n");
        break;
    default:
        K2OSKERN_Debug("Resource Type %d not handled\n", Resource->Type);
        K2_ASSERT(0);
        break;
    }
         
    return AE_OK;
}

void Res_CreateFromPciDevice(DEV_NODE *apDevNode, DEV_NODE_PCI *apDevPci)
{
    Pci_DumpRes(apDevPci);
}

void Res_EnumAndAdd(DEV_NODE *apDevNode)
{
    ACPI_STATUS         acpiStatus;
    ACPI_HANDLE         hAcpi;
    ACPI_BUFFER         ResBuffer;
    DEV_NODE_PCI *      pPci;
    ENUM_CALLCONTEXT    ctx;

    hAcpi = (ACPI_HANDLE)apDevNode->DevTreeNode.mUserVal;
    if (0 != hAcpi)
    {
        //
        // get ACPI defined resources
        //
        ResBuffer.Pointer = NULL;
        ResBuffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
        ctx.mpDevNode = apDevNode;
        ctx.mResCount = 0;
        acpiStatus = AcpiGetCurrentResources(hAcpi, &ResBuffer);
        if (!ACPI_FAILURE(acpiStatus))
        {
            AcpiWalkResourceBuffer(&ResBuffer, Res_AcpiEnumCallback, &ctx);
            K2OS_HeapFree(ResBuffer.Pointer);
        }
    }

    pPci = apDevNode->mpPci;
    if (NULL != pPci)
    {
        //
        // get PCI defined resources
        //
        Res_CreateFromPciDevice(apDevNode, pPci);
    }
}

void Res_Create(DEV_NODE *apDevNode)
{
    DEV_NODE *      pChild;
    K2LIST_LINK *   pListLink;

    Res_EnumAndAdd(apDevNode);
    pListLink = apDevNode->ChildList.mpHead;
    if (pListLink == NULL)
        return;
    do {
        pChild = K2_GET_CONTAINER(DEV_NODE, pListLink, ChildListLink);
        pListLink = pListLink->mpNext;
        Res_Create(pChild);
    } while (pListLink != NULL);
}

void Res_Init(void)
{
    //
    // scan dev tree and enumerate already-allocated resources
    //
    K2OSKERN_Debug("Res_Init()\n");
    Res_Create(gpDev_RootNode);


}