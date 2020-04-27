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

BOOL KernSchedEx_EventChange(K2OSKERN_OBJ_EVENT *apEvent, BOOL aSignal)
{
    K2OSKERN_SCHED_WAITENTRY *  pEntry;
    K2OSKERN_SCHED_MACROWAIT *  pWait;
    K2LIST_ANCHOR *             pAnchor;
    K2LIST_LINK *               pListLink;
    K2OSKERN_OBJ_HEADER *       pObjRel;
    BOOL                        oneSat;

    if (aSignal == FALSE)
    {
        //
        // either event is set and nothing is waiting on it
        // or event is not set and we're doing nothing
        //
        apEvent->mIsSignalled = FALSE;
        return FALSE;
    }

    //
    // setting the event
    //
    if (apEvent->mIsSignalled)
    {
        return FALSE;
    }

    if (apEvent->Hdr.WaitEntryPrioList.mNodeCount == 0)
    {
        //
        // either event is already set and nobody is waiting on it
        // or event is not set and nobody is waiting on it, so we
        // can just set it to signalled.  
        apEvent->mIsSignalled = TRUE;
        return FALSE;
    }

    pAnchor = &apEvent->Hdr.WaitEntryPrioList;
    K2_ASSERT(pAnchor->mNodeCount > 0);

    pListLink = pAnchor->mpHead;

    if (apEvent->mIsAutoReset)
    {
        //
        // try to release a single thread
        //
        do {
            pEntry = K2_GET_CONTAINER(K2OSKERN_SCHED_WAITENTRY, pListLink, WaitPrioListLink);
            pWait = K2_GET_CONTAINER(K2OSKERN_SCHED_MACROWAIT, pEntry, SchedWaitEntry[pEntry->mMacroIndex]);

            if (!pWait->mWaitAll)
                break;

            pObjRel = NULL;
            oneSat = KernSched_CheckSignalOne_SatisfyAll(pWait, pEntry, &pObjRel);
            if (NULL != pObjRel)
            {
                //
                // this object needs to be released back in user mode
                //
                K2_ASSERT(0);
            }
            if (oneSat)
                return TRUE;

            pListLink = pListLink->mpNext;

        } while (pListLink != NULL);

        if (pListLink == NULL)
        {
            //
            // could not release a single thread - only stuff waiting was WaitAll
            // 
            apEvent->mIsSignalled = TRUE;
            return FALSE;
        }

        KernSched_EndThreadWait(pWait, K2OS_WAIT_SIGNALLED_0 + pEntry->mMacroIndex);

        return TRUE;
    }

    //
    // not an auto-reset event. open the gates
    //
    apEvent->mIsSignalled = TRUE;

    //
    // release everything we can
    //
    do {
        pEntry = K2_GET_CONTAINER(K2OSKERN_SCHED_WAITENTRY, pListLink, WaitPrioListLink);
        pWait = K2_GET_CONTAINER(K2OSKERN_SCHED_MACROWAIT, pEntry, SchedWaitEntry[pEntry->mMacroIndex]);

        pListLink = pListLink->mpNext;

        if (pWait->mWaitAll)
        {
            //
            // this *MAY* remove pEntry from the list.  we have already moved 
            // the link to the next link
            //
            pObjRel = NULL;
            KernSched_CheckSignalOne_SatisfyAll(pWait, pEntry, &pObjRel);
            if (NULL != pObjRel)
            {
                //
                // this object has to be released back in user mode
                //
                K2_ASSERT(0);
            }
        }
        else
        {
            //
            // this will remove pEntry from the list.  we have already moved 
            // the link to the next link
            //
            KernSched_EndThreadWait(pWait, K2OS_WAIT_SIGNALLED_0 + pEntry->mMacroIndex);
        }

    } while (pListLink != NULL);

    return TRUE;
}

BOOL KernSched_Exec_EventChange(void)
{
    K2OSKERN_OBJ_EVENT *    pEvent;
    BOOL                    setReset;
    BOOL                    result;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_EventChange);

    pEvent = gData.Sched.mpActiveItem->Args.EventChange.mpEvent;
    setReset = gData.Sched.mpActiveItem->Args.EventChange.mSetReset;

//    K2OSKERN_Debug("SCHED:EventSetReset(%08X,%d)\n", pEvent, setReset);

    result = KernSchedEx_EventChange(pEvent, setReset);

    gData.Sched.mpActiveItem->mSchedCallResult = K2STAT_NO_ERROR;

    return result;
}
