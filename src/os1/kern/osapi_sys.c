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

UINT32  K2_CALLCONV_CALLERCLEANS K2OS_SysGetInfo(K2OS_SYSINFO *apRetInfo)
{
    K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
    return 0;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_SysGetProperty(UINT32 aPropertyId, void *apRetValue, UINT32 aValueBufferBytes)
{
    switch (aPropertyId)
    {
    case K2OS_SYSPROP_ID_KERN_FWTABLES_PHYS_ADDR:
        K2_ASSERT(aValueBufferBytes >= sizeof(UINT32));
        *((UINT32 *)apRetValue) = gData.mpShared->LoadInfo.mFwTabPagesPhys;
        return TRUE;

    case K2OS_SYSPROP_ID_KERN_FWTABLES_VIRT_ADDR:
        K2_ASSERT(aValueBufferBytes >= sizeof(UINT32));
        *((UINT32 *)apRetValue) = gData.mpShared->LoadInfo.mFwTabPagesVirt;
        return TRUE;

    case K2OS_SYSPROP_ID_KERN_FWTABLES_BYTES:
        K2_ASSERT(aValueBufferBytes >= sizeof(UINT32));
        *((UINT32 *)apRetValue) = gData.mpShared->LoadInfo.mFwTabPageCount * K2_VA32_MEMPAGE_BYTES;
        return TRUE;

    case K2OS_SYSPROP_ID_KERN_FWFACS_PHYS_ADDR:
        K2_ASSERT(aValueBufferBytes >= sizeof(UINT32));
        *((UINT32 *)apRetValue) = gData.mpShared->LoadInfo.mFwFacsPhys;
        return TRUE;

    case K2OS_SYSPROP_ID_KERN_XFWFACS_PHYS_ADDR:
        K2_ASSERT(aValueBufferBytes >= sizeof(UINT32));
        *((UINT32 *)apRetValue) = gData.mpShared->LoadInfo.mFwXFacsPhys;
        return TRUE;

    default:
        K2OS_ThreadSetStatus(K2STAT_ERROR_NOT_IMPL);
        break;
    }

    return FALSE;
}