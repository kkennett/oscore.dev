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

static K2OSKERN_CPUCORE * volatile  sgpPanicCore = NULL;

void 
KernPanic_Ici(
    K2OSKERN_CPUCORE *apThisCore
)
{
    K2OSKERN_SetIntr(FALSE);
    K2OSKERN_Debug("--Core %d intentional hang due to panic on core %d\n", apThisCore->mCoreIx, sgpPanicCore->mCoreIx);
    K2ATOMIC_Inc(&gData.mCoresInPanic);
    while (1);
}

void
K2OSKERN_Panic(
    char const *apFormat,
    ...
)
{
    K2OSKERN_CPUCORE *  pThisCore;
    VALIST              vList;

    K2OSKERN_SetIntr(FALSE);

    pThisCore = K2OSKERN_GET_CURRENT_CPUCORE;

    if (0 != K2ATOMIC_CompareExchange((UINT32 volatile *)&sgpPanicCore, (UINT32)pThisCore, 0))
    {
        K2OSKERN_Debug("Lead panic core %d simultaneously panic with core %d\n", sgpPanicCore->mCoreIx, pThisCore->mCoreIx);
        KernPanic_Ici(pThisCore);
        while (1);
    }

    K2ATOMIC_Inc(&gData.mCoresInPanic);

    K2OSKERN_Debug("\n\n-------------------\nCore %d PANIC:\n  ", pThisCore->mCoreIx);
    
    if (apFormat != NULL)
    {
        K2_VASTART(vList, apFormat);

        KernDebug_OutputWithArgs(apFormat, vList);

        K2_VAEND(vList);
    }

    if (gData.mCpuCount > 1)
    {
        KernCpuCore_SendIciToAllOtherCores(pThisCore, KernCpuCoreEvent_Ici_Panic);
        do {
            K2_CpuReadBarrier();
            if (gData.mpShared->FuncTab.MicroStall)
                gData.mpShared->FuncTab.MicroStall(100);
        } while (gData.mCoresInPanic < gData.mCpuCount);
    }

    KernArch_Panic(pThisCore, (apFormat == NULL) ? FALSE : TRUE);

    while (1);
}

