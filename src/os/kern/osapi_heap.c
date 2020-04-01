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

void * K2_CALLCONV_CALLERCLEANS K2OS_HeapAlloc(UINT32 aByteCount)
{
    K2STAT  stat;
    void *  ptr;

    if (aByteCount == 0)
        return NULL;

    if (gData.mKernInitStage < KernInitStage_Threaded)
        stat = K2STAT_ERROR_NOT_READY;
    else
    {
        K2OSKERN_Debug("+RamHeap_Alloc(%d)\n", aByteCount);
        ptr = NULL;
        stat = K2OS_RAMHEAP_Alloc(&gData.RamHeap, aByteCount, TRUE, &ptr);
        K2OSKERN_Debug("-RamHeap_Alloc(%d, %08X)\n", aByteCount, stat);
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        ptr = NULL;
    }
    else
    {
        K2_ASSERT(((UINT32)ptr) >= K2OS_KVA_KERN_BASE);
    }

    return ptr;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_HeapFree(void *aPtr)
{
    K2STAT  stat;
    BOOL    result;

    if (gData.mKernInitStage < KernInitStage_Threaded)
        stat = K2STAT_ERROR_NOT_READY;
    else
    {
        K2_ASSERT(((UINT32)aPtr) >= K2OS_KVA_KERN_BASE);
        stat = K2OS_RAMHEAP_Free(&gData.RamHeap, aPtr);
    }
    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);

    return result;
}

BOOL K2_CALLCONV_CALLERCLEANS K2OS_HeapGetState(K2OS_HEAP_STATE *apRetState)
{
    K2STAT              stat;
    BOOL                result;
    K2OS_RAMHEAP_STATE  heapState;
    UINT32              largestFree;

    if (gData.mKernInitStage < KernInitStage_Threaded)
        stat = K2STAT_ERROR_NOT_READY;
    else
    {
        K2_ASSERT(((UINT32)apRetState) >= K2OS_KVA_KERN_BASE);
        stat = K2OS_RAMHEAP_GetState(&gData.RamHeap, &heapState, &largestFree);
    }

    result = (!K2STAT_IS_ERROR(stat));
    if (!result)
        K2OS_ThreadSetStatus(stat);
    else
    {
        apRetState->mAllocCount = heapState.mAllocCount;
        apRetState->mTotalAlloc = heapState.mTotalAlloc;
        apRetState->mTotalOverhead = heapState.mTotalOverhead;
        apRetState->mLargestFree = largestFree;
    }

    return result;
}

