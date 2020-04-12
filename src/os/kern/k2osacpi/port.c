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

#include "ik2osacpi.h"

ACPI_STATUS
AcpiOsReadPort(
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width)
{
#if K2_TARGET_ARCH_IS_INTEL
    if (Width == 8)
        *Value = X32_IoRead8((UINT16)Address);
    else if (Width == 16)
        *Value = X32_IoRead16((UINT16)Address);
    else if (Width == 32)
        *Value = X32_IoRead32((UINT16)Address);
    else
    {
        *Value = 0;
        return AE_ERROR;
    }
//    K2OSKERN_Debug("IoRead%d(%04X) = %d\n", Width, Address, *Value);
#else
    K2_ASSERT(0);
#endif
    return AE_OK;
}

ACPI_STATUS
AcpiOsWritePort(
    ACPI_IO_ADDRESS         Address,
    UINT32                  Value,
    UINT32                  Width)
{
#if K2_TARGET_ARCH_IS_INTEL
//    K2OSKERN_Debug("IoWrite%d(%04X, %08X)\n", Width, Address, Value);
    if (Width == 8)
        X32_IoWrite8((UINT8)Value,(UINT16)Address);
    else if (Width == 16)
        X32_IoWrite16((UINT16)Value,(UINT16)Address);
    else if (Width == 32)
        X32_IoWrite32(Value,(UINT16)Address);
    else
        return AE_ERROR;
#else
    K2_ASSERT(0);
#endif
    return AE_OK;
}

