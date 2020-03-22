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
// exported 
//
K2_CACHEINFO const * gpK2OSKERN_CacheInfo;

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
    K2MEM_Zero(&gData, sizeof(KERN_DATA));

    gData.mpShared = (K2OSKERN_SHARED *)aReason;

    gpK2OSKERN_CacheInfo = gData.mpShared->mpCacheInfo;

    K2OSKERN_SeqIntrInit(&gData.DebugSeqLock);

    gData.mCpuCount = gData.mpShared->LoadInfo.mCpuCoreCount;

    gData.mpShared->FuncTab.Exec = KernExec;

    gData.mpShared->FuncTab.Debug = K2OSKERN_Debug;
    gData.mpShared->FuncTab.Panic = K2OSKERN_Panic;
    gData.mpShared->FuncTab.SetIntr = K2OSKERN_SetIntr;
    gData.mpShared->FuncTab.GetIntr = K2OSKERN_GetIntr;
    gData.mpShared->FuncTab.MicroStall = K2OSKERN_MicroStall;
    gData.mpShared->FuncTab.SysUpTimeMs = K2OS_SysUpTimeMs;
    gData.mpShared->FuncTab.SeqIntrInit = K2OSKERN_SeqIntrInit;
    gData.mpShared->FuncTab.SeqIntrLock = K2OSKERN_SeqIntrLock;
    gData.mpShared->FuncTab.SeqIntrUnlock = K2OSKERN_SeqIntrUnlock;
    gData.mpShared->FuncTab.GetCpuIndex = K2OSKERN_GetCpuIndex;

    gData.mpShared->FuncTab.SysUpTimeMs = K2OS_SysUpTimeMs;
    gData.mpShared->FuncTab.CritSecInit = K2OS_CritSecInit;
    gData.mpShared->FuncTab.CritSecEnter = K2OS_CritSecEnter;
    gData.mpShared->FuncTab.CritSecLeave = K2OS_CritSecLeave;
    gData.mpShared->FuncTab.HeapAlloc = K2OS_HeapAlloc;
    gData.mpShared->FuncTab.HeapFree = K2OS_HeapFree;

    gData.mpShared->FuncTab.DlxHost.CritSec = KernDlx_CritSec;
    gData.mpShared->FuncTab.DlxHost.Open = KernDlx_Open;
    gData.mpShared->FuncTab.DlxHost.AtReInit = KernDlx_AtReInit;
    gData.mpShared->FuncTab.DlxHost.ReadSectors = KernDlx_ReadSectors;
    gData.mpShared->FuncTab.DlxHost.Prepare = KernDlx_Prepare;
    gData.mpShared->FuncTab.DlxHost.PreCallback = KernDlx_PreCallback;
    gData.mpShared->FuncTab.DlxHost.PostCallback = KernDlx_PostCallback;
    gData.mpShared->FuncTab.DlxHost.Finalize = KernDlx_Finalize;
    gData.mpShared->FuncTab.DlxHost.Purge = KernDlx_Purge;

    gData.mpShared->FuncTab.Assert = KernEx_Assert;
    gData.mpShared->FuncTab.ExTrap_Mount = KernEx_TrapMount;
    gData.mpShared->FuncTab.ExTrap_Dismount = KernEx_TrapDismount;
    gData.mpShared->FuncTab.RaiseException = KernEx_RaiseException;

    KernInit_Stage(KernInitStage_dlx_entry);

    return 0;
}
