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

