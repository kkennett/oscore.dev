#include "k2osacpi.h"

K2STAT 
K2_CALLCONV_REGS 
dlx_entry(
    DLX *   apDlx,
    UINT32  aReason
    )
{
    K2OS_DebugPrint("k2osacpi AcpiInitializeSubsystem\n");
    AcpiInitializeSubsystem();

    K2OS_DebugPrint("k2osacpi AcpiInitializeTables\n");
    AcpiInitializeTables(NULL, 16, FALSE);

#if 0
    /* Install local handlers */
    Status = InstallHandlers();
    if (ACPI_FAILURE(Status))
    {
        ACPI_EXCEPTION((AE_INFO, Status, "While installing handlers"));
        return (Status);
    }

    /* Initialize the ACPI hardware */

    Status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status))
    {
        ACPI_EXCEPTION((AE_INFO, Status, "While enabling ACPICA"));
        return (Status);
    }

    /* Create the ACPI namespace from ACPI tables */

    Status = AcpiLoadTables();
    if (ACPI_FAILURE(Status))
    {
        ACPI_EXCEPTION((AE_INFO, Status, "While loading ACPI tables"));
        return (Status);
    }

    /* Complete the ACPI namespace object initialization */

    Status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status))
    {
        ACPI_EXCEPTION((AE_INFO, Status, "While initializing ACPICA objects"));
        return (Status);
    }
#endif

    K2OS_DebugPrint("k2osacpi dlx_entry completed\n");

    return 0;
}

