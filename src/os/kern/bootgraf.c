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

void sDrawBootGraphics(UINT32 aFrameBuffer)
{
    K2MEM_Set((void *)aFrameBuffer, 0x77, 1024 * 4 * 16);
}

static K2OSKERN_OBJ_SEGMENT * sgpSeg_BootGraf = NULL;

static void sInit_MemReady(void)
{
    K2STAT  stat;
    UINT32  work;
    UINT32  base;
    UINT32  pageCount;

    if ((0 == gData.mpShared->LoadInfo.BootGraf.mFrameBufferBytes) ||
        (0 == gData.mpShared->LoadInfo.BootGraf.mFrameBufferPhys))
        return;

//    K2OSKERN_Debug("Graphics @ %08X for %08X\n", gData.mpShared->LoadInfo.BootGraf.mFrameBufferPhys, gData.mpShared->LoadInfo.BootGraf.mFrameBufferBytes);

    pageCount = gData.mpShared->LoadInfo.BootGraf.mFrameBufferBytes;
    base = gData.mpShared->LoadInfo.BootGraf.mFrameBufferPhys;
    work = base & K2_VA32_MEMPAGE_OFFSET_MASK;
    if (0 != work)
    {
        pageCount += work;
        base &= K2_VA32_PAGEFRAME_MASK;
    }
    pageCount = (pageCount + (K2_VA32_MEMPAGE_BYTES - 1)) / K2_VA32_MEMPAGE_BYTES;

    //
    // map boot graphics buffer, display splash
    //
    stat = KernMem_MapContigPhys(
        base,
        pageCount,
        K2OS_MAPTYPE_KERN_WRITETHRU_CACHED,
        &sgpSeg_BootGraf
    );
    if (K2STAT_IS_ERROR(stat))
    {
        K2OSKERN_Debug("Could not map boot graphics segment\n");
        sgpSeg_BootGraf = NULL;
        return;
    }

    sDrawBootGraphics(sgpSeg_BootGraf->ProcSegTreeNode.mUserVal);
}

static void sInit_AtRunExec(void)
{
    if (NULL == sgpSeg_BootGraf)
        return;

    //
    // unmap boot graphics buffer. driver will take over
    //
    K2OSKERN_ReleaseObject(&sgpSeg_BootGraf->Hdr);
    sgpSeg_BootGraf = NULL;
}

void KernInit_BootGraf(void)
{
    switch (gData.mKernInitStage)
    {
    case KernInitStage_MemReady:
        sInit_MemReady();
        break;
    case KernInitStage_AtRunExec:
        sInit_AtRunExec();
        break;
    default:
        break;
    }
}


