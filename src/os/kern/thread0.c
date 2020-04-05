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

#include "kern.h"

void KernAcpi_Start(void)
{
    DLX_pf_ENTRYPOINT   acpiEntryPoint;
    K2STAT              stat;

    stat = gData.mpShared->FuncTab.GetDlxInfo(
        gData.mpShared->mpDlxAcpi,
        NULL,
        &acpiEntryPoint,
        NULL,
        NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    stat = acpiEntryPoint(gData.mpShared->mpDlxAcpi, DLX_ENTRY_REASON_LOAD);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
}

UINT32 K2_CALLCONV_REGS K2OSKERN_Thread0(void *apArg)
{
    KernInitStage initStage;
    UINT64 last, newTick;

    //
    // run stages up to multithreaded
    //
    for (initStage = KernInitStage_Threaded; initStage < KernInitStage_MultiThreaded; initStage++)
    {
        KernInit_Stage(initStage);
    }

    K2OSKERN_Debug("Up to KernInitStage_MultiThreaded\n");

    //
    // kernel init finished.  init core stuff that needs threads
    //
    gData.mpShared->FuncTab.CoreThreadedPostInit();

    //
    // find and call entrypoint for acpi dlx
    //
    KernAcpi_Start();

    //
    // main Thread0 actions can commence here
    //
#if 1
    K2OSKERN_Debug("Hang ints on\n");
    last = K2OS_SysUpTimeMs();
    while (1)
    {
        do {
            newTick = K2OS_SysUpTimeMs();
        } while (newTick - last < 1000);
        last = newTick;
        K2OSKERN_Debug("Tick %d\n", (UINT32)(newTick & 0xFFFFFFFF));
    }
#endif

    return 0xAABBCCDD;
}

