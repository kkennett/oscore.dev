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

void KernSegment_Dump(K2OSKERN_OBJ_SEGMENT *apSeg)
{
    K2_ASSERT(apSeg->Hdr.mObjType == K2OS_Obj_Segment);
    K2OSKERN_Debug("r%08X f%08X a%02d @%08X z%08X\n",
        apSeg->Hdr.mRefCount,
        apSeg->Hdr.mObjFlags,
        apSeg->mSegAndMemPageAttr,
        apSeg->SegTreeNode.mUserVal,
        apSeg->mPagesBytes);
    switch (apSeg->mSegAndMemPageAttr & K2OS_SEG_ATTR_TYPE_MASK)
    {
    case K2OS_SEG_ATTR_TYPE_DLX_PART:
        K2OSKERN_Debug("  DlxObj @%08X\n", apSeg->Info.DlxPart.mpDlxObj);
        K2OSKERN_Debug("   SegIx  %d\n", apSeg->Info.DlxPart.mSegmentIndex);
        K2OSKERN_Debug("  Actual  %08X\n", apSeg->Info.DlxPart.DlxSegmentInfo.mMemActualBytes);
        break;

    case K2OS_SEG_ATTR_TYPE_PROCESS:
        K2OSKERN_Debug("  ProcObj @%08X\n", apSeg->Info.Process.mpProc);
        break;

    case K2OS_SEG_ATTR_TYPE_THREAD:
        K2OSKERN_Debug("  ThreadObj @%08X\n", apSeg->Info.ThreadStack.mpThread);
        break;

    default:
        K2_ASSERT(0);
        break;
    };
}

