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

void KernInit_Stage(KernInitStage aStage)
{
    gData.mKernInitStage = aStage;
//    KernInit_Arch();
//    KernInit_DlxHost();
//    KernInit_Mem();
//    KernInit_Process();
//    KernInit_Thread();
//    KernInit_Sched();
//    KernInit_Hal();
//    KernInit_CpuCore();
}

static void sInit(void)
{
    KernInitStage initStage;
    for (initStage = KernInitStage_dlx_entry + 1; initStage < KernInitStage_Count; initStage++)
    {
        KernInit_Stage(initStage);
    }
}

void K2_CALLCONV_REGS KernExec(void)
{
    //
    // never returns here after it launches cores
    //
    sInit();

    // should never return
//    K2OSKERN_Debug("Launch Cores...\n");
//    KernArch_LaunchCores();

    // must never return
    K2OSKERN_Debug("Init end - HANG\n");
    while (1);
}

