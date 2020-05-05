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
    K2LIST_Init(&apMsg->Hdr.WaitEntryPrioList);

    apMsg->mState = KernMsgState_Ready;

    stat = KernEvent_Create(&apMsg->CompletionEvent, NULL, FALSE, TRUE);
    if (K2STAT_IS_ERROR(stat))
        return stat;

    apMsg->CompletionEvent.Hdr.mObjFlags |= K2OSKERN_OBJ_FLAG_EMBEDDED;

    stat = KernObj_Add(&apMsg->Hdr, NULL);
    if (K2STAT_IS_ERROR(stat))
    {
        KernObj_Release(&apMsg->CompletionEvent.Hdr);
    }

    return stat;
}

K2STAT KernMsg_Send(K2OSKERN_OBJ_MAILBOX *apMailbox, K2OSKERN_OBJ_MSG *apMsg, K2OS_MSGIO const *apMsgIo)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2STAT                  stat;

    if (apMsg->mState != KernMsgState_Ready)
    {
        return K2STAT_ERROR_NOT_READY;
    }

    if ((apMsg->Io.mOpCode & SYSMSG_OPCODE_HIGH_MASK) == SYSMSG_OPCODE_HIGH)
    {
        //
        // somebody is trying to send a system message
        //
        if (apMailbox == gData.mpMsgBox_K2OSEXEC)
        {
            //
            // somebody is trying to send a system message to the system mailslot
            //
            if (apMsg->Io.mOpCode == SYSMSG_OPCODE_THREAD_EXIT)
            {
                //
                // unsendable message. only a thread exiting can
                // generate this message from inside the scheduler
                //
                return K2STAT_ERROR_BAD_ARGUMENT;
            }
        }
    }

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_MsgSend;
    pThisThread->Sched.Item.Args.MsgSend.mpIn_Mailbox = apMailbox;
    pThisThread->Sched.Item.Args.MsgSend.mpIn_Msg = apMsg;
    pThisThread->Sched.Item.Args.MsgSend.mpIn_Io = apMsgIo;
    KernArch_ThreadCallSched();
    stat = pThisThread->Sched.Item.mSchedCallResult;

    return stat;
}

K2STAT KernMsg_Abort(K2OSKERN_OBJ_MSG *apMsg, BOOL aClear)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MSG *      pMsg;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;

    if (apMsg->CompletionEvent.mIsSignalled)
        return K2STAT_NO_ERROR;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_MsgAbort;
    pThisThread->Sched.Item.Args.MsgAbort.mpIn_Msg = apMsg;
    pThisThread->Sched.Item.Args.MsgAbort.mIn_Clear = aClear;
    pThisThread->Sched.Item.Args.MsgAbort.mpOut_MsgToRelease = NULL;
    pThisThread->Sched.Item.Args.MsgAbort.mpOut_MailboxToRelease = NULL;
    KernArch_ThreadCallSched();
    stat = pThisThread->Sched.Item.mSchedCallResult;

    pMsg = pThisThread->Sched.Item.Args.MsgAbort.mpOut_MsgToRelease;
    pMailbox = pThisThread->Sched.Item.Args.MsgAbort.mpOut_MailboxToRelease;
    if (pMsg != NULL)
    {
        K2_ASSERT(pMailbox != NULL);

        stat2 = KernObj_Release(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));

        stat2 = KernObj_Release(&pMailbox->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }
    else
    {
        K2_ASSERT(K2STAT_IS_ERROR(stat));
        K2_ASSERT(pMailbox == NULL);
    }

    return stat;
}

K2STAT KernMsg_ReadResponse(K2OSKERN_OBJ_MSG *apMsg, K2OS_MSGIO * apRetRespIo, BOOL aClear)
{
    K2OSKERN_OBJ_THREAD *   pThisThread;

    pThisThread = K2OSKERN_CURRENT_THREAD;
    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_MsgReadResp;
    pThisThread->Sched.Item.Args.MsgReadResp.mpIn_Msg = apMsg;
    pThisThread->Sched.Item.Args.MsgReadResp.mpIn_MsgIoOutBuf = apRetRespIo;
    pThisThread->Sched.Item.Args.MsgReadResp.mIn_Clear = aClear;
    KernArch_ThreadCallSched();

    return pThisThread->Sched.Item.mSchedCallResult;
}

void KernMsg_Dispose(K2OSKERN_OBJ_MSG *apMsg)
{
    BOOL check;

    K2_ASSERT(apMsg->Hdr.mObjType == K2OS_Obj_Msg);
    K2_ASSERT(apMsg->Hdr.mRefCount == 0);
    K2_ASSERT(!(apMsg->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apMsg->Hdr.WaitEntryPrioList.mNodeCount == 0);

    K2_ASSERT((apMsg->mState == KernMsgState_Ready) || (apMsg->mState == KernMsgState_Completed));
    K2_ASSERT(apMsg->CompletionEvent.mIsSignalled);
    K2_ASSERT(apMsg->mpMailbox == NULL);

    KernObj_Release(&apMsg->CompletionEvent.Hdr);

    check = !(apMsg->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    K2MEM_Zero(apMsg, sizeof(K2OSKERN_OBJ_MSG));

    if (check)
    {
        check = K2OS_HeapFree(apMsg);
        K2_ASSERT(check);
    }
}

