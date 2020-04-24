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
    K2STAT                  stat;
    K2OSKERN_OBJ_MSG *      pMsg;
    K2OSKERN_OBJ_MAILSLOT * pSlot;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    K2LIST_LINK *           pListLink;
    BOOL                    changedSomething;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MboxRecv);

    pMailbox = gData.Sched.mpActiveItem->Args.MboxRecv.mpMailbox;

//    K2OSKERN_Debug("SCHED:MboxRecv(%08X)\n", pMailbox);

    stat = K2STAT_NO_ERROR;

    changedSomething = FALSE;

    gData.Sched.mpActiveItem->Args.MboxRecv.mpRetMsgRecv = NULL;

    pSlot = pMailbox->mpSlot;
    if (pSlot == NULL)
    {
        stat = K2STAT_ERROR_NOT_IN_USE;
    }
    else if (pMailbox->mState == K2OSKERN_MAILBOX_EMPTY)
    {
        stat = K2STAT_ERROR_EMPTY;
    }
    else
    {
        K2MEM_Copy(gData.Sched.mpActiveItem->Args.MboxRecv.mpRetMsgIo, &pMailbox->InSvcSendIo, sizeof(K2OS_MSGIO));

        if (pMailbox->mState == K2OSKERN_MAILBOX_IN_SERVICE_WITH_MSG)
        {
            gData.Sched.mpActiveItem->Args.MboxRecv.mRetRequestId = pMailbox->mInSvcRequestId;
        }
        else
        {
            //
            // no msg or abandoned
            //
            gData.Sched.mpActiveItem->Args.MboxRecv.mRetRequestId = 0;

            K2_ASSERT(pMailbox->mpInSvcMsg == NULL);

            if (pMailbox->mState == K2OSKERN_MAILBOX_IN_SERVICE_MSG_ABANDONED)
            {
                K2_ASSERT(pMailbox->mInSvcRequestId != 0);
                stat = K2STAT_ERROR_ABANDONED;
                pMailbox->mInSvcRequestId = 0;
            }
            else
            {
                K2_ASSERT(pMailbox->mInSvcRequestId == 0);
            }

            //
            // stuff below this is for the next message, not the one being retrieved now
            //
            if (pSlot->PendingMsgList.mNodeCount == 0)
            {
                //
                // no next message to retrieve. put mailbox back on slot empty list
                //
                KernSchedEx_EventChange(&pMailbox->Event, FALSE);
                K2LIST_Remove(&pSlot->FullMailboxList, &pMailbox->SlotListLink);
                K2LIST_AddAtTail(&pSlot->EmptyMailboxList, &pMailbox->SlotListLink);
                pMailbox->mState = K2OSKERN_MAILBOX_EMPTY;
                K2MEM_Zero(&pMailbox->InSvcSendIo, sizeof(K2OS_MSGIO));
            }
            else
            {
                //
                // dequeue pending message for next recv
                //
                pListLink = pSlot->PendingMsgList.mpHead;
                pMsg = K2_GET_CONTAINER(K2OSKERN_OBJ_MSG, pListLink, SlotPendingMsgListLink);
                K2LIST_Remove(&pSlot->PendingMsgList, pListLink);
                pMsg->mpPendingOnSlot = NULL;

                K2MEM_Copy(&pMailbox->InSvcSendIo, &pMsg->Io, sizeof(K2OS_MSGIO));
                pMailbox->mInSvcRequestId = pMsg->mRequestId;

                if (pMsg->mFlags & K2OSKERN_MSG_FLAG_RESPONSE_REQUIRED)
                {
                    //
                    // next message is a response-required message
                    //
                    pMailbox->mpInSvcMsg = pMsg;
                    pMailbox->mState = K2OSKERN_MAILBOX_IN_SERVICE_WITH_MSG;
                    pMsg->mpSittingInMailbox = pMailbox;

                    K2_ASSERT(pMsg->mRequestId != 0);

                    //
                    // slot's reference to msg becomes mailbox's reference
                    //
                }
                else
                {
                    //
                    // non-response message delivered
                    //
                    pMailbox->mState = K2OSKERN_MAILBOX_IN_SERVICE_NO_MSG;

                    K2_ASSERT(pMsg->mRequestId == 0);
                    K2_ASSERT(pMsg->mpSittingInMailbox == NULL);

                    if (KernSchedEx_EventChange(&pMsg->Event, TRUE))
                        changedSomething = TRUE;

                    //
                    // release msg at exit of scheduler call
                    //
                    gData.Sched.mpActiveItem->Args.MboxRecv.mpRetMsgRecv = pMsg;
                }

                if (KernSchedEx_EventChange(&pMailbox->Event, TRUE))
                    changedSomething = TRUE;
            }
        }
    }

    gData.Sched.mpActiveItem->mSchedCallResult = stat;

    return changedSomething;
}

BOOL KernSched_Exec_MboxRespond(void)
{
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    UINT32                  requestId;
    K2OS_MSGIO const *      pRespMsgIo;
    K2OSKERN_OBJ_MAILSLOT * pSlot;
    K2OSKERN_OBJ_MSG *      pMsg;
    K2LIST_LINK *           pListLink;
    K2STAT                  stat;
    BOOL                    changedSomething;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MboxRespond);

    pMailbox = gData.Sched.mpActiveItem->Args.MboxRespond.mpMailbox;
    requestId = gData.Sched.mpActiveItem->Args.MboxRespond.mRequestId;
    pRespMsgIo = gData.Sched.mpActiveItem->Args.MboxRespond.mpRespMsgIo;

    K2OSKERN_Debug("SCHED:MboxRespond(%08X)\n", pMailbox);

    changedSomething = FALSE;

    stat = K2STAT_NO_ERROR;

    gData.Sched.mpActiveItem->Args.MboxRespond.mpRetMsg1 = NULL;
    gData.Sched.mpActiveItem->Args.MboxRespond.mpRetMsg2 = NULL;

    pSlot = pMailbox->mpSlot;
    if (pSlot == NULL)
    {
        stat = K2STAT_ERROR_NOT_IN_USE;
    }
    else if (pMailbox->mState == K2OSKERN_MAILBOX_EMPTY)
    {
        stat = K2STAT_ERROR_EMPTY;
    }
    else if (requestId != pMailbox->mInSvcRequestId)
    {
        stat = K2STAT_ERROR_BAD_ARGUMENT;
    }
    else
    {
        K2_ASSERT(pMailbox->mState != K2OSKERN_MAILBOX_IN_SERVICE_NO_MSG);

        pMailbox->mInSvcRequestId = 0;

        if (pMailbox->mState == K2OSKERN_MAILBOX_IN_SERVICE_MSG_ABANDONED)
        {
            K2_ASSERT(pMailbox->mpInSvcMsg == NULL);
            stat = K2STAT_ERROR_ABANDONED;
        }
        else
        {
            pMsg = pMailbox->mpInSvcMsg;
            K2_ASSERT(pMsg != NULL);
            pMailbox->mpInSvcMsg = NULL;

            //
            // msg request id zeroed by reading the response
            //
            K2MEM_Copy(&pMsg->Io, pRespMsgIo, sizeof(K2OS_MSGIO));
            pMsg->mpSittingInMailbox = NULL;
            K2OSKERN_Debug("Setting msg event (response)\n");
            if (KernSchedEx_EventChange(&pMsg->Event, TRUE))
            {
                K2OSKERN_Debug("released thread that was waiting on msg\n");
                changedSomething = TRUE;
            }

            //
            // release msg below as we detached it from the message box
            //
            gData.Sched.mpActiveItem->Args.MboxRespond.mpRetMsg1 = pMsg;
        }

        //
        // now do next message stuff for mailbox, not the one being responded to now
        //
        if (pSlot->PendingMsgList.mNodeCount == 0)
        {
            //
            // no next message to retrieve. put mailbox back on slot empty list
            //
            KernSchedEx_EventChange(&pMailbox->Event, FALSE);
            K2LIST_Remove(&pSlot->FullMailboxList, &pMailbox->SlotListLink);
            K2LIST_AddAtTail(&pSlot->EmptyMailboxList, &pMailbox->SlotListLink);
            pMailbox->mState = K2OSKERN_MAILBOX_EMPTY;
            K2MEM_Zero(&pMailbox->InSvcSendIo, sizeof(K2OS_MSGIO));
        }
        else
        {
            //
            // dequeue pending message for next recv
            //
            pListLink = pSlot->PendingMsgList.mpHead;
            pMsg = K2_GET_CONTAINER(K2OSKERN_OBJ_MSG, pListLink, SlotPendingMsgListLink);
            K2LIST_Remove(&pSlot->PendingMsgList, pListLink);
            pMsg->mpPendingOnSlot = NULL;

            K2MEM_Copy(&pMailbox->InSvcSendIo, &pMsg->Io, sizeof(K2OS_MSGIO));
            pMailbox->mInSvcRequestId = pMsg->mRequestId;

            if (pMsg->mFlags & K2OSKERN_MSG_FLAG_RESPONSE_REQUIRED)
            {
                //
                // next message is a response-required message
                //
                pMailbox->mpInSvcMsg = pMsg;
                pMailbox->mState = K2OSKERN_MAILBOX_IN_SERVICE_WITH_MSG;
                pMsg->mpSittingInMailbox = pMailbox;

                K2_ASSERT(pMsg->mRequestId != 0);

                //
                // slot's reference to msg becomes mailbox's reference
                //
            }
            else
            {
                //
                // non-response message delivered
                //
                pMailbox->mState = K2OSKERN_MAILBOX_IN_SERVICE_NO_MSG;

                K2_ASSERT(pMsg->mRequestId == 0);
                K2_ASSERT(pMsg->mpSittingInMailbox == NULL);
                if (KernSchedEx_EventChange(&pMsg->Event, TRUE))
                    changedSomething = TRUE;

                //
                // release msg below as we removed it from the queue
                //
                gData.Sched.mpActiveItem->Args.MboxRespond.mpRetMsg2 = pMsg;
            }

            if (KernSchedEx_EventChange(&pMailbox->Event, TRUE))
                changedSomething = TRUE;
        }
    }

    gData.Sched.mpActiveItem->mSchedCallResult = stat;

    return changedSomething;
}

BOOL KernSched_Exec_MboxPurge(void)
{
    K2OSKERN_OBJ_MSG *      pMsg;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    BOOL                    changedSomething;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_MboxPurge);

    pMailbox = gData.Sched.mpActiveItem->Args.MboxPurge.mpMailbox;

//    K2OSKERN_Debug("SCHED:MboxPurge(%08X)\n", pMailbox);

    changedSomething = FALSE;

    gData.Sched.mpActiveItem->Args.MboxPurge.mpRetMsgPurge = pMsg = pMailbox->mpInSvcMsg;

    if (pMsg != NULL)
    {
        pMailbox->mpInSvcMsg = NULL;
        pMailbox->mState = K2OSKERN_MAILBOX_EMPTY;
        pMsg->Io.mStatus = K2STAT_ERROR_ABANDONED;
        pMsg->mpSittingInMailbox = NULL;
        changedSomething = KernSchedEx_EventChange(&pMsg->Event, TRUE);
    }

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    return changedSomething;
}

