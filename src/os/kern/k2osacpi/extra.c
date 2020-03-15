

ACPI_STATUS
AcpiOsReadPciConfiguration(
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    UINT64                  *Value,
    UINT32                  Width)
{
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsWritePciConfiguration(
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    UINT64                  Value,
    UINT32                  Width)
{
    return ACPI_FAILURE(1);
}

BOOLEAN
AcpiOsReadable(
    void                    *Pointer,
    ACPI_SIZE               Length)
{
    return FALSE;
}

BOOLEAN
AcpiOsWritable(
    void                    *Pointer,
    ACPI_SIZE               Length)
{
    return FALSE;
}

ACPI_STATUS
AcpiOsEnterSleep(
    UINT8                   SleepState,
    UINT32                  RegaValue,
    UINT32                  RegbValue)
{
    return ACPI_FAILURE(1);
}

void
AcpiOsRedirectOutput(
    void                    *Destination)
{
    
}

ACPI_STATUS
AcpiOsGetLine(
    char                    *Buffer,
    UINT32                  BufferLength,
    UINT32                  *BytesRead)
{
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsInitializeDebugger(
    void)
{
    return ACPI_FAILURE(1);
}

void
AcpiOsTerminateDebugger(
    void)
{
    
}

ACPI_STATUS
AcpiOsGetTableByName(
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsGetTableByIndex(
    UINT32                  Index,
    ACPI_TABLE_HEADER       **Table,
    UINT32                  *Instance,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    return ACPI_FAILURE(1);
}

ACPI_STATUS
AcpiOsGetTableByAddress(
    ACPI_PHYSICAL_ADDRESS   Address,
    ACPI_TABLE_HEADER       **Table)
{
    return ACPI_FAILURE(1);
}

void *
AcpiOsOpenDirectory(
    char                    *Pathname,
    char                    *WildcardSpec,
    char                    RequestedFileType)
{
    return NULL;
}

char *
AcpiOsGetNextFilename(
    void                    *DirHandle)
{
    return NULL;
}

void
AcpiOsCloseDirectory(
    void                    *DirHandle)
{
    
}



