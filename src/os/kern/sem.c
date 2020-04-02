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

//
// The state of a semaphore object is signaled when its count is greater than zero and nonsignaled when its count is equal to zero.
//
// waiting for a semaphore waits until the count can be decreased by one
// releasing the semaphore increases the count by a particular amount
//

K2STAT KernSem_Create(K2OSKERN_OBJ_SEM *apSem, K2OSKERN_OBJ_NAME *apName, UINT32 aMaxCount, UINT32 aInitCount)
{
    K2STAT stat;

    K2_ASSERT(apSem != NULL);

    if (apName != NULL)
    {
        stat = KernObj_AddRef(&apName->Hdr);
        if (K2STAT_IS_ERROR(stat))
            return stat;
    }

    K2MEM_Zero(apSem, sizeof(K2OSKERN_OBJ_SEM));

    apSem->Hdr.mObjType = K2OS_Obj_Semaphore;
    apSem->Hdr.mObjFlags = 0;
    apSem->Hdr.mpName = NULL;
    apSem->Hdr.mRefCount = 1;
    K2LIST_Init(&apSem->Hdr.WaitingThreadsPrioList);
    apSem->mCurCount = aInitCount;
    apSem->mMaxCount = aMaxCount;

    stat = KernObj_Add(&apSem->Hdr, apName);

    if (apName != NULL)
    {
        KernObj_Release(&apName->Hdr);
    }

    return stat;
}

K2STAT KernSem_Release(K2OSKERN_OBJ_SEM *apSem, UINT32 aCount, UINT32 *apRetNewCount)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_THREAD *   pCurThread;

    if (aCount == 0)
        return K2STAT_ERROR_BAD_ARGUMENT;

    if (apRetNewCount != NULL)
        *apRetNewCount = (UINT32)-1;

    K2_ASSERT(apSem != NULL);

    stat = KernObj_AddRef(&apSem->Hdr);
    if (K2STAT_IS_ERROR(stat))
        return stat;
    
    do {
        if (apSem->mCurCount + aCount > apSem->mMaxCount)
        {
            stat = K2STAT_ERROR_BAD_ARGUMENT;
            break;
        }

        pCurThread = K2OSKERN_CURRENT_THREAD;
        pCurThread->Sched.Item.mSchedItemType = KernSchedItem_ReleaseSem;
        pCurThread->Sched.Item.Args.ReleaseSem.mpSem = apSem;
        pCurThread->Sched.Item.Args.ReleaseSem.mCount = aCount;
        KernArch_ThreadCallSched();
        stat = pCurThread->Sched.Item.mResult;
        if ((!K2STAT_IS_ERROR(stat)) && (apRetNewCount != NULL))
        {
            *apRetNewCount = pCurThread->Sched.Item.Args.ReleaseSem.mRetNewCount;
        }

    } while (0);

    stat2 = KernObj_Release(&apSem->Hdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat2));

    return stat;
}

void KernSem_Dispose(K2OSKERN_OBJ_SEM *apSem)
{
    BOOL check;

    K2_ASSERT(apSem != NULL);
    K2_ASSERT(apSem->Hdr.mObjType == K2OS_Obj_Semaphore);
    K2_ASSERT(apSem->Hdr.mRefCount == 0);
    K2_ASSERT(!(apSem->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));

    //
    // shouldnt need to do any cleanup as no references to this remain
    //
    // however if there are any waiting threads should have their waits abandoned
    //

    check = !(apSem->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    K2MEM_Zero(apSem, sizeof(K2OSKERN_OBJ_SEM));

    if (check)
    {
        check = K2OS_HeapFree(apSem);
        K2_ASSERT(check);
    }
}

