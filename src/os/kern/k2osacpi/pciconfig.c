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

#include "k2osacpi.h"
#if K2_TARGET_ARCH_IS_INTEL
#include <spec/x32pcdef.inc>
#endif

//
// this is for port access.  memory mapped access if applicable is done 
// via a handler installed externally
//

typedef union _PCIADDR PCIADDR;
union _PCIADDR
{
    UINT32  mAsUINT32;
    struct
    {
        UINT32 mSBZ : 2;
        UINT32 mWord : 6;
        UINT32 mFunction : 3;
        UINT32 mDevice : 5;
        UINT32 mBus : 8;
        UINT32 mReserved : 7;
        UINT32 mSBO : 1;
    };
};
K2_STATIC_ASSERT(sizeof(PCIADDR) == sizeof(UINT32));

ACPI_STATUS
AcpiOsReadPciConfiguration(
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    UINT64                  *Value,
    UINT32                  Width)
{
    PCIADDR addr;

#if !K2_TARGET_ARCH_IS_INTEL
    *Value = 0;
    return AE_OK;
#endif

    K2OSKERN_Debug("ReadPciConfig(%d_%d/%d/%d, Reg %d, Width %d\n", PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, Width);

    addr.mAsUINT32 = 0;
    addr.mSBO = 1;
    addr.mBus = PciId->Bus;
    addr.mDevice = PciId->Device;
    addr.mFunction = PciId->Function;
    addr.mWord = Reg;

    X32_IoWrite32(addr.mAsUINT32, X32PC_PCI_CONFIG_ADDR_IOPORT);

    switch (Width)
    {
    case 8:
        *Value = (UINT64)X32_IoRead8(X32PC_PCI_CONFIG_DATA_IOPORT);
        break;
    case 16:
        *Value = (UINT64)X32_IoRead16(X32PC_PCI_CONFIG_DATA_IOPORT);
        break;
    case 32:
        *Value = (UINT64)X32_IoRead32(X32PC_PCI_CONFIG_DATA_IOPORT);
        break;
    default:
        K2_ASSERT(0);
    }

    return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration(
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    UINT64                  Value,
    UINT32                  Width)
{
#if !K2_TARGET_ARCH_IS_INTEL



    return AE_ERROR;

#endif


    K2_ASSERT(0);
    return AE_ERROR;
}

