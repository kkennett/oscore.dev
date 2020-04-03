#include "k2osacpi.h"

ACPI_STATUS
AcpiOsReadPort(
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width)
{
#if K2_TARGET_ARCH_IS_INTEL
//    K2OSKERN_Debug("IoRead%d(%04X)\n", Width, Address);
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

