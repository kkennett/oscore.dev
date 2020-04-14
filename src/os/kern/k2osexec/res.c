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

ACPI_STATUS
Res_AcpiEnumCallback(
    ACPI_RESOURCE * Resource,
    void *          Context
)
{
    DEV_NODE *pDevNode;
    pDevNode = (DEV_NODE *)Context;

    K2OSKERN_Debug("ResType %d\n", Resource->Type);

#if 0
    ACPI_RESOURCE_IRQ                       Irq;
    ACPI_RESOURCE_DMA                       Dma;
    ACPI_RESOURCE_START_DEPENDENT           StartDpf;
    ACPI_RESOURCE_IO                        Io;
    ACPI_RESOURCE_FIXED_IO                  FixedIo;
    ACPI_RESOURCE_FIXED_DMA                 FixedDma;
    ACPI_RESOURCE_VENDOR                    Vendor;
    ACPI_RESOURCE_VENDOR_TYPED              VendorTyped;
    ACPI_RESOURCE_END_TAG                   EndTag;
    ACPI_RESOURCE_MEMORY24                  Memory24;
    ACPI_RESOURCE_MEMORY32                  Memory32;
    ACPI_RESOURCE_FIXED_MEMORY32            FixedMemory32;
    ACPI_RESOURCE_ADDRESS16                 Address16;
    ACPI_RESOURCE_ADDRESS32                 Address32;
    ACPI_RESOURCE_ADDRESS64                 Address64;
    ACPI_RESOURCE_EXTENDED_ADDRESS64        ExtAddress64;
    ACPI_RESOURCE_EXTENDED_IRQ              ExtendedIrq;
    ACPI_RESOURCE_GENERIC_REGISTER          GenericReg;
    ACPI_RESOURCE_GPIO                      Gpio;
    ACPI_RESOURCE_I2C_SERIALBUS             I2cSerialBus;
    ACPI_RESOURCE_SPI_SERIALBUS             SpiSerialBus;
    ACPI_RESOURCE_UART_SERIALBUS            UartSerialBus;
    ACPI_RESOURCE_COMMON_SERIALBUS          CommonSerialBus;
    ACPI_RESOURCE_PIN_FUNCTION              PinFunction;
    ACPI_RESOURCE_PIN_CONFIG                PinConfig;
    ACPI_RESOURCE_PIN_GROUP                 PinGroup;
    ACPI_RESOURCE_PIN_GROUP_FUNCTION        PinGroupFunction;
    ACPI_RESOURCE_PIN_GROUP_CONFIG          PinGroupConfig;
#endif

}

void Res_EnumAndAdd(DEV_NODE *apDevNode)
{
    ACPI_STATUS acpiStatus;
    ACPI_BUFFER ResBuffer;

    if (apDevNode->DevTreeNode.mUserVal != 0)
    {
        //
        // get ACPI defined resources
        //
        ResBuffer.Pointer = NULL;
        ResBuffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
        acpiStatus = AcpiGetCurrentResources((ACPI_HANDLE)apDevNode->DevTreeNode.mUserVal, &ResBuffer);
        if (!ACPI_FAILURE(acpiStatus))
        {
            AcpiWalkResourceBuffer(apResInfoBuffer, Res_AcpiEnumCallback, apDevNode);
            K2OS_HeapFree(ResBuffer.Pointer);
        }
    }
    if (apDevNode->mpPci)
    {
        //
        // get PCI defined resources
        //
    }
}

void Res_Create(DEV_NODE *apDevNode)
{
    DEV_NODE *      pChild;
    K2LIST_LINK *   pListLink;

    pListLink = apDevNode->ChildList.mpHead;
    if (pListLink == NULL)
        return;
    Res_EnumAndAdd(apDevNode);
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
    Res_Create(gDev_RootNode);


}