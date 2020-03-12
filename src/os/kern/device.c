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

static
K2STAT
sMapDeviceEarly(
    UINT32      aPhysDeviceAddr,
    UINT32      aPageCount,
    UINT32 *    apRetVirtAddr
)
{

    return K2STAT_ERROR_NOT_IMPL;
}

static
K2STAT
sMapDeviceAfterHal(
    UINT32      aPhysDeviceAddr,
    UINT32      aPageCount,
    UINT32 *    apRetVirtAddr
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT
K2OSKERN_MapDevice(
    UINT32      aPhysDeviceAddr,
    UINT32      aPageCount,
    UINT32 *    apRetVirtAddr
)
{
    K2STAT stat;

    K2OSKERN_Debug("+K2OSKERN_MapDevice(%08X,%d)\n", aPhysDeviceAddr, aPageCount);

    if (gData.mKernInitStage < KernInitStage_After_Hal)
        stat = sMapDeviceEarly(aPhysDeviceAddr, aPageCount, apRetVirtAddr);
    else
        stat = sMapDeviceAfterHal(aPhysDeviceAddr, aPageCount, apRetVirtAddr);

    K2OSKERN_Debug("-K2OSKERN_MapDevice(%08X,%d)\n", aPhysDeviceAddr, aPageCount);

    return stat;
}

static
K2STAT
sUnmapDeviceEarly(
    UINT32  aVirtDeviceAddr
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

static
K2STAT
sUnmapDeviceAfterHal(
    UINT32  aVirtDeviceAddr
)
{
    return K2STAT_ERROR_NOT_IMPL;
}

K2STAT
K2OSKERN_UnmapDevice(
    UINT32  aVirtDeviceAddr
)
{
    K2STAT stat;

    K2OSKERN_Debug("+K2OSKERN_UnmapDevice(%08X)\n", aVirtDeviceAddr);

    if (gData.mKernInitStage < KernInitStage_After_Hal)
        stat = sUnmapDeviceEarly(aVirtDeviceAddr);
    else
        stat = sUnmapDeviceAfterHal(aVirtDeviceAddr);

    K2OSKERN_Debug("-K2OSKERN_UnmapDevice(%08X)\n", aVirtDeviceAddr);

    return stat;
}

