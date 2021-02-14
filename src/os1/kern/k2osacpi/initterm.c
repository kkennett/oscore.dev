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

#define MAX_NUM_INTR 4

K2OSACPI_INTR sgIntr[MAX_NUM_INTR];

K2LIST_ANCHOR gK2OSACPI_IntrList;
K2LIST_ANCHOR gK2OSACPI_IntrFreeList;

K2OSKERN_SEQLOCK    gK2OSACPI_CacheSeqLock;
K2LIST_ANCHOR       gK2OSACPI_CacheList;

ACPI_STATUS
AcpiOsInitialize(
    void)
{
    UINT32 ix;

    K2OSKERN_Debug("AcpiOsInitialize()\n");

    K2LIST_Init(&gK2OSACPI_IntrList);
    K2LIST_Init(&gK2OSACPI_IntrFreeList);

    for (ix = 0; ix < MAX_NUM_INTR; ix++)
    {
        K2LIST_AddAtTail(&gK2OSACPI_IntrFreeList, &sgIntr[ix].ListLink);
    }

    K2OSKERN_SeqIntrInit(&gK2OSACPI_CacheSeqLock);
    K2LIST_Init(&gK2OSACPI_CacheList);
    
    return AE_OK;
}

ACPI_STATUS
AcpiOsTerminate(
    void)
{
    return AE_OK;
}
