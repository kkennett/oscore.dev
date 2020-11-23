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

K2STAT KernEvent_Create(K2OSKERN_OBJ_EVENT *apEvent, K2OSKERN_OBJ_NAME *apName, BOOL aAutoReset, BOOL aInitialState)
{
    K2STAT stat;

    K2_ASSERT(apEvent != NULL);

    if (apName != NULL)
    {
        stat = KernObj_AddRef(&apName->Hdr);
        if (K2STAT_IS_ERROR(stat))
            return stat;
    }

    K2MEM_Zero(apEvent, sizeof(K2OSKERN_OBJ_EVENT));

    apEvent->Hdr.mObjType = K2OS_Obj_Event;
    apEvent->Hdr.mObjFlags = 0;
    apEvent->Hdr.mpName = NULL;
    apEvent->Hdr.mRefCount = 1;
    apEvent->Hdr.Dispose = KernEvent_Dispose;

    K2LIST_Init(&apEvent->Hdr.WaitEntryPrioList);

    apEvent->mIsAutoReset = aAutoReset;
    apEvent->mIsSignalled = aInitialState;

    stat = KernObj_Add(&apEvent->Hdr, apName);

    if (apName != NULL)
    {
        KernObj_Release(&apName->Hdr);
    }

    return stat;
}

K2STAT KernEvent_Change(K2OSKERN_OBJ_EVENT *apEvtObj, BOOL aSetReset)
{
    K2OSKERN_OBJ_THREAD *pThisThread;

    pThisThread = K2OSKERN_CURRENT_THREAD;

    pThisThread->Sched.Item.mSchedItemType = KernSchedItem_EventChange;
    pThisThread->Sched.Item.Args.EventChange.mpEvent = apEvtObj;
    pThisThread->Sched.Item.Args.EventChange.mSetReset = aSetReset;
    KernArch_ThreadCallSched();
    return pThisThread->Sched.Item.mSchedCallResult;
}

void KernEvent_Dispose(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    BOOL check;

    K2OSKERN_OBJ_EVENT *apEvent = (K2OSKERN_OBJ_EVENT *)apObjHdr;

    K2_ASSERT(apEvent != NULL);
    K2_ASSERT(apEvent->Hdr.mObjType == K2OS_Obj_Event);
    K2_ASSERT(apEvent->Hdr.mRefCount == 0);
    K2_ASSERT(!(apEvent->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apEvent->Hdr.WaitEntryPrioList.mNodeCount == 0);

    check = !(apEvent->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    K2MEM_Zero(apEvent, sizeof(K2OSKERN_OBJ_EVENT));

    if (check)
    {
        check = K2OS_HeapFree(apEvent);
        K2_ASSERT(check);
    }
}

