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

void KernProc_Dump(K2OSKERN_OBJ_PROCESS *apProc)
{
    K2OSKERN_Debug("\nDUMP PROCESS %08X\n", apProc);
    K2OSKERN_Debug("  Hdr.mObjType = %d\n", apProc->Hdr.mObjType);
    K2OSKERN_Debug("  Hdr.mObjFlags = %08X\n", apProc->Hdr.mObjFlags);
    K2OSKERN_Debug("  Hdr.mRefCount = %08X\n", apProc->Hdr.mRefCount);
    K2OSKERN_Debug("  mId = %d\n", apProc->mId);
    K2OSKERN_Debug("  mTransTableKVA = %08X\n", apProc->mTransTableKVA);
    K2OSKERN_Debug("  mTransTableRegVal = %08X\n", apProc->mTransTableRegVal);
    K2OSKERN_Debug("  mVirtMapKVA = %08X\n", apProc->mVirtMapKVA);
    K2OSKERN_Debug("  ThreadList.mCount = %d\n", apProc->ThreadList.mNodeCount);
}

static void sInit_AtDlxEntry(void)
{
    K2STAT stat;

    K2MEM_Zero(gpProc0, sizeof(K2OSKERN_OBJ_PROCESS));
    gpProc0->Hdr.mObjType = K2OS_Obj_Process;
    gpProc0->Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
    gpProc0->Hdr.mRefCount = 0x7FFFFFFF;
    gpProc0->mTransTableKVA = K2OS_KVA_TRANSTAB_BASE;
    gpProc0->mVirtMapKVA = K2OS_KVA_KERNVAMAP_BASE;
#if K2_TARGET_ARCH_IS_ARM
    gpProc0->mTransTableRegVal = A32_ReadTTBR0();
#elif K2_TARGET_ARCH_IS_INTEL
    gpProc0->mTransTableRegVal = X32_ReadCR3();
#else
#error !!!Unsupported Architecture
#endif

    gpProc0->mState = KernProcState_Init;

    K2OSKERN_SeqIntrInit(&gpProc0->TokSeqLock);

    K2LIST_Init(&gpProc0->ThreadList);
    K2LIST_AddAtTail(&gData.ProcList, &gpProc0->ProcListLink);
    stat = KernObj_Add(&gpProc0->Hdr, NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
}

static void sInit_Threaded(void)
{
    K2OSKERN_SeqIntrInit(&gpProc0->ThreadListSeqLock);

    K2OS_CritSecInit(&gpProc0->TlsMaskSec);
}

static void sInit_MemReady(void)
{
    K2OSKERN_TOKEN_PAGE *   pTokPage;
    UINT32                  ix;

    gpProc0->mppTokPages = (K2OSKERN_TOKEN_PAGE **)K2OS_HeapAlloc(sizeof(K2OSKERN_TOKEN_PAGE *));
    K2_ASSERT(gpProc0->mppTokPages != NULL);

    pTokPage = (K2OSKERN_TOKEN_PAGE *)(K2OS_KVA_PROC0_BASE + K2_VA32_MEMPAGE_BYTES);

    gpProc0->mppTokPages[0] = pTokPage;
    gpProc0->mTokPageCount = 1;

    pTokPage->Hdr.mPageIndex = 0;
    pTokPage->Hdr.mInUseCount = 1;  // this will prevent the page from ever going away as the inUseCount will never be zero
    for (ix = 1; ix < K2OSKERN_TOKENS_PER_PAGE; ix++)
    {
        K2LIST_AddAtTail(&gpProc0->TokFreeList, &pTokPage->Tokens[ix].FreeLink);
    }
    gpProc0->mTokSalt = K2OSKERN_TOKEN_SALT_MASK;
    gpProc0->mTokCount = 0;

    gpProc0->mState = KernProcState_Exec;
}

void KernInit_Process(void)
{
    switch (gData.mKernInitStage)
    {
    case KernInitStage_dlx_entry:
        sInit_AtDlxEntry();
        break;

    case KernInitStage_Threaded:
        sInit_Threaded();
        break;

    case KernInitStage_MemReady:
        sInit_MemReady();

    default:
        break;
    }
}

