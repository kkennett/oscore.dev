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

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesAlloc(UINT32 *apAddr, UINT32 aPageCount, UINT32 aVirtAllocFlags, UINT32 aPageAttrFlags)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesCommit(UINT32 aPagesAddr, UINT32 aPageCount, UINT32 aPageAttrFlags)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesDecommit(UINT32 aPagesAddr, UINT32 aPageCount)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesSetAttr(UINT32 aPagesAddr, UINT32 aPageCount, UINT32 aPageAttrFlags) 
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesGetAttr(UINT32 aPagesAddr, UINT32 *apRetPageCount, UINT32 *apRetPagesAttrFlags)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesSetLock(UINT32 aPagesAddr, UINT32 aPageCount, BOOL aSetLocked)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesFree(UINT32 aPagesAllocAddr)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return FALSE;
}

