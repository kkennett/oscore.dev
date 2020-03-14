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

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecInit(K2OS_CRITSEC *apSec)
{
    K2STAT              stat;
    BOOL                result;
    K2OSKERN_CRITSEC *  pSec;

    do {
        stat = K2STAT_ERROR_BAD_ARGUMENT;

        K2_ASSERT(apSec != NULL);
        K2_ASSERT(((UINT32)apSec) >= K2OS_KVA_KERN_BASE);
        if (((NULL == apSec) || (((UINT32)apSec)) < K2OS_KVA_KERN_BASE))
            break;

        //
        // align up to cache line
        //
        pSec = (K2OSKERN_CRITSEC *)(((((UINT32)apSec) + (K2OS_MAX_CACHELINE_BYTES - 1)) / K2OS_MAX_CACHELINE_BYTES) * K2OS_MAX_CACHELINE_BYTES);

        //
        // init
        //
        pSec->mLockVal = 0;
        pSec->mRecursionCount = 0;
        pSec->mpOwner = NULL;
        pSec->ThreadOwnedCritSecLink.mpPrev = NULL;
        pSec->ThreadOwnedCritSecLink.mpNext = NULL;
        pSec->WaitingThreadPrioList.mpHead = NULL;
        pSec->WaitingThreadPrioList.mpTail = NULL;
        pSec->mSentinel = (UINT32)pSec;
        K2_CpuWriteBarrier();

        stat = K2STAT_NO_ERROR;

    } while (0);

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecTryEnter(K2OS_CRITSEC *apSec)
{
    K2STAT                  stat;
    BOOL                    result;
    K2OSKERN_CRITSEC *      pSec;
    K2OSKERN_OBJ_THREAD *   pThisThread;

    do {
        stat = K2STAT_ERROR_BAD_ARGUMENT;

        K2_ASSERT(apSec != NULL);
        K2_ASSERT(((UINT32)apSec) >= K2OS_KVA_KERN_BASE);
        if (((NULL == apSec) || (((UINT32)apSec)) >= K2OS_KVA_KERN_BASE))
            break;

        pThisThread = K2OSKERN_CURRENT_THREAD;

        //
        // align up to cache line
        //
        pSec = (K2OSKERN_CRITSEC *)(((((UINT32)apSec) + (K2OS_MAX_CACHELINE_BYTES - 1)) / K2OS_MAX_CACHELINE_BYTES) * K2OS_MAX_CACHELINE_BYTES);
        if (pSec->mSentinel != (UINT32)pSec)
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
            break;
        }

        if ((pSec->mLockVal & 0xFFFF) == pThisThread->Env.mId)
        {
            //
            // recursive enter
            //
            K2_ASSERT(pSec->mpOwner == pThisThread);
            pSec->mRecursionCount++;
            K2_CpuWriteBarrier();
            stat = K2STAT_NO_ERROR;
            break;
        }

        //
        // not a recursive enter - try to enter uncontended
        //
        if (0 != K2ATOMIC_CompareExchange(&pSec->mLockVal, pThisThread->Env.mId, 0))
        {
            stat = K2STAT_ERROR_IN_USE;
            break;
        }

        stat = K2STAT_NO_ERROR;

    } while (0);

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecEnter(K2OS_CRITSEC *apSec)
{
    K2STAT                  stat;
    BOOL                    result;
    K2OSKERN_CRITSEC *      pSec;
    K2OSKERN_OBJ_THREAD *   pThisThread;
    UINT32                  v;

    do {
        stat = K2STAT_ERROR_BAD_ARGUMENT;

        K2_ASSERT(apSec != NULL);
        K2_ASSERT(((UINT32)apSec) >= K2OS_KVA_KERN_BASE);
        if (((NULL == apSec) || (((UINT32)apSec)) >= K2OS_KVA_KERN_BASE))
            break;

        pThisThread = K2OSKERN_CURRENT_THREAD;

        //
        // align up to cache line
        //
        pSec = (K2OSKERN_CRITSEC *)(((((UINT32)apSec) + (K2OS_MAX_CACHELINE_BYTES - 1)) / K2OS_MAX_CACHELINE_BYTES) * K2OS_MAX_CACHELINE_BYTES);
        if (pSec->mSentinel != (UINT32)pSec)
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
            break;
        }

        if ((pSec->mLockVal & 0xFFFF) == pThisThread->Env.mId)
        {
            //
            // recursive enter
            //
            K2_ASSERT(pSec->mpOwner == pThisThread);
            pSec->mRecursionCount++;
            stat = K2STAT_NO_ERROR;
            break;
        }

        //
        // not a recursive enter
        //

        pThisThread->Sched.mpActionSec = pSec;

        do {
            if (pSec->mSentinel != (UINT32)pSec)
            {
                stat = K2STAT_ERROR_NOT_IN_USE;
                break;
            }

            v = pSec->mLockVal;
            K2_CpuReadBarrier();
            if (v == 0)
            {
                //
                // try to get in uncontended
                //
                if (0 == K2ATOMIC_CompareExchange(&pSec->mLockVal, pThisThread->Env.mId, 0))
                {
                    //
                    // we got in uncontended
                    //
                    K2LIST_AddAtHead(&pThisThread->Sched.OwnedCritSecList, &pSec->ThreadOwnedCritSecLink);
                    pThisThread->Sched.mpActionSec = NULL;
                    pSec->mpOwner = pThisThread;
                    pSec->mRecursionCount = 1;
                    stat = K2STAT_NO_ERROR;
                    break;
                }
            }

            //
            // try to latch our contention
            //
            if (v == K2ATOMIC_CompareExchange(&pSec->mLockVal, v + 0x10000, v))
            {
                //
                // we successfully latched in our contention
                // so we must now enter the scheduler no matter what
                //
                pThisThread->Sched.Item.mSchedItemType = KernSchedItem_Contended_Critsec;
                pThisThread->Sched.Item.Args.ContendedCritSec.mEntering = TRUE;
                KernArch_ThreadCallSched();
                //
                // entry can fail if section is destroyed
                // while we are waiting for it
                //
                stat = pThisThread->Sched.Item.mResult;
                break;
            }

            //
            // did not latch - go around again
            //
        } while (1);

        if (!K2STAT_IS_ERROR(stat))
        {
            K2_ASSERT(pSec->mRecursionCount == 1);
            K2_ASSERT(pThisThread->Sched.mpActionSec == NULL);
            K2_ASSERT((pSec->mLockVal & 0xFFFF) == pThisThread->Env.mId);
            K2_ASSERT(pSec->mpOwner == pThisThread);
        }

    } while (0);

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecLeave(K2OS_CRITSEC *apSec)
{
    K2STAT                  stat;
    BOOL                    result;
    K2OSKERN_CRITSEC *      pSec;
    K2OSKERN_OBJ_THREAD *   pThisThread;
    UINT32                  v;

    do {
        stat = K2STAT_ERROR_BAD_ARGUMENT;

        K2_ASSERT(apSec != NULL);
        K2_ASSERT(((UINT32)apSec) >= K2OS_KVA_KERN_BASE);
        if (((NULL == apSec) || (((UINT32)apSec)) >= K2OS_KVA_KERN_BASE))
            break;

        pThisThread = K2OSKERN_CURRENT_THREAD;

        //
        // align up to cache line
        //
        pSec = (K2OSKERN_CRITSEC *)(((((UINT32)apSec) + (K2OS_MAX_CACHELINE_BYTES - 1)) / K2OS_MAX_CACHELINE_BYTES) * K2OS_MAX_CACHELINE_BYTES);
        if (pSec->mSentinel != (UINT32)pSec)
        {
            stat = K2STAT_ERROR_NOT_IN_USE;
            break;
        }

        v = pSec->mLockVal;
        K2_CpuReadBarrier();

        if ((v & 0xFFFF) != pThisThread->Env.mId)
        {
            K2_ASSERT(pSec->mpOwner != pThisThread);
            stat = K2STAT_ERROR_NOT_OWNED;
            break;
        }

        K2_ASSERT(pSec->mpOwner == pThisThread);

        if (0 != --pSec->mRecursionCount)
        {
            //
            // nested leave
            //
            stat = K2STAT_NO_ERROR;
            break;
        }

        //
        // not a nested leave, recursion count already decremented
        //
        if ((v & 0xFFFF0000) == 0)
        {
            //
            // no contention right now - try to get out without entering scheduler
            //
            K2LIST_Remove(&pThisThread->Sched.OwnedCritSecList, &pSec->ThreadOwnedCritSecLink);
            pSec->mpOwner = NULL;

            if (v == K2ATOMIC_CompareExchange(&pSec->mLockVal, 0, v))
            {
                //
                // got out of uncontended sec
                //
                stat = K2STAT_NO_ERROR;
                break;
            }

            //
            // failed to get out without entering scheduler
            //
            pSec->mpOwner = pThisThread;
            K2LIST_AddAtHead(&pThisThread->Sched.OwnedCritSecList, &pSec->ThreadOwnedCritSecLink);
        }

        //
        // leaving contended sec. must enter scheduler
        //
        pThisThread->Sched.mpActionSec = pSec;
        pThisThread->Sched.Item.mSchedItemType = KernSchedItem_Contended_Critsec;
        pThisThread->Sched.Item.Args.ContendedCritSec.mEntering = FALSE;
        KernArch_ThreadCallSched();
        stat = pThisThread->Sched.Item.mResult;
        if (K2STAT_IS_ERROR(stat))
        {
            //
            // failed to leave - !!!
            //
            pSec->mRecursionCount = 1;
            break;
        }

        K2_ASSERT(pThisThread->Sched.mpActionSec == NULL);
        K2_ASSERT((pSec->mLockVal & 0xFFFF) != pThisThread->Env.mId);
        K2_ASSERT(pSec->mpOwner != pThisThread);

    } while (0);

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecDone(K2OS_CRITSEC *apSec)
{
    BOOL                    result;
    K2OSKERN_CRITSEC *      pSec;
    K2OSKERN_OBJ_THREAD *   pThisThread;
    UINT32                  v;

    result = K2OS_CritSecTryEnter(apSec);
    if (result)
    {
        //
        // we own the sec. try to kill it now
        //
        pThisThread = K2OSKERN_CURRENT_THREAD;
        pSec = (K2OSKERN_CRITSEC *)(((((UINT32)apSec) + (K2OS_MAX_CACHELINE_BYTES - 1)) / K2OS_MAX_CACHELINE_BYTES) * K2OS_MAX_CACHELINE_BYTES);
        pSec->mSentinel = 0;
        K2_CpuWriteBarrier();
        v = K2ATOMIC_And(&pSec->mLockVal, 0xFFFF0000);
        if (v != 0)
        {
            //
            // somebody is trying to get into the sec that is being destroyed. must destroy sec via scheduler
            // to get the entering threads to fail their scheduled enter operation.
            //
            pThisThread->Sched.mpActionSec = pSec;
            pThisThread->Sched.Item.mSchedItemType = KernSchedItem_Destroy_Critsec;
            KernArch_ThreadCallSched();
            K2_ASSERT(pThisThread->Sched.mpActionSec == NULL);
            K2_ASSERT(pSec->mpOwner != pThisThread);
        }
        else
        {
            pSec->mpOwner = NULL;
            K2_CpuWriteBarrier();
        }
    }

    return result;
}


