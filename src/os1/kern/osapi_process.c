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

BOOL    K2_CALLCONV_CALLERCLEANS K2OS_ProcessCreate(K2OS_TOKEN aNameToken, K2OS_FILE_TOKEN aDlxFileToken, K2OS_PROCESSCREATE const *apProcessCreate, K2OS_PROCESS_CREATED *apRetCreated)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL    K2_CALLCONV_CALLERCLEANS K2OS_ProcessGetId(K2OS_TOKEN aProcessToken, UINT32 *apRetId)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

UINT32  K2_CALLCONV_CALLERCLEANS K2OS_ProcessGetOwnId(void)
{
    K2OSKERN_OBJ_THREAD *   pCurThread;

    pCurThread = K2OSKERN_CURRENT_THREAD;

    return pCurThread->mpProc->mId;
}

BOOL    K2_CALLCONV_CALLERCLEANS K2OS_ProcessGetImageInfo(K2OS_TOKEN aProcessToken, K2OS_IMAGEINFO *apRetImageInfo)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL    K2_CALLCONV_CALLERCLEANS K2OS_ProcessGetOwnImageInfo(K2OS_IMAGEINFO *apRetImageInfo)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ProcessAcquireByName(K2OS_TOKEN aNameToken)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return NULL;
}

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ProcessAcquireById(UINT32 aProcessId)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return NULL;
}

