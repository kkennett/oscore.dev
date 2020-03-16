#include "k2osacpi.h"

#define LOCBUFFER_CHARS 80

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf(
    const char              *Format,
    ...)
{
    VALIST  vList;
    char    locBuffer[LOCBUFFER_CHARS];

    locBuffer[0] = 0;

    K2_VASTART(vList, Format);

    K2ASC_PrintfVarLen(locBuffer, LOCBUFFER_CHARS, Format, vList);

    K2_VAEND(vList);

    locBuffer[LOCBUFFER_CHARS - 1] = 0;

    K2OS_DebugPrint(locBuffer);
}

void
AcpiOsVprintf(
    const char              *Format,
    va_list                 Args)
{
    char    locBuffer[LOCBUFFER_CHARS];

    locBuffer[0] = 0;
    K2ASC_PrintfVarLen(locBuffer, LOCBUFFER_CHARS, Format, Args);
    locBuffer[LOCBUFFER_CHARS - 1] = 0;
    K2OS_DebugPrint(locBuffer);
}


