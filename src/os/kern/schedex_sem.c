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

BOOL KernSched_Exec_ReleaseSem(void)
{
    BOOL                result;
    K2OSKERN_OBJ_SEM *  pSem;
    UINT32              relCount;

    K2_ASSERT(gData.Sched.mpActiveItem->mSchedItemType == KernSchedItem_ReleaseSem);

    pSem = gData.Sched.mpActiveItem->Args.ReleaseSem.mpSem;
    relCount = gData.Sched.mpActiveItem->Args.ReleaseSem.mCount;

    K2OSKERN_Debug("SCHED:ReleaseSem(%08X,%d)\n", pSem, relCount);

    result = FALSE;

    if ((pSem->mCurCount + relCount) > pSem->mMaxCount)
    {
        K2OSKERN_Debug("**Failed to release sem - count would exceed max\n");
        gData.Sched.mpActiveItem->mResult = K2STAT_ERROR_BAD_ARGUMENT;
    }
    else if ((pSem->mCurCount == 0) && (pSem->Hdr.WaitingThreadsPrioList.mNodeCount > 0))
    {
        //
        // we need to release threads that are waiting, up to 'relCount' of them.
        //
        K2_ASSERT(0);
        result = TRUE;
    }

    pSem->mCurCount += relCount;

    return result;  
}
