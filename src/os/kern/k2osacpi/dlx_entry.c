#include "k2osacpi.h"

K2STAT 
K2_CALLCONV_REGS 
dlx_entry(
    DLX *   apDlx,
    UINT32  aReason
    )
{
    K2OS_DebugPrint("k2osacpi AcpiInitializeTables\n");
    AcpiInitializeTables(NULL, 0, FALSE);

    K2OS_DebugPrint("k2osacpi AcpiInitializeSubsystem\n");
    AcpiInitializeSubsystem();

    K2OS_DebugPrint("k2osacpi AcpiEnableSubsystem\n");
    AcpiEnableSubsystem(0);

    K2OS_DebugPrint("k2osacpi AcpiInitializeObjects\n");
    AcpiInitializeObjects(0);

    K2OS_DebugPrint("k2osacpi dlx_entry completed\n");

    return 0;
}

