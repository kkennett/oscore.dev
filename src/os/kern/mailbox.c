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

K2STAT KernMailbox_Create(K2OSKERN_OBJ_MAILBOX *apMailbox, K2OSKERN_OBJ_NAME *apName, BOOL aInitBlocked)
{
    K2STAT stat;

    K2_ASSERT(apMailbox != NULL);

    K2MEM_Zero(apMailbox, sizeof(K2OSKERN_OBJ_MAILBOX));

    apMailbox->Hdr.mObjType = K2OS_Obj_Mailbox;
    apMailbox->Hdr.mObjFlags = 0;
    apMailbox->Hdr.mpName = NULL;
    apMailbox->Hdr.mRefCount = 1;
    K2LIST_Init(&apMailbox->Hdr.WaitEntryPrioList);

    apMailbox->mBlocked = aInitBlocked;
    K2LIST_Init(&apMailbox->PendingMsgList);
    K2LIST_Init(&apMailbox->InSvcMsgList);

    stat = KernEvent_Create(&apMailbox->Event, NULL, TRUE, FALSE);
    if (K2STAT_IS_ERROR(stat))
        return stat;

    apMailbox->Event.Hdr.mObjFlags |= K2OSKERN_OBJ_FLAG_EMBEDDED;

    stat = KernObj_Add(&apMailbox->Hdr, apName);
    if (K2STAT_IS_ERROR(stat))
    {
        KernObj_Release(&apMailbox->Event.Hdr);
    }

    return stat;
}

K2STAT KernMailbox_SetBlock(K2OSKERN_OBJ_MAILBOX *apMailbox, BOOL aSetBlock)
{
    K2OSKERN_OBJ_THREAD *pThisThread;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_MboxBlock;
    pThisThread->Sched.Item.Args.MboxBlock.mpMailbox = apMailbox;
    pThisThread->Sched.Item.Args.MboxBlock.mSetBlock = aSetBlock;
    KernArch_ThreadCallSched();
    return pThisThread->Sched.Item.mSchedCallResult;
}

K2STAT KernMailbox_Recv(K2OSKERN_OBJ_MAILBOX *apMailbox, K2OS_MSGIO * apRetMsgIo, UINT32 *apRetRequestId)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MSG *      pMsg;

    if (apMailbox->PendingMsgList.mNodeCount == 0)
    {
        return K2STAT_ERROR_EMPTY;
    }

    *apRetRequestId = 0;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_MboxRecv;

    pThisThread->Sched.Item.Args.MboxRecv.mpMailbox = apMailbox;
    pThisThread->Sched.Item.Args.MboxRecv.mRetRequestId = 0;
    pThisThread->Sched.Item.Args.MboxRecv.mpRetMsgIo = apRetMsgIo;
    pThisThread->Sched.Item.Args.MboxRecv.mpRetMsgRecv = NULL;
    KernArch_ThreadCallSched();
    stat = pThisThread->Sched.Item.mSchedCallResult;

    pMsg = pThisThread->Sched.Item.Args.MboxRecv.mpRetMsgRecv;
    if (pMsg != NULL)
    {
        stat2 = KernObj_Release(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (!K2STAT_IS_ERROR(stat))
    {
        *apRetRequestId = pThisThread->Sched.Item.Args.MboxRecv.mRetRequestId;
    }

    return stat;
}

K2STAT KernMailbox_Respond(K2OSKERN_OBJ_MAILBOX *apMailbox, UINT32 aRequestId, K2OS_MSGIO const *apRespIo)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MSG *      pMsg;

    if ((aRequestId == 0) || (apRespIo == NULL))
        return K2STAT_ERROR_BAD_ARGUMENT;

    if (apMailbox->InSvcMsgList.mNodeCount == 0)
    {
        return K2STAT_ERROR_NOT_FOUND;
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_MboxRespond;

    pThisThread->Sched.Item.Args.MboxRespond.mpMailbox = apMailbox;
    pThisThread->Sched.Item.Args.MboxRespond.mRequestId = aRequestId;
    pThisThread->Sched.Item.Args.MboxRespond.mpRespMsgIo = apRespIo;
    pThisThread->Sched.Item.Args.MboxRespond.mpRetMsg1 = NULL;
    pThisThread->Sched.Item.Args.MboxRespond.mpRetMsg2 = NULL;
    KernArch_ThreadCallSched();
    stat = pThisThread->Sched.Item.mSchedCallResult;

    pMsg = pThisThread->Sched.Item.Args.MboxRespond.mpRetMsg1;
    if (pMsg != NULL)
    {
        stat2 = KernObj_Release(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    pMsg = pThisThread->Sched.Item.Args.MboxRespond.mpRetMsg2;
    if (pMsg != NULL)
    {
        stat2 = KernObj_Release(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    return stat;
}

void KernMailbox_Dispose(K2OSKERN_OBJ_MAILBOX *apMailbox)
{
    BOOL    check;

    K2_ASSERT(apMailbox->Hdr.mObjType == K2OS_Obj_Mailbox);
    K2_ASSERT(apMailbox->Hdr.mRefCount == 0);
    K2_ASSERT(!(apMailbox->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apMailbox->Hdr.WaitEntryPrioList.mNodeCount == 0);

    K2_ASSERT(apMailbox->PendingMsgList.mNodeCount == 0);
    K2_ASSERT(apMailbox->InSvcMsgList.mNodeCount == 0);

    KernObj_Release(&apMailbox->Event.Hdr);

    check = !(apMailbox->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    K2MEM_Zero(apMailbox, sizeof(K2OSKERN_OBJ_MAILBOX));

    if (check)
    {
        check = K2OS_HeapFree(apMailbox);
        K2_ASSERT(check);
    }
}
