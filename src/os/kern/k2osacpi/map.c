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
            return (void *)target;
        }
        else
        {
            return NULL;
        }
    }

    ok = K2OS_SysGetProperty(K2OS_SYSPROP_ID_KERN_FWFACS_PHYS_ADDR, &physFw, sizeof(UINT32));
    K2_ASSERT(ok);

    if ((Where >= physFw) && ((Where - physFw) < K2OS_KVA_FACS_SIZE))
    {
        if (Length <= (K2OS_KVA_FACS_SIZE - (Where - physFw)))
        {
            target = (((UINT32)Where) - physFw) + K2OS_KVA_FACS_BASE;
            return (void *)target;
        }
        else
        {
            return NULL;
        }
    }

    ok = K2OS_SysGetProperty(K2OS_SYSPROP_ID_KERN_XFWFACS_PHYS_ADDR, &physFw, sizeof(UINT32));
    K2_ASSERT(ok);

    if ((Where >= physFw) && ((Where - physFw) < K2OS_KVA_XFACS_SIZE))
    {
        if (Length <= (K2OS_KVA_XFACS_SIZE - (Where - physFw)))
        {
            target = (((UINT32)Where) - physFw) + K2OS_KVA_XFACS_BASE;
            return (void *)target;
        }
        else
        {
            return NULL;
        }
    }

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

    if ((virtAddr >= K2OS_KVA_FACS_BASE) && ((virtAddr - K2OS_KVA_FACS_BASE) < K2OS_KVA_FACS_SIZE))
    {
        if (Size <= (K2OS_KVA_FACS_SIZE - (virtAddr - K2OS_KVA_FACS_BASE)))
        {
            return;
        }
    }

    if ((virtAddr >= K2OS_KVA_XFACS_BASE) && ((virtAddr - K2OS_KVA_XFACS_BASE) < K2OS_KVA_XFACS_SIZE))
    {
        if (Size <= (K2OS_KVA_XFACS_SIZE - (virtAddr - K2OS_KVA_XFACS_BASE)))
        {
            return;
        }
    }
}

