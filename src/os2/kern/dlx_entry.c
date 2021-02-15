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

//
// global
//
KERN_DATA gData;

//
// dlx_entry needs to call this.  all other calls are in init.c
//
void KernInit_Stage(KernInitStage aStage);


K2STAT 
K2_CALLCONV_REGS 
dlx_entry(
    DLX *   apDlx,
    UINT32  aReason
    )
{
    K2STAT stat;

    K2MEM_Zero(&gData, sizeof(KERN_DATA));

    gData.mpShared = (K2OSKERN_SHARED *)aReason;

    K2OSKERN_SeqIntrInit(&gData.DebugSeqLock);

    gData.mCpuCount = gData.mpShared->LoadInfo.mCpuCoreCount;

    gData.mpShared->FuncTab.Exec = KernExec;

    gData.mpShared->FuncTab.Debug = K2OSKERN_Debug;
    gData.mpShared->FuncTab.Panic = K2OSKERN_Panic;
    gData.mpShared->FuncTab.SetIntr = K2OSKERN_SetIntr;
    gData.mpShared->FuncTab.GetIntr = K2OSKERN_GetIntr;
    gData.mpShared->FuncTab.MicroStall = K2OSKERN_MicroStall;
    gData.mpShared->FuncTab.SeqIntrInit = K2OSKERN_SeqIntrInit;
    gData.mpShared->FuncTab.SeqIntrLock = K2OSKERN_SeqIntrLock;
    gData.mpShared->FuncTab.SeqIntrUnlock = K2OSKERN_SeqIntrUnlock;
    gData.mpShared->FuncTab.GetCpuIndex = K2OSKERN_GetCpuIndex;

    gData.mpShared->FuncTab.Assert = KernEx_Assert;
    gData.mpShared->FuncTab.ExTrap_Mount = KernEx_TrapMount;
    gData.mpShared->FuncTab.ExTrap_Dismount = KernEx_TrapDismount;

    //
    // first init is with no support functions.  reinit will not get called
    //
    stat = K2DLXSUPP_Init((void *)K2OS_KVA_LOADERPAGE_BASE, NULL, TRUE, TRUE);
    while(K2STAT_IS_ERROR(stat));

    stat = DLX_Acquire("k2oshal.dlx", NULL, &gData.mpDlxHal);
    while(K2STAT_IS_ERROR(stat));

    stat = DLX_Acquire("k2oskern.dlx", NULL, &gData.mpDlxKern);
    while(K2STAT_IS_ERROR(stat));

    //
    // second init is with support functions. reinit will get called, and all dlx
    // have been acquired
    //
    gData.DlxHost.mHostSizeBytes = sizeof(gData.DlxHost);
    gData.DlxHost.AtReInit = KernDlxSupp_AtReInit;
    stat = K2DLXSUPP_Init((void *)K2OS_KVA_LOADERPAGE_BASE, &gData.DlxHost, TRUE, TRUE);
    while (K2STAT_IS_ERROR(stat));

    KernInit_Stage(KernInitStage_dlx_entry);

    return 0;
}

