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
    K2OS_USER_THREAD_PAGE * pThreadPage;

    pThreadPage = (K2OS_USER_THREAD_PAGE *)(K2OS_UVA_TLSAREA_BASE + (CRT_GET_CURRENT_THREAD_INDEX * K2_VA32_MEMPAGE_BYTES));

    return pThreadPage->mLastStatus;
}

K2STAT
K2OS_Thread_SetLastStatus(
    K2STAT aStatus
)
{
    K2OS_USER_THREAD_PAGE * pThreadPage;
    UINT32                  result;

    pThreadPage = (K2OS_USER_THREAD_PAGE *)(K2OS_UVA_TLSAREA_BASE + (CRT_GET_CURRENT_THREAD_INDEX * K2_VA32_MEMPAGE_BYTES));

    result = pThreadPage->mLastStatus;
    pThreadPage->mLastStatus = aStatus;

    return result;
}

BOOL
K2OS_Thread_WaitForNotify(
    K2OS_TOKEN aTokNotify
)
{
    return CrtKern_SysCall1(K2OS_SYSCALL_ID_WAIT_FOR_NOTIFY, (UINT32)aTokNotify);
}

BOOL   
K2OS_Thread_TestForNotify(
    K2OS_TOKEN aTokNotify
)
{
    return CrtKern_SysCall1(K2OS_SYSCALL_ID_TEST_NOTIFY, (UINT32)aTokNotify);
}
