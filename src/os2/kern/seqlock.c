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
KernSeqLock_Init(
    K2OSKERN_SEQLOCK *  apLock
)
{
    apLock->mSeqIn = 0;
    apLock->mSeqOut = 0;
    K2_CpuWriteBarrier();
}

void
K2_CALLCONV_REGS
KernSeqLock_Lock(
    K2OSKERN_SEQLOCK *  apLock
)
{
    UINT32  mySeq;

    if (1 == gData.LoadInfo.mCpuCoreCount)
        return;

    do
    {
        mySeq = apLock->mSeqIn;
    } while (mySeq == K2ATOMIC_CompareExchange(&apLock->mSeqIn, mySeq + 1, mySeq));

    do {
        if (apLock->mSeqOut == mySeq)
            break;
        KernHal_MicroStall(10);
    } while (1);
}

void
K2_CALLCONV_REGS
KernSeqLock_Unlock(
    K2OSKERN_SEQLOCK *  apLock
)
{
    if (1 == gData.LoadInfo.mCpuCoreCount)
        return;

    apLock->mSeqOut = apLock->mSeqOut + 1;
    K2_CpuWriteBarrier();
}