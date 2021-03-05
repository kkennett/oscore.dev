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

K2STAT
K2OS_Thread_GetLastStatus(
    void
)
{
    UINT32 *pTls;
    
    pTls = (UINT32 *)(K2OS_UVA_TLSAREA_BASE + (K2OS_SYSCALL(K2OS_SYSCALL_ID_GET_THREAD_IX, 0) * K2_VA32_MEMPAGE_BYTES));
    
    return (K2STAT)(pTls[0]);
}

K2STAT
K2OS_Thread_SetLastStatus(
    K2STAT aStatus
)
{
    UINT32 *pTls;
    UINT32  result;

    pTls = (UINT32 *)(K2OS_UVA_TLSAREA_BASE + (K2OS_SYSCALL(K2OS_SYSCALL_ID_GET_THREAD_IX, 0) * K2_VA32_MEMPAGE_BYTES));
    
    result = (K2STAT)(pTls[0]);
    pTls[0] = (UINT32)aStatus;

    return result;
}