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

void
K2_CALLCONV_REGS
K2OSKERN_SeqIntrInit(
    K2OSKERN_SEQLOCK *  apLock
)
{
    apLock->mSeqIn = 0;
    apLock->mSeqOut = 0;
    K2_CpuWriteBarrier();
}

BOOL
K2_CALLCONV_REGS
K2OSKERN_SeqIntrLock(
    K2OSKERN_SEQLOCK *  apLock
)
{
    BOOL    enabled;
    UINT32  mySeq;

    enabled = K2OSKERN_SetIntr(FALSE);

    if (gData.mCpuCount > 1)
    {
        do {
            mySeq = apLock->mSeqIn;
            if (mySeq == K2ATOMIC_CompareExchange(&apLock->mSeqIn, mySeq + 1, mySeq))
                break;
            if (enabled)
            {
                K2OSKERN_SetIntr(TRUE);
                K2OSKERN_MicroStall(10);
                K2OSKERN_SetIntr(FALSE);
            }
        } while (1);

        do {
            if (apLock->mSeqOut == mySeq)
                break;
            K2OSKERN_MicroStall(10);
        } while (1);
    }

    return enabled;
}

void
K2_CALLCONV_REGS
K2OSKERN_SeqIntrUnlock(
    K2OSKERN_SEQLOCK *  apLock,
    BOOL                aDisp
)
{
    if (gData.mCpuCount > 1)
    {
        apLock->mSeqOut = apLock->mSeqOut + 1;
        K2_CpuWriteBarrier();
    }
    if (aDisp)
        K2OSKERN_SetIntr(TRUE);
}