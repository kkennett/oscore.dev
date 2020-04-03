#include "k2osacpi.h"

void *
AcpiOsMapMemory(
    ACPI_PHYSICAL_ADDRESS   Where,
    ACPI_SIZE               Length)
{
    BOOL    ok;
    UINT32  physFw;
    UINT32  virtFw;
    UINT32  sizeFw;
    UINT32  target;

    K2OSKERN_Debug("+AcpiOsMapMemory(0x%08X, 0x%08X)\n", (UINT32)Where, (UINT32)Length);

    ok = K2OS_SysGetProperty(K2OS_SYSPROP_ID_KERN_FWTABLES_PHYS_ADDR, &physFw, sizeof(UINT32));
    K2_ASSERT(ok);

    ok = K2OS_SysGetProperty(K2OS_SYSPROP_ID_KERN_FWTABLES_VIRT_ADDR, &virtFw, sizeof(UINT32));
    K2_ASSERT(ok);

    ok = K2OS_SysGetProperty(K2OS_SYSPROP_ID_KERN_FWTABLES_BYTES, &sizeFw, sizeof(UINT32));
    K2_ASSERT(ok);

    if ((Where >= physFw) && ((Where - physFw) < sizeFw))
    {
        if (Length <= (sizeFw - (Where - physFw)))
        {
            target = (((UINT32)Where) - physFw) + virtFw;
            K2OSKERN_Debug("-AcpiOsMapMemory(%08X -> %08X)\n", (UINT32)Where, target);
            return (void *)target;
        }
        else
        {
            K2OSKERN_Debug("-AcpiOsMapMemory(NULL) - range extends past map bound\n");
            return NULL;
        }
    }

    K2OSKERN_Debug("Fw Range is %08X for %08X\n", physFw, sizeFw);
    K2OSKERN_Debug("Map into non-FW range %08X for %08X\n", (UINT32)Where, (UINT32)Length);

    K2_ASSERT(0);

    return NULL;
}

void
AcpiOsUnmapMemory(
    void                    *LogicalAddress,
    ACPI_SIZE               Size)
{
    BOOL    ok;
    UINT32  virtFw;
    UINT32  sizeFw;
    UINT32  virtAddr;

    virtAddr = (UINT32)LogicalAddress;

    K2OSKERN_Debug("AcpiOsUnmapMemory(0x%08X, 0x%08X)\n", (UINT32)LogicalAddress, (UINT32)Size);

    ok = K2OS_SysGetProperty(K2OS_SYSPROP_ID_KERN_FWTABLES_VIRT_ADDR, &virtFw, sizeof(UINT32));
    K2_ASSERT(ok);

    ok = K2OS_SysGetProperty(K2OS_SYSPROP_ID_KERN_FWTABLES_BYTES, &sizeFw, sizeof(UINT32));
    K2_ASSERT(ok);

    if ((virtAddr >= virtFw) && ((virtAddr - virtFw) < sizeFw))
    {
        if (Size <= (sizeFw - (virtAddr - virtFw)))
        {
            return;
        }
    }

    K2_ASSERT(0);
}

