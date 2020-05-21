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

#if TRACING_ON
static UINT8 sgTraceBuffer[1024 * 1024];
#endif

//
// dlx_entry needs to call this.  all other calls are in init.c
//
void KernInit_Stage(KernInitStage aStage);

int
sNameCompare(
    UINT32          aKey,
    K2TREE_NODE *   apNode
    )
{
    K2OSKERN_OBJ_NAME *pNameObj;

    K2_ASSERT(aKey != 0);

    pNameObj = K2_GET_CONTAINER(K2OSKERN_OBJ_NAME, apNode, NameTreeNode);

    return K2ASC_CompInsLen((char const *)aKey, pNameObj->NameBuffer, K2OS_NAME_MAX_LEN + 1);
}

int
sIFaceCompare(
    UINT32          aKey,
    K2TREE_NODE *   apNode
)
{
    K2OSKERN_IFACE *pIFace;

    K2_ASSERT(aKey != 0);

    pIFace = K2_GET_CONTAINER(K2OSKERN_IFACE, apNode, IfaceTreeNode);

    return K2MEM_Compare(&(((K2OSKERN_IFACE *)aKey)->InterfaceId), &pIFace->InterfaceId, sizeof(K2_GUID128));
}

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

    gpK2OSKERN_CacheInfo = gData.mpShared->mpCacheInfo;

    K2OSKERN_SeqIntrInit(&gData.DebugSeqLock);

    gData.mCpuCount = gData.mpShared->LoadInfo.mCpuCoreCount;

    K2OSKERN_SeqIntrInit(&gData.ProcListSeqLock);
    K2LIST_Init(&gData.ProcList);

    K2OSKERN_SeqIntrInit(&gData.IntrTreeSeqLock);
    K2TREE_Init(&gData.IntrTree, NULL);             // mUserVal is source irq number

    K2OSKERN_SeqIntrInit(&gData.ServTreeSeqLock);
    K2TREE_Init(&gData.ServTree, NULL);             // mUserVal is service instance id
    K2TREE_Init(&gData.IfaceTree, sIFaceCompare);   // mUserVal is pointer to interface
    K2TREE_Init(&gData.IfInstTree, NULL);           // mUserVal is interface instance id

    K2LIST_Init(&gData.FsProvList);

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

    gData.mpShared->FuncTab.Assert = KernEx_Assert;
    gData.mpShared->FuncTab.ExTrap_Mount = KernArch_ExTrapMount;
    gData.mpShared->FuncTab.ExTrap_Dismount = KernEx_TrapDismount;
    gData.mpShared->FuncTab.RaiseException = KernArch_RaiseException;

    K2OSKERN_SeqIntrInit(&gData.ObjTreeSeqLock);

    K2TREE_Init(&gData.ObjTree, NULL);

    K2TREE_Init(&gData.NameTree, sNameCompare);

    //
    // we are going to fill in some fields in this, so we
    // clear it out now rather than when we init all the 
    // rest of the proc0 stuff
    //
    K2MEM_Zero(gpProc0, sizeof(K2OSKERN_OBJ_PROCESS));

#if TRACING_ON
    gData.mTraceBottom = (UINT32)sgTraceBuffer;
    gData.mTraceTop = gData.mTraceBottom + sizeof(sgTraceBuffer);
    gData.mTrace = gData.mTraceTop;
#endif

    //
    // first init is with no support functions.  reinit will not get called
    //
    stat = K2DLXSUPP_Init((void *)K2OS_KVA_LOADERPAGE_BASE, NULL, TRUE, TRUE);
    while(K2STAT_IS_ERROR(stat));

    stat = DLX_Acquire("k2oshal.dlx", &gData.mpDlxHal);
    while(K2STAT_IS_ERROR(stat));
    stat = DLX_Acquire("k2osacpi.dlx", &gData.mpDlxAcpi);
    while(K2STAT_IS_ERROR(stat));
    stat = DLX_Acquire("k2osexec.dlx", &gData.mpDlxExec);
    while(K2STAT_IS_ERROR(stat));
    stat = DLX_Acquire("k2oskern.dlx", &gData.mpDlxKern);
    while(K2STAT_IS_ERROR(stat));

    //
    // second init is with support functions. reinit will get called, and all dlx
    // have been acquired
    //
    gData.DlxHost.AtReInit = KernDlx_AtReInit;
    stat = K2DLXSUPP_Init((void *)K2OS_KVA_LOADERPAGE_BASE, &gData.DlxHost, TRUE, TRUE);
    while (K2STAT_IS_ERROR(stat));

    KernInit_Stage(KernInitStage_dlx_entry);

    return 0;
}

