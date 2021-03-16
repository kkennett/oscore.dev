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
#include "crt.h"

typedef struct _IntCritSec IntCritSec;
struct _IntCritSec
{
    UINT32      mLockOwner;
    K2OS_TOKEN  mTokNotify;
};

BOOL 
K2OS_CritSec_Init(
    K2OS_CRITSEC *apSec
)
{
    IntCritSec *pSec;
    pSec = (IntCritSec *)((((UINT32)apSec) + (K2OS_CACHELINE_BYTES - 1)) & (~(K2OS_CACHELINE_BYTES - 1)));

    pSec->mLockOwner = 0;
    pSec->mTokNotify = K2OS_Notify_Create(0);
    if (NULL == pSec->mTokNotify)
        return FALSE;

    return TRUE;
}

BOOL   
K2OS_CritSec_TryEnter(
    K2OS_CRITSEC *apSec
)
{
    return FALSE;
}

BOOL 
K2OS_CritSec_Enter(
    K2OS_CRITSEC *apSec
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

BOOL 
K2OS_CritSec_Leave(
    K2OS_CRITSEC *apSec
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

BOOL 
K2OS_CritSec_Done(
    K2OS_CRITSEC *apSec
)
{
    K2STAT      stat;
    IntCritSec *pSec;

    pSec = (IntCritSec *)((((UINT32)apSec) + (K2OS_CACHELINE_BYTES - 1)) & (~(K2OS_CACHELINE_BYTES - 1)));

    stat = K2OS_Token_Destroy(pSec->mTokNotify);
    if (!K2STAT_IS_ERROR(stat))
    {
        K2MEM_Zero(apSec, sizeof(K2OS_CRITSEC));
    }
    
    return stat;
}

