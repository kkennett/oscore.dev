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

#include "ik2osexec.h"

static K2OS_CRITSEC         sgDlx_Sec;
static K2ROFS_DIR const *   sgpBuiltinRoot;

K2STAT Dlx_CritSec(BOOL aEnter)
{
    BOOL ok;
    if (aEnter)
        ok = K2OS_CritSecEnter(&sgDlx_Sec);
    else
        ok = K2OS_CritSecLeave(&sgDlx_Sec);
    if (!ok)
        return K2OS_ThreadGetStatus();
    return K2STAT_OK;
}

K2STAT Dlx_Open(char const * apDlxName, UINT32 aDlxNameLen, K2DLXSUPP_OPENRESULT *apRetResult)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT Dlx_ReadSectors(K2DLXSUPP_HOST_FILE aHostFile, void *apBuffer, UINT32 aSectorCount)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT Dlx_Prepare(K2DLXSUPP_HOST_FILE aHostFile, DLX_INFO *apInfo, UINT32 aInfoSize, BOOL aKeepSymbols, K2DLXSUPP_SEGALLOC *apRetAlloc)
{
    return K2STAT_ERROR_NOT_IMPL;
}

BOOL   Dlx_PreCallback(K2DLXSUPP_HOST_FILE aHostFile, BOOL aIsLoad)
{
    return FALSE;
}

K2STAT Dlx_PostCallback(K2DLXSUPP_HOST_FILE aHostFile, K2STAT aUserStatus)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT Dlx_Finalize(K2DLXSUPP_HOST_FILE aHostFile, K2DLXSUPP_SEGALLOC *apUpdateAlloc)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT Dlx_Purge(K2DLXSUPP_HOST_FILE aHostFile)
{
    return K2STAT_ERROR_NOT_IMPL;
}

void
Dlx_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
)
{
    BOOL ok;

    apInitInfo->mfDlxCritSec = Dlx_CritSec;
    apInitInfo->mfDlxOpen = Dlx_Open;
    apInitInfo->mfDlxReadSectors = Dlx_ReadSectors;
    apInitInfo->mfDlxPrepare = Dlx_Prepare;
    apInitInfo->mfDlxPreCallback = Dlx_PreCallback;
    apInitInfo->mfDlxPostCallback = Dlx_PostCallback;
    apInitInfo->mfDlxFinalize = Dlx_Finalize;
    apInitInfo->mfDlxPurge = Dlx_Purge;

    ok = K2OS_CritSecInit(&sgDlx_Sec);
    K2_ASSERT(ok);

    sgpBuiltinRoot = K2ROFS_ROOTDIR(apInitInfo->mpBuiltinRofs);
    K2_ASSERT(sgpBuiltinRoot != NULL);



}
