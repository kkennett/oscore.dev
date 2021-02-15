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

static 
void 
K2_CALLCONV_REGS 
sPreKernelAssert(
    char const *    apFile, 
    int             aLineNum, 
    char const *    apCondition
)
{
    while (1);
}
K2_pf_ASSERT            K2_Assert = sPreKernelAssert;
K2_pf_EXTRAP_MOUNT      K2_ExTrap_Mount = NULL;
K2_pf_EXTRAP_DISMOUNT   K2_ExTrap_Dismount = NULL;
K2_pf_RAISE_EXCEPTION   K2_RaiseException = NULL;

void KernInit_Stage(KernInitStage aStage)
{
    gData.mKernInitStage = aStage;

    // init latched modules
    KernInit_Arch();

}

static void sInit(void)
{
    KernInitStage initStage;
    for (initStage = KernInitStage_dlx_entry + 1; initStage < KernInitStage_Threaded; initStage++)
    {
        KernInit_Stage(initStage);
    }
}

void K2_CALLCONV_REGS KernExec(void)
{
    sInit();

    // should never return
    K2OSKERN_Debug("Launch Cores...\n");
    KernArch_LaunchCores();

    // must never return
    K2OSKERN_Debug("Init end - HANG\n");
    while (1);
}

