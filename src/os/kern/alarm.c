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

K2STAT KernAlarm_Create(K2OSKERN_OBJ_ALARM *apAlarm, K2OSKERN_OBJ_NAME *apName, UINT32 aIntervalMs, BOOL aIsPeriodic)
{
    K2STAT                  stat;
    K2OSKERN_OBJ_THREAD *   pThisThread;

    K2_ASSERT(apAlarm != NULL);
    K2_ASSERT(aIntervalMs != 0);
    K2_ASSERT(aIntervalMs != K2OS_TIMEOUT_INFINITE);

    if (apName != NULL)
    {
        stat = KernObj_AddRef(&apName->Hdr);
        if (K2STAT_IS_ERROR(stat))
            return stat;
    }

    K2MEM_Zero(apAlarm, sizeof(K2OSKERN_OBJ_ALARM));

    apAlarm->Hdr.mObjType = K2OS_Obj_Alarm;
    apAlarm->Hdr.mObjFlags = 0;
    apAlarm->Hdr.mpName = NULL;
    apAlarm->Hdr.mRefCount = 1;

    K2LIST_Init(&apAlarm->Hdr.WaitEntryPrioList);

    apAlarm->mIsPeriodic = aIsPeriodic;
    apAlarm->mIntervalMs = aIntervalMs;

    K2OSKERN_Debug("Add Alarm %08X\n", apAlarm);
    stat = KernObj_Add(&apAlarm->Hdr, apName);
    if (!K2STAT_IS_ERROR(stat))
    {
        pThisThread = K2OSKERN_CURRENT_THREAD;
        pThisThread->Sched.Item.mSchedItemType = KernSchedItem_AlarmChange;
        pThisThread->Sched.Item.Args.AlarmChange.mpAlarm = apAlarm;
        pThisThread->Sched.Item.Args.AlarmChange.mIsMount = TRUE;
        KernArch_ThreadCallSched();
        stat = pThisThread->Sched.Item.mSchedCallResult;

        if (K2STAT_IS_ERROR(stat))
        {
            KernObj_Release(&apAlarm->Hdr);
        }
    }

    if (apName != NULL)
    {
        KernObj_Release(&apName->Hdr);
    }

    return stat;
}

void KernAlarm_Dispose(K2OSKERN_OBJ_ALARM *apAlarm)
{
    BOOL                    check;
    K2STAT                  stat;
    K2OSKERN_OBJ_THREAD *   pThisThread;

    K2_ASSERT(apAlarm != NULL);
    K2_ASSERT(apAlarm->Hdr.mObjType == K2OS_Obj_Alarm);
    K2_ASSERT(apAlarm->Hdr.mRefCount == 0);
    K2_ASSERT(!(apAlarm->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apAlarm->Hdr.WaitEntryPrioList.mNodeCount == 0);

    if (apAlarm->mIsMounted)
    {
        pThisThread = K2OSKERN_CURRENT_THREAD;
        pThisThread->Sched.Item.mSchedItemType = KernSchedItem_AlarmChange;
        pThisThread->Sched.Item.Args.AlarmChange.mpAlarm = apAlarm;
        pThisThread->Sched.Item.Args.AlarmChange.mIsMount = FALSE;
        KernArch_ThreadCallSched();
        stat = pThisThread->Sched.Item.mSchedCallResult;
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
        K2_ASSERT(!apAlarm->mIsMounted);
    }

    check = !(apAlarm->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    K2MEM_Zero(apAlarm, sizeof(K2OSKERN_OBJ_ALARM));

    if (check)
    {
        check = K2OS_HeapFree(apAlarm);
        K2_ASSERT(check);
    }
}

