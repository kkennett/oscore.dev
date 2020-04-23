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

BOOL KernSched_Exec_MboxRecv(void)
{
    K2_ASSERT(0);
    return FALSE;
}

BOOL KernSched_Exec_MboxRespond(void)
{
    K2_ASSERT(0);
    return FALSE;
}

BOOL KernSched_Exec_MboxPurge(void)
{
    K2_ASSERT(0);
    return FALSE;
}

#if 0

K2STAT KernMailbox_Recv(K2OSKERN_OBJ_MAILBOX *apMailbox, K2OS_MSGIO * apRetMsgIo, UINT32 *apRetRequestId)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    OSSIM_OBJ_MSG *         pMsg;
    OSSIM_OBJ_MAILSLOT *    pSlot;
    K2LIST_LINK *           pListLink;

    stat = K2STAT_NO_ERROR;
    pMsg = NULL;

    EnterCriticalSection(&gMsgSec);

    pSlot = apMailbox->mpSlot;
    if (pSlot == NULL)
    {
        stat = K2STAT_ERROR_NOT_IN_USE;
    }
    else if (apMailbox->mState == OSSIM_MAILBOX_EMPTY)
    {
        stat = K2STAT_ERROR_EMPTY;
    }
    else
    {
        *apRetMsgIo = apMailbox->InSvcSendIo;

        if (apMailbox->mState == OSSIM_MAILBOX_IN_SERVICE_WITH_MSG)
        {
            *apRetRequestId = apMailbox->mInSvcRequestId;
        }
        else
        {
            //
            // no msg or abandoned
            //
            *apRetRequestId = 0;

            K2_ASSERT(apMailbox->mpInSvcMsg == NULL);

            if (apMailbox->mState == OSSIM_MAILBOX_IN_SERVICE_MSG_ABANDONED)
            {
                K2_ASSERT(apMailbox->mInSvcRequestId != 0);
                stat = K2STAT_ERROR_ABANDONED;
                apMailbox->mInSvcRequestId = 0;
            }
            else
            {
                K2_ASSERT(apMailbox->mInSvcRequestId == 0);
            }

            //
            // stuff below this is for the next message, not the one being retrieved now
            //
            if (pSlot->PendingMsgList.mNodeCount == 0)
            {
                //
                // no next message to retrieve. put mailbox back on slot empty list
                //
                ResetEvent(apMailbox->mhWin32Event);
                K2LIST_Remove(&pSlot->FullMailboxList, &apMailbox->SlotListLink);
                K2LIST_AddAtTail(&pSlot->EmptyMailboxList, &apMailbox->SlotListLink);
                apMailbox->mState = OSSIM_MAILBOX_EMPTY;
                K2MEM_Zero(&apMailbox->InSvcSendIo, sizeof(K2OS_MSGIO));
            }
            else
            {
                //
                // dequeue pending message for next recv
                //
                pListLink = pSlot->PendingMsgList.mpHead;
                pMsg = K2_GET_CONTAINER(OSSIM_OBJ_MSG, pListLink, SlotPendingMsgListLink);
                K2LIST_Remove(&pSlot->PendingMsgList, pListLink);
                pMsg->mpPendingOnSlot = NULL;

                apMailbox->InSvcSendIo = pMsg->Io;
                apMailbox->mInSvcRequestId = pMsg->mRequestId;

                if (pMsg->mFlags & OSSIM_MSG_FLAG_RESPONSE_REQUIRED)
                {
                    //
                    // next message is a response-required message
                    //
                    apMailbox->mpInSvcMsg = pMsg;
                    apMailbox->mState = OSSIM_MAILBOX_IN_SERVICE_WITH_MSG;
                    pMsg->mpSittingInMailbox = apMailbox;

                    K2_ASSERT(pMsg->mRequestId != 0);

                    //
                    // slot's reference to msg becomes mailbox's reference
                    //
                    pMsg = NULL;
                }
                else
                {
                    //
                    // non-response message delivered
                    //
                    apMailbox->mState = OSSIM_MAILBOX_IN_SERVICE_NO_MSG;

                    K2_ASSERT(pMsg->mRequestId == 0);
                    K2_ASSERT(pMsg->mpSittingInMailbox == NULL);
                    SetEvent(pMsg->mhWin32Event);

                    //
                    // release msg below as we removed it from the queue
                    //
                }
            }
        }
    }

    LeaveCriticalSection(&gMsgSec);

    if (pMsg != NULL)
    {
        stat2 = OSSIM_ObjectRelease(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    return stat;
}

K2STAT KernMailbox_Respond(K2OSKERN_OBJ_MAILBOX *apMailbox, UINT32 aRequestId, K2OS_MSGIO const *apRespIo)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    OSSIM_OBJ_MSG *         pMsg;
    OSSIM_OBJ_MSG *         pMsg2;
    OSSIM_OBJ_MAILSLOT *    pSlot;
    K2LIST_LINK *           pListLink;

    stat = K2STAT_NO_ERROR;
    pMsg = NULL;
    pMsg2 = NULL;

    if (aRequestId == 0)
        return K2STAT_ERROR_BAD_ARGUMENT;

    EnterCriticalSection(&gMsgSec);

    pSlot = apMailbox->mpSlot;
    if (pSlot == NULL)
    {
        stat = K2STAT_ERROR_NOT_IN_USE;
    }
    else if (apMailbox->mState == OSSIM_MAILBOX_EMPTY)
    {
        stat = K2STAT_ERROR_EMPTY;
    }
    else if (aRequestId != apMailbox->mInSvcRequestId)
    {
        stat = K2STAT_ERROR_BAD_ARGUMENT;
    }
    else
    {
        K2_ASSERT(apMailbox->mState != OSSIM_MAILBOX_IN_SERVICE_NO_MSG);

        apMailbox->mInSvcRequestId = 0;

        if (apMailbox->mState == OSSIM_MAILBOX_IN_SERVICE_MSG_ABANDONED)
        {
            K2_ASSERT(apMailbox->mpInSvcMsg == NULL);
            stat = K2STAT_ERROR_ABANDONED;
        }
        else
        {
            pMsg = apMailbox->mpInSvcMsg;
            K2_ASSERT(pMsg != NULL);
            apMailbox->mpInSvcMsg = NULL;

            //
            // msg request id zeroed by reading the response
            //
            pMsg->Io = *apRespIo;
            pMsg->mpSittingInMailbox = NULL;
            SetEvent(pMsg->mhWin32Event);
            //
            // release msg below as we detached it from the message box
            //
        }

        //
        // now do next message stuff for mailbox, not the one being responded to now
        //
        if (pSlot->PendingMsgList.mNodeCount == 0)
        {
            //
            // no next message to retrieve. put mailbox back on slot empty list
            //
            ResetEvent(apMailbox->mhWin32Event);
            K2LIST_Remove(&pSlot->FullMailboxList, &apMailbox->SlotListLink);
            K2LIST_AddAtTail(&pSlot->EmptyMailboxList, &apMailbox->SlotListLink);
            apMailbox->mState = OSSIM_MAILBOX_EMPTY;
            K2MEM_Zero(&apMailbox->InSvcSendIo, sizeof(K2OS_MSGIO));
        }
        else
        {
            //
            // dequeue pending message for next recv
            //
            pListLink = pSlot->PendingMsgList.mpHead;
            pMsg2 = K2_GET_CONTAINER(OSSIM_OBJ_MSG, pListLink, SlotPendingMsgListLink);
            K2LIST_Remove(&pSlot->PendingMsgList, pListLink);
            pMsg2->mpPendingOnSlot = NULL;

            apMailbox->InSvcSendIo = pMsg2->Io;
            apMailbox->mInSvcRequestId = pMsg2->mRequestId;

            if (pMsg2->mFlags & OSSIM_MSG_FLAG_RESPONSE_REQUIRED)
            {
                //
                // next message is a response-required message
                //
                apMailbox->mpInSvcMsg = pMsg2;
                apMailbox->mState = OSSIM_MAILBOX_IN_SERVICE_WITH_MSG;
                pMsg2->mpSittingInMailbox = apMailbox;

                K2_ASSERT(pMsg2->mRequestId != 0);

                //
                // slot's reference to msg becomes mailbox's reference
                //
                pMsg2 = NULL;
            }
            else
            {
                //
                // non-response message delivered
                //
                apMailbox->mState = OSSIM_MAILBOX_IN_SERVICE_NO_MSG;

                K2_ASSERT(pMsg2->mRequestId == 0);
                K2_ASSERT(pMsg2->mpSittingInMailbox == NULL);
                SetEvent(pMsg2->mhWin32Event);

                //
                // release msg below as we removed it from the queue
                //
            }
        }
    }

    LeaveCriticalSection(&gMsgSec);

    if (pMsg != NULL)
    {
        stat2 = OSSIM_ObjectRelease(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    if (pMsg2 != NULL)
    {
        stat2 = OSSIM_ObjectRelease(&pMsg2->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    }

    return stat;
}

void KernMailbox_Dispose(K2OSKERN_OBJ_MAILBOX *apMailbox)
{
    K2STAT          stat;
    OSSIM_OBJ_MSG * pMsg;
    BOOL            check;

    K2_ASSERT(apMailbox->Hdr.mObjType == K2OS_Obj_Mailbox);
    K2_ASSERT(apMailbox->Hdr.mRefCount == 0);
    K2_ASSERT(!(apMailbox->Hdr.mObjFlags & OSSIM_OBJ_FLAG_PERMANENT));

    K2_ASSERT(apMailbox->mpSlot == NULL);

    EnterCriticalSection(&gMsgSec);

    pMsg = apMailbox->mpInSvcMsg;
    if (pMsg != NULL)
    {
        apMailbox->mpInSvcMsg = NULL;
        apMailbox->mState = OSSIM_MAILBOX_EMPTY;
        pMsg->Io.Hdr.mStatus = K2STAT_ERROR_ABANDONED;
        pMsg->mpSittingInMailbox = NULL;
        SetEvent(pMsg->mhWin32Event);
    }

    LeaveCriticalSection(&gMsgSec);

    if (pMsg != NULL)
    {
        stat = OSSIM_ObjectRelease(&pMsg->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }

    CloseHandle(apMailbox->mhWin32Event);

    check = !(apMailbox->Hdr.mObjFlags & OSSIM_OBJ_FLAG_EMBEDDED);

    K2MEM_Zero(apMailbox, sizeof(OSSIM_OBJ_MAILBOX));

    if (check)
    {
        check = K2OSAPI_HeapFree(apMailbox);
        K2_ASSERT(check);
    }
}


#endif