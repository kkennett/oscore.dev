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

static UINT8            sgPageBuf[3 * K2_VA32_MEMPAGE_BYTES];
static UINT8 *          sgpLoaderPage;
static K2DLXSUPP_HOST   sgDlxHost;
static K2ROFS const *   sgpROFS;


K2STAT 
CrtDlx_CritSec(
    BOOL aEnter
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT 
CrtDlx_AcqAlreadyLoaded(
    void *              apAcqContext, 
    K2DLXSUPP_HOST_FILE aHostFile
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT 
CrtDlx_Open(
    void *                  apAcqContext, 
    char const *            apFileSpec, 
    char const *            apNamePart, 
    UINT32                  aNamePartLen, 
    K2DLXSUPP_OPENRESULT *  apRetResult
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT 
CrtDlx_ReadSectors(
    void *              apAcqContext, 
    K2DLXSUPP_HOST_FILE aHostFile, 
    void *              apBuffer, 
    UINT32              aSectorCount
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT 
CrtDlx_Prepare(
    void *              apAcqContext, 
    K2DLXSUPP_HOST_FILE aHostFile, 
    DLX_INFO *          apInfo, 
    UINT32              aInfoSize, 
    BOOL                aKeepSymbols, 
    K2DLXSUPP_SEGALLOC *apRetAlloc
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

BOOL 
CrtDlx_PreCallback(
    void *              apAcqContext, 
    K2DLXSUPP_HOST_FILE aHostFile, 
    BOOL                aIsLoad, 
    DLX *               apDlx
)
{
    return FALSE;
}

K2STAT 
CrtDlx_PostCallback(
    void *              apAcqContext, 
    K2DLXSUPP_HOST_FILE aHostFile, 
    K2STAT              aUserStatus, 
    DLX *               apDlx
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT 
CrtDlx_Finalize(
    void *              apAcqContext, 
    K2DLXSUPP_HOST_FILE aHostFile, 
    K2DLXSUPP_SEGALLOC *apUpdateAlloc
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT 
CrtDlx_RefChange(
    K2DLXSUPP_HOST_FILE aHostFile, 
    DLX *               apDlx, 
    INT32               aRefChange
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT 
CrtDlx_Purge(
    K2DLXSUPP_HOST_FILE aHostFile
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT 
CrtDlx_ErrorPoint(
    char const *    apFile, 
    int             aLine, 
    K2STAT          aStatus
)
{
    return aStatus;
}

void 
CrtDlx_Init(
    K2ROFS const * apROFS
)
{
    K2STAT              stat;
    K2DLXSUPP_PRELOAD   preloadSelf;
    K2ROFS_FILE const * pFile;

    sgpROFS = apROFS;

    pFile = K2ROFSHELP_SubFile(sgpROFS, K2ROFS_ROOTDIR(sgpROFS), "k2oscrt.dlx");
    K2_ASSERT(NULL != pFile);

    sgpLoaderPage = (UINT8 *)((((UINT32)(&sgPageBuf[0])) + (K2_VA32_MEMPAGE_BYTES - 1)) & K2_VA32_PAGEFRAME_MASK);

    preloadSelf.mpDlxPage = sgpLoaderPage + K2_VA32_MEMPAGE_BYTES;
    preloadSelf.mpDlxFileData = K2ROFS_FILEDATA(sgpROFS, pFile);
    preloadSelf.mDlxFileSizeBytes = pFile->mSizeBytes;

    K2MEM_Zero(&sgDlxHost, sizeof(sgDlxHost));
    sgDlxHost.mHostSizeBytes = sizeof(sgDlxHost);
    sgDlxHost.CritSec = CrtDlx_CritSec;
    sgDlxHost.AcqAlreadyLoaded = CrtDlx_AcqAlreadyLoaded;
    sgDlxHost.Open = CrtDlx_Open;
    sgDlxHost.ReadSectors = CrtDlx_ReadSectors;
    sgDlxHost.Prepare = CrtDlx_Prepare;
    sgDlxHost.PreCallback = CrtDlx_PreCallback;
    sgDlxHost.PostCallback = CrtDlx_PostCallback;
    sgDlxHost.Finalize = CrtDlx_Finalize;
    sgDlxHost.RefChange = CrtDlx_RefChange;
    sgDlxHost.Purge = CrtDlx_Purge;
    sgDlxHost.ErrorPoint = CrtDlx_ErrorPoint;

    stat = K2DLXSUPP_Init(sgpLoaderPage, &sgDlxHost, TRUE, FALSE, &preloadSelf);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
}