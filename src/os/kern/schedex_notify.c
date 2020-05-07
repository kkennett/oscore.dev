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

BOOL KernSched_Exec_NotifyLatch(void)
{
    K2OSKERN_NOTIFY_BLOCK * pBlock;
    K2OSKERN_OBJ_NOTIFY *   pNotify;
    UINT32                  ix;
    BOOL                    changedSomething;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_NotifyLatch);

    pBlock = gData.Sched.mpActiveItem->Args.NotifyLatch.mpIn_NotifyBlock;
    pBlock->mActiveCount = pBlock->mTotalCount;

    changedSomething = FALSE;

    for (ix = 0; ix < pBlock->mTotalCount; ix++)
    {
        K2_ASSERT(pBlock->Rec[ix].mRecBlockIndex == ix);
        pNotify = pBlock->Rec[ix].mpSubscrip->mpNotify;
        pBlock->Rec[ix].mpSubscrip = NULL;  // caller holds reference separately
        pBlock->Rec[ix].mpNotify = pNotify;
        KernObj_AddRef(&pNotify->Hdr);
        K2LIST_AddAtTail(&pNotify->RecList, &pBlock->Rec[ix].NotifyRecListLink);
        if (KernSchedEx_EventChange(&pNotify->AvailEvent, TRUE))
            changedSomething = TRUE;
    }

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    return changedSomething;
}

BOOL KernSched_Exec_NotifyRead(void)
{
    K2OSKERN_NOTIFY_BLOCK * pBlock;
    K2OSKERN_OBJ_NOTIFY *   pNotify;
    K2OSKERN_NOTIFY_REC *   pRec;
    BOOL                    changedSomething;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_NotifyRead);

    pNotify = gData.Sched.mpActiveItem->Args.NotifyRead.mpIn_Notify;

    if (pNotify->RecList.mNodeCount == 0)
    {
        gData.Sched.mpActiveItem->Args.NotifyRead.mpOut_NotifyBlockToRelease = NULL;
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_EMPTY;
        return FALSE;
    }

    changedSomething = FALSE;
    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    //
    // remove notify record from the active list
    //
    pRec = K2_GET_CONTAINER(K2OSKERN_NOTIFY_REC, pNotify->RecList.mpHead, NotifyRecListLink);
    K2_ASSERT(pRec->mpNotify == pNotify);
    K2LIST_Remove(&pNotify->RecList, &pRec->NotifyRecListLink);
    if (pNotify->RecList.mNodeCount == 0)
    {
        if (KernSchedEx_EventChange(&pNotify->AvailEvent, FALSE))
            changedSomething = TRUE;
    }

    pBlock = K2_GET_CONTAINER(K2OSKERN_NOTIFY_BLOCK, pRec, Rec[pRec->mRecBlockIndex]);
    K2_ASSERT(pBlock->mActiveCount > 0);
    if (--pBlock->mActiveCount == 0)
    {
        gData.Sched.mpActiveItem->Args.NotifyRead.mpOut_NotifyBlockToRelease = pBlock;
    }
    else
    {
        gData.Sched.mpActiveItem->Args.NotifyRead.mpOut_NotifyBlockToRelease = NULL;
    }

    gData.Sched.mpActiveItem->Args.NotifyRead.mOut_IsArrival = pBlock->mIsArrival;
    K2MEM_Copy(gData.Sched.mpActiveItem->Args.NotifyRead.mpOut_InterfaceId, &pBlock->InterfaceId, sizeof(K2_GUID128));
    gData.Sched.mpActiveItem->Args.NotifyRead.mOut_Context = pRec->mpContext;
    K2MEM_Copy(gData.Sched.mpActiveItem->Args.NotifyRead.mpOut_IfInst, &pBlock->Instance, sizeof(K2OSKERN_SVC_IFINST));

    return changedSomething;
}
