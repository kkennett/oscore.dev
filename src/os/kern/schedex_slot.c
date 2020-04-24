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

BOOL KernSched_Exec_SlotBlock(void)
{
    K2OSKERN_OBJ_MAILSLOT * pMailslot;
    BOOL                    setBlock;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_SlotBlock);

    pMailslot = gData.Sched.mpActiveItem->Args.SlotBlock.mpMailslot;
    setBlock = gData.Sched.mpActiveItem->Args.SlotBlock.mSetBlock;
    
//    K2OSKERN_Debug("SCHED:SlotBlock(%08X,%d)\n", pMailslot, setBlock);

    pMailslot->mBlocked = setBlock;

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    return FALSE;
}

BOOL KernSched_Exec_SlotBoxes(void)
{
    K2STAT                  stat;
    BOOL                    doAttach;
    K2OSKERN_OBJ_MAILSLOT * pMailslot;
    UINT32                  boxCount;
    K2OSKERN_OBJ_MAILBOX ** ppMailbox;
    UINT32                  ix;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_SlotBoxes);

    doAttach = gData.Sched.mpActiveItem->Args.SlotBoxes.mAttach;
    pMailslot = gData.Sched.mpActiveItem->Args.SlotBoxes.mpMailslot;
    boxCount = gData.Sched.mpActiveItem->Args.SlotBoxes.mBoxCount;
    ppMailbox = gData.Sched.mpActiveItem->Args.SlotBoxes.mppMailbox;

//    K2OSKERN_Debug("SCHED:SlotBoxes(%08X,%d)\n", pMailslot, doAttach);

    stat = K2STAT_NO_ERROR;

    if (doAttach)
    {
        if (pMailslot->EmptyMailboxList.mNodeCount > 0)
        {
            stat = K2STAT_ERROR_IN_USE;
        }
        else
        {
            //
            // must re-verify boxes are not assigned to slots and assign them to this
            // slot, setting their attachment indexes
            //
            for (ix = 0; ix < boxCount; ix++)
            {
                if (ppMailbox[ix]->mpSlot != NULL)
                {
                    stat = K2STAT_ERROR_IN_USE;
                    break;
                }
                ppMailbox[ix]->mpSlot = pMailslot;
                ppMailbox[ix]->mId = (UINT16)ix;
                K2LIST_AddAtTail(&pMailslot->EmptyMailboxList, &ppMailbox[ix]->SlotListLink);
            }
            if (K2STAT_IS_ERROR(stat))
            {
                if (ix > 0)
                {
                    do {
                        --ix;
                        ppMailbox[ix]->mpSlot = NULL;
                        ppMailbox[ix]->mId = (UINT16)-1;
                        K2LIST_Remove(&pMailslot->EmptyMailboxList, &ppMailbox[ix]->SlotListLink);
                    } while (ix > 0);
                }
            }
        }
    }
    else
    {
        if (pMailslot->EmptyMailboxList.mNodeCount == 0)
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
        }
        else
        {
            K2_ASSERT(boxCount == pMailslot->EmptyMailboxList.mNodeCount);
            for (ix = 0; ix < boxCount; ix++)
            {
                K2LIST_Remove(&pMailslot->EmptyMailboxList, &ppMailbox[ix]->SlotListLink);
                ppMailbox[ix]->mpSlot = NULL;
                ppMailbox[ix]->mId = (UINT16)-1;
            }
        }
    }

    gData.Sched.mpActiveItem->mSchedCallResult = stat;

    return FALSE;
}

BOOL KernSched_Exec_SlotPurge(void)
{
    K2OSKERN_OBJ_MAILSLOT * pMailslot;
    UINT32                  objCount;
    UINT32                  ix;
    K2LIST_LINK *           pListLink;
    K2OSKERN_OBJ_HEADER **  ppObj;
    K2OSKERN_OBJ_MAILBOX *  pMailbox;
    K2OSKERN_OBJ_MSG *      pMsg;
    BOOL                    somethingChanged;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_SlotPurge);

    pMailslot = gData.Sched.mpActiveItem->Args.SlotPurge.mpMailslot;
    objCount = gData.Sched.mpActiveItem->Args.SlotPurge.mObjCount;
    ppObj = gData.Sched.mpActiveItem->Args.SlotPurge.mppObj;

    pMailslot->mBlocked = TRUE;
    ix = 0;
    somethingChanged = FALSE;

    do {
        pListLink = pMailslot->EmptyMailboxList.mpHead;
        if (pListLink == NULL)
            break;
        pMailbox = K2_GET_CONTAINER(K2OSKERN_OBJ_MAILBOX, pListLink, SlotListLink);
        K2LIST_Remove(&pMailslot->EmptyMailboxList, &pMailbox->SlotListLink);
        pMailbox->mpSlot = NULL;
        pMailbox->mId = (UINT16)-1;
        ppObj[ix++] = &pMailbox->Hdr;
    } while (1);

    do {
        pListLink = pMailslot->FullMailboxList.mpHead;
        if (pListLink == NULL)
            break;
        pMailbox = K2_GET_CONTAINER(K2OSKERN_OBJ_MAILBOX, pListLink, SlotListLink);
        K2LIST_Remove(&pMailslot->FullMailboxList, &pMailbox->SlotListLink);
        pMailbox->mpSlot = NULL;
        pMailbox->mId = (UINT16)-1;
        ppObj[ix++] = &pMailbox->Hdr;
    } while (1);

    do {
        pListLink = pMailslot->PendingMsgList.mpHead;
        if (pListLink == NULL)
            break;
        pMsg = K2_GET_CONTAINER(K2OSKERN_OBJ_MSG, pListLink, SlotPendingMsgListLink);
        K2LIST_Remove(&pMailslot->PendingMsgList, &pMsg->SlotPendingMsgListLink);
        pMsg->mpPendingOnSlot = NULL;
        pMsg->Io.mStatus = K2STAT_ERROR_ABANDONED;

        if (!pMsg->Event.mIsSignalled)
        {
            if (KernSched_EventChange(&pMsg->Event, TRUE))
                somethingChanged = TRUE;
        }

        ppObj[ix++] = &pMsg->Hdr;
    } while (1);

    K2_ASSERT(objCount == ix);

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    return somethingChanged;
}