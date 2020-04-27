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

BOOL KernSched_Exec_SemRelease(void)
{
    K2OSKERN_OBJ_SEM *          pSem;
    UINT32                      relCount;
    K2OSKERN_SCHED_MACROWAIT *  pWait;
    K2OSKERN_SCHED_WAITENTRY *  pEntry;
    K2LIST_ANCHOR *             pAnchor;
    K2LIST_LINK *               pListLink;
    BOOL                        changedSomething;
    BOOL                        oneSat;
    K2OSKERN_OBJ_HEADER *       pObjRel;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_SemRelease);

    pSem = gData.Sched.mpActiveItem->Args.SemRelease.mpSem;
    relCount = gData.Sched.mpActiveItem->Args.SemRelease.mCount;

    K2_ASSERT(relCount > 0);
    
//    K2OSKERN_Debug("SCHED:SemRelease(%08X,%d)\n", pSem, relCount);

    changedSomething = FALSE;

    if ((pSem->mCurCount + relCount) > pSem->mMaxCount)
    {
        K2OSKERN_Debug("**Failed to release sem - count would exceed max\n");
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_ERROR_BAD_ARGUMENT;
    }
    else
    {
        gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

        pAnchor = &pSem->Hdr.WaitEntryPrioList;

        if (pSem->mCurCount == 0)
        {
            K2_ASSERT(pAnchor->mNodeCount > 0);

            //
            // we need to try to release threads that are waiting, up to 'relCount' of them.
            //
            pListLink = pAnchor->mpHead;
            do {
                pEntry = K2_GET_CONTAINER(K2OSKERN_SCHED_WAITENTRY, pListLink, WaitPrioListLink);
                pWait = K2_GET_CONTAINER(K2OSKERN_SCHED_MACROWAIT, pEntry, SchedWaitEntry[pEntry->mMacroIndex]);
                pListLink = pListLink->mpNext;

                if (!pWait->mWaitAll)
                {
                    --relCount;
                    KernSched_EndThreadWait(pWait, K2OS_WAIT_SIGNALLED_0 + pEntry->mMacroIndex);
                    changedSomething = TRUE;
                }
                else
                {
                    pObjRel = NULL;
                    oneSat = KernSched_CheckSignalOne_SatisfyAll(pWait, pEntry, &pObjRel);
                    if (NULL != pObjRel)
                    {
                        //
                        // this object must be released back in user mode
                        //
                        K2_ASSERT(0);
                    }
                    if (oneSat)
                    {
                        --relCount;
                        changedSomething = TRUE;
                    }
                }
            } while ((pListLink != NULL) && (relCount > 0));

            //
            // whatever is left is what increments the cur count
            //
        }
        else
        {
            K2_ASSERT(pAnchor->mNodeCount == 0);
        }

        pSem->mCurCount += relCount;
    }

    return changedSomething;  
}
