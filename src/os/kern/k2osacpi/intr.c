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

#include "k2osacpi.h"

ACPI_STATUS
AcpiOsInstallInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine,
    void                    *Context)
{
    K2OSKERN_INTR_CONFIG    config;
    K2LIST_LINK *           pListLink;
    K2OSACPI_INTR *         pIntr;
    K2STAT                  stat;

    K2_ASSERT(gK2OSACPI_IntrFreeList.mNodeCount > 0);

    pListLink = gK2OSACPI_IntrFreeList.mpHead;

    K2LIST_Remove(&gK2OSACPI_IntrFreeList, pListLink);

    pIntr = K2_GET_CONTAINER(K2OSACPI_INTR, pListLink, ListLink);

    pIntr->InterruptNumber = InterruptNumber;
    pIntr->ServiceRoutine = ServiceRoutine;
    pIntr->mToken = NULL;

    K2LIST_AddAtTail(&gK2OSACPI_IntrList, pListLink);

    K2MEM_Zero(&config, sizeof(config));

    config.mSourceId = InterruptNumber;
    config.mDestCoreIx = 0;
    config.mIsLevelTriggered = TRUE;
    config.mIsActiveLow = TRUE;

    stat = K2OSKERN_InstallIntrHandler(&config, ServiceRoutine, Context, &pIntr->mToken);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine)
{
    K2_ASSERT(0);
    return AE_ERROR;
}

