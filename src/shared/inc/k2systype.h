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
#ifndef __K2SYSTYPE_H
#define __K2SYSTYPE_H

#include <spec/dlx.h>

//
//------------------------------------------------------------------------
//

#ifdef __cplusplus
extern "C" {
#endif

//
// Raw Opaque Token
//
typedef void * K2_RAW_TOKEN;


//
// Assert
//
typedef 
void
(K2_CALLCONV_REGS *K2_pf_ASSERT)(
    char const *    apFile,
    int             aLineNum,
    char const *    apCondition
    );

extern K2_pf_ASSERT     K2_Assert;
#ifdef K2_DEBUG
#define K2_ASSERT(x)    if (!(x)) K2_Assert(__FILE__, __LINE__, K2_STRINGIZE(x))
#else
#define K2_ASSERT(x)    
#endif


//
// Raw Atomics
//
UINT32
K2_CALLCONV_REGS
K2ATOMIC_CompareExchange32(
    UINT32 volatile *apMem,
    UINT32 aNewVal,
    UINT32 aComparand
);

UINT64
K2_CALLCONV_REGS
K2ATOMIC_CompareExchange64(
    UINT64 volatile *apMem,
    UINT64 aNewVal,
    UINT64 aComparand
);



//
// Exception Trap
//

K2_PACKED_PUSH
typedef struct _K2_EXCEPTION_TRAP K2_EXCEPTION_TRAP;
struct _K2_EXCEPTION_TRAP
{
    K2_EXCEPTION_TRAP * mpNextTrap;
    K2STAT              mResult;
    K2_TRAP_CONTEXT     SavedContext;
} K2_PACKED_ATTRIB;
K2_PACKED_POP

typedef 
BOOL       
(K2_CALLCONV_REGS *K2_pf_EXTRAP_MOUNT)(
    K2_EXCEPTION_TRAP *apTrap
    );

typedef 
K2STAT 
(K2_CALLCONV_REGS *K2_pf_EXTRAP_DISMOUNT)(
    K2_EXCEPTION_TRAP *apTrap
    );

typedef 
void       
(K2_CALLCONV_REGS *K2_pf_RAISE_EXCEPTION)(
    K2STAT aExceptionCode
    );

extern K2_pf_EXTRAP_MOUNT      K2_ExTrap_Mount;
extern K2_pf_EXTRAP_DISMOUNT   K2_ExTrap_Dismount;
extern K2_pf_RAISE_EXCEPTION   K2_RaiseException;

#define K2_EXTRAP(pTrap, Func)  (K2_ExTrap_Mount(pTrap) ? \
        (pTrap)->mResult : \
        ((pTrap)->mResult = (Func), K2_ExTrap_Dismount(pTrap)))

#ifdef __cplusplus
}
#endif

//
//------------------------------------------------------------------------
//

#endif  // __K2SYSTYPE_H
