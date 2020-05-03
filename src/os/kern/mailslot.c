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

K2STAT KernMailslot_Create(K2OSKERN_OBJ_MAILSLOT *apMailslot, K2OSKERN_OBJ_NAME *apName, UINT32 aMailboxCount, K2OSKERN_OBJ_MAILBOX **appMailbox, BOOL aInitBlocked)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    UINT32                  ix;
    K2OSKERN_OBJ_THREAD *   pThisThread;

    K2_ASSERT(apMailslot != NULL);
    K2_ASSERT(aMailboxCount > 0);
    K2_ASSERT(appMailbox != NULL);

    if (apName != NULL)
    {
        stat = KernObj_AddRef(&apName->Hdr);
        if (K2STAT_IS_ERROR(stat))
            return stat;
    }

    do {
        //
        // take references and verify boxes are not assigned to slots
        //
        for (ix = 0; ix < aMailboxCount; ix++)
        {
            if (appMailbox[ix]->mpSlot != NULL)
            {
                stat = K2STAT_ERROR_IN_USE;
                break;
            }
            stat = KernObj_AddRef(&appMailbox[ix]->Hdr);
            K2_ASSERT(!K2STAT_IS_ERROR(stat));
        }
        if (ix < aMailboxCount)
        {
            if (ix > 0)
            {
                do {
                    --ix;
                    stat2 = KernObj_Release(&appMailbox[ix]->Hdr);
                    K2_ASSERT(!K2STAT_IS_ERROR(stat2));
                } while (ix > 0);
            }
        }
        if (K2STAT_IS_ERROR(stat))
            break;

        do {
            //
            // init the slot and try to attach mailboxes to it
            //
            K2MEM_Zero(apMailslot, sizeof(K2OSKERN_OBJ_MAILSLOT));

            apMailslot->Hdr.mObjType = K2OS_Obj_Mailslot;
            apMailslot->Hdr.mObjFlags = 0;
            apMailslot->Hdr.mpName = NULL;
            apMailslot->Hdr.mRefCount = 1;
            K2LIST_Init(&apMailslot->Hdr.WaitEntryPrioList);

            K2LIST_Init(&apMailslot->PendingMsgList);
            apMailslot->mNextRequestSeq = ((UINT32)apMailslot) & K2OSKERN_MAILBOX_REQUESTSEQ_MASK;
            apMailslot->mBlocked = TRUE; // slot attaches with block set. block is removed on success if user wants it removed

            K2LIST_Init(&apMailslot->FullMailboxList);
            K2LIST_Init(&apMailslot->EmptyMailboxList);

            pThisThread = K2OSKERN_CURRENT_THREAD;

            pThisThread->Sched.Item.mSchedItemType = KernSchedItem_SlotBoxes;
            pThisThread->Sched.Item.Args.SlotBoxes.mAttach = TRUE;
            pThisThread->Sched.Item.Args.SlotBoxes.mpMailslot = apMailslot;
            pThisThread->Sched.Item.Args.SlotBoxes.mBoxCount = aMailboxCount;
            pThisThread->Sched.Item.Args.SlotBoxes.mppMailbox = appMailbox;
            KernArch_ThreadCallSched();
            stat = pThisThread->Sched.Item.mSchedCallResult;

            if (K2STAT_IS_ERROR(stat))
                break;

            do {
                stat = KernObj_Add(&apMailslot->Hdr, apName);
                if (K2STAT_IS_ERROR(stat))
                    break;

                //
                // cannot set stat to an error from this point on
                //

                if (aInitBlocked)
                    break;

                pThisThread->Sched.Item.mSchedItemType = KernSchedItem_SlotBlock;
                pThisThread->Sched.Item.Args.SlotBlock.mpMailslot = apMailslot;
                pThisThread->Sched.Item.Args.SlotBlock.mSetBlock = FALSE;
                KernArch_ThreadCallSched();
                K2_ASSERT(!K2STAT_IS_ERROR(pThisThread->Sched.Item.mSchedCallResult));

            } while (0);

            if (K2STAT_IS_ERROR(stat))
            {
                pThisThread->Sched.Item.mSchedItemType = KernSchedItem_SlotBoxes;
                pThisThread->Sched.Item.Args.SlotBoxes.mAttach = FALSE;
                pThisThread->Sched.Item.Args.SlotBoxes.mpMailslot = apMailslot;
                pThisThread->Sched.Item.Args.SlotBoxes.mBoxCount = aMailboxCount;
                pThisThread->Sched.Item.Args.SlotBoxes.mppMailbox = appMailbox;
                KernArch_ThreadCallSched();
                K2_ASSERT(!K2STAT_IS_ERROR(pThisThread->Sched.Item.mSchedCallResult));
            }

        } while (0);

        if (K2STAT_IS_ERROR(stat))
        {
            for (ix = 0; ix < aMailboxCount; ix++)
            {
                stat = KernObj_Release(&appMailbox[ix]->Hdr);
                K2_ASSERT(!K2STAT_IS_ERROR(stat));
            }
        }

    } while (0);

    if (apName != NULL)
        KernObj_Release(&apName->Hdr);

    return stat;
}

K2STAT KernMailslot_SetBlock(K2OSKERN_OBJ_MAILSLOT *apMailslot, BOOL aBlock)
{
    K2OSKERN_OBJ_THREAD *pThisThread;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_SlotBlock;
    pThisThread->Sched.Item.Args.SlotBlock.mpMailslot = apMailslot;
    pThisThread->Sched.Item.Args.SlotBlock.mSetBlock = aBlock;
    KernArch_ThreadCallSched();
    return pThisThread->Sched.Item.mSchedCallResult;
}

void KernMailslot_Dispose(K2OSKERN_OBJ_MAILSLOT *apMailslot)
{
    K2STAT                  stat;
    BOOL                    check;
    UINT32                  objCount;
    UINT32                  ix;
    K2OSKERN_OBJ_HEADER **  ppObj;

    K2_ASSERT(apMailslot != NULL);
    K2_ASSERT(apMailslot->Hdr.mObjType == K2OS_Obj_Mailslot);
    K2_ASSERT(apMailslot->Hdr.mRefCount == 0);
    K2_ASSERT(!(apMailslot->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apMailslot->PendingMsgList.mNodeCount == 0);

    objCount = apMailslot->EmptyMailboxList.mNodeCount +
        apMailslot->FullMailboxList.mNodeCount +
        apMailslot->PendingMsgList.mNodeCount;

    ppObj = K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_HEADER *) * objCount);
    K2_ASSERT(ppObj != NULL);

    K2OSKERN_OBJ_THREAD *pThisThread;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_SlotPurge;
    pThisThread->Sched.Item.Args.SlotPurge.mpMailslot = apMailslot;
    pThisThread->Sched.Item.Args.SlotPurge.mObjCount = objCount;
    pThisThread->Sched.Item.Args.SlotPurge.mppObj = ppObj;
    KernArch_ThreadCallSched();
    K2_ASSERT(!K2STAT_IS_ERROR(pThisThread->Sched.Item.mSchedCallResult));

    for (ix = 0; ix < objCount; ix++)
    {
        stat = KernObj_Release(ppObj[ix]);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }

    check = K2OS_HeapFree(ppObj);
    K2_ASSERT(check);

    check = !(apMailslot->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    K2MEM_Zero(apMailslot, sizeof(K2OSKERN_OBJ_MAILSLOT));

    if (check)
    {
        check = K2OS_HeapFree(apMailslot);
        K2_ASSERT(check);
    }
}

