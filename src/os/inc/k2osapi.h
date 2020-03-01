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
#ifndef __K2OSAPI_H
#define __K2OSAPI_H

#include "k2osbase.h"

//
//------------------------------------------------------------------------
//

typedef UINT32  (K2_CALLCONV_CALLERCLEANS *pf_K2OS_DebugPrint)(char const *apString);

//
//------------------------------------------------------------------------
//

#define K2OS_CRITSEC_BYTES  K2OS_CACHELINE_ALIGNED_BUFFER_MAX

typedef struct _K2OS_CRITSEC K2OS_CRITSEC;
struct K2_ALIGN_ATTRIB(K2OS_MAX_CACHELINE_BYTES) _K2OS_CRITSEC
{
    UINT8   mOpaque[K2OS_CRITSEC_BYTES];
};

typedef BOOL(K2_CALLCONV_CALLERCLEANS *pf_K2OS_CritSecInit)(K2OS_CRITSEC *apSec);
typedef BOOL(K2_CALLCONV_CALLERCLEANS *pf_K2OS_CritSecTryEnter)(K2OS_CRITSEC *apSec);
typedef BOOL(K2_CALLCONV_CALLERCLEANS *pf_K2OS_CritSecEnter)(K2OS_CRITSEC *apSec);
typedef BOOL(K2_CALLCONV_CALLERCLEANS *pf_K2OS_CritSecLeave)(K2OS_CRITSEC *apSec);
typedef BOOL(K2_CALLCONV_CALLERCLEANS *pf_K2OS_CritSecDone)(K2OS_CRITSEC *apSec);

//
//------------------------------------------------------------------------
//

typedef struct _K2OS_HEAP_STATE K2OS_HEAP_STATE;
struct _K2OS_HEAP_STATE
{
    UINT32  mAllocCount;
    UINT32  mTotalAlloc;
    UINT32  mTotalOverhead;
    UINT32  mLargestFree;
};

typedef BOOL(K2_CALLCONV_CALLERCLEANS *pf_K2OS_HeapAlloc)(UINT32 aByteCount, void **appRetPtr);
typedef BOOL(K2_CALLCONV_CALLERCLEANS *pf_K2OS_HeapFree)(void *aPtr);
typedef BOOL(K2_CALLCONV_CALLERCLEANS *pf_K2OS_HeapGetState)(K2OS_HEAP_STATE *apRetState);

//
//------------------------------------------------------------------------
//

typedef struct _K2OSAPI_V0 K2OSAPI_V0;
struct _K2OSAPI_V0
{
    UINT32  mVersion;

    // Version 0 -------------------------------------------

    pf_K2OS_DebugPrint                  DebugPrint;

    pf_K2OS_CritSecInit                 CritSecInit;
    pf_K2OS_CritSecTryEnter             CritSecTryEnter;
    pf_K2OS_CritSecEnter                CritSecEnter;
    pf_K2OS_CritSecLeave                CritSecLeave;
    pf_K2OS_CritSecDone                 CritSecDone;

    pf_K2OS_HeapAlloc                   HeapAlloc;
    pf_K2OS_HeapFree                    HeapFree;
    pf_K2OS_HeapGetState                HeapGetState;

};

#define K2OSAPI                     K2OSAPI_V0
#define K2OSAPI_VERSION_CURRENT     0
//
//------------------------------------------------------------------------
//

#endif // __K2OSAPI_H