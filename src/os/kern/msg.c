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

K2STAT KernMsg_Create(K2OSKERN_OBJ_MSG *apMsg)
{
    K2STAT stat;

    K2_ASSERT(apMsg != NULL);

    K2MEM_Zero(apMsg, sizeof(K2OSKERN_OBJ_MSG));

    apMsg->Hdr.mObjType = K2OS_Obj_Msg;
    apMsg->Hdr.mObjFlags = 0;
    apMsg->Hdr.mpName = NULL;
    apMsg->Hdr.mRefCount = 1;
    K2LIST_Init(&apMsg->Hdr.WaitingThreadsPrioList);

    stat = KernEvent_Create(&apMsg->Event, NULL, TRUE, TRUE);
    if (K2STAT_IS_ERROR(stat))
        return stat;

    apMsg->Event.Hdr.mObjFlags |= K2OSKERN_OBJ_FLAG_EMBEDDED;

    stat = KernObj_Add(&apMsg->Hdr, NULL);
    if (K2STAT_IS_ERROR(stat))
    {
        KernObj_Release(&apMsg->Event.Hdr);
    }

    return stat;
}

K2STAT KernMsg_Send(K2OSKERN_OBJ_MAILSLOT *apMailslot, K2OSKERN_OBJ_MSG *apMsg, K2OS_MSGIO const *apMsgIo, BOOL aResponseRequired)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT KernMsg_Abort(K2OSKERN_OBJ_MSG *apMsg)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2STAT                  stat;
    K2STAT                  stat2;

    if (apMsg->Event.mIsSignalled)
        return K2STAT_NO_ERROR;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_MsgAbort;
    pThisThread->Sched.Item.Args.MsgAbort.mpMsgInOut = apMsg;
    KernArch_ThreadCallSched();
    stat = pThisThread->Sched.Item.mResult;

    apMsg = pThisThread->Sched.Item.Args.MsgAbort.mpMsgInOut;
    if (apMsg != NULL)
    {
        stat2 = KernObj_Release(&apMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    return stat;
}

K2STAT KernMsg_ReadResponse(K2OSKERN_OBJ_MSG *apMsg, K2OS_MSGIO * apRetRespIo, BOOL aClear)
{
    return K2STAT_ERROR_NOT_IMPL;
}

void KernMsg_Dispose(K2OSKERN_OBJ_MSG *apMsg)
{
    BOOL check;

    K2_ASSERT(apMsg->Hdr.mObjType == K2OS_Obj_Msg);
    K2_ASSERT(apMsg->Hdr.mRefCount == 0);
    K2_ASSERT(!(apMsg->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apMsg->Hdr.WaitingThreadsPrioList.mNodeCount == 0);

    K2_ASSERT(apMsg->mpPendingOnSlot == NULL);
    K2_ASSERT(apMsg->mpSittingInMailbox == NULL);

    KernObj_Release(&apMsg->Event.Hdr);

    check = !(apMsg->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    K2MEM_Zero(apMsg, sizeof(K2OSKERN_OBJ_MSG));

    if (check)
    {
        check = K2OS_HeapFree(apMsg);
        K2_ASSERT(check);
    }
}

