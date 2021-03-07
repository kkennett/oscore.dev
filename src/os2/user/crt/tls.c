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

// slot 0 is thread last error slot
static UINT32 volatile sgSlotBitfield = 1;  

K2STAT 
K2OS_Tls_AllocSlot(
    UINT32 *apRetNewIndex
)
{
    UINT32 oldVal;
    UINT32 nextBit;
    UINT32 alt;

    do
    {
        oldVal = sgSlotBitfield;
        K2_CpuReadBarrier();
        if (0xFFFFFFFF == oldVal)
            return K2STAT_ERROR_OUT_OF_RESOURCES;

        //
        // find first set bit
        //
        nextBit = ~oldVal;
        alt = nextBit - 1;
        nextBit = oldVal | ((nextBit | alt) ^ alt);

    } while (oldVal != K2ATOMIC_CompareExchange(&sgSlotBitfield, nextBit, oldVal));

    K2BIT_GetLowestPos32(alt, apRetNewIndex);

    return K2STAT_NO_ERROR;
}

K2STAT
K2OS_Tls_FreeSlot(
    UINT32 aSlotIndex
)
{
    UINT32 oldVal;
    UINT32 newVal;

    if (K2OS_MAX_NUM_TLS_SLOTS <= aSlotIndex)
        return K2STAT_ERROR_OUT_OF_BOUNDS;

    do
    {
        oldVal = sgSlotBitfield;
        K2_CpuReadBarrier();
        if (0 == (oldVal & (1 << aSlotIndex)))
            return K2STAT_ERROR_NOT_IN_USE;
        newVal = oldVal & ~(1 << aSlotIndex);
    } while (oldVal != K2ATOMIC_CompareExchange(&sgSlotBitfield, newVal, oldVal));

    return K2STAT_NO_ERROR;
}

K2STAT
K2OS_Tls_SetValue(
    UINT32 aSlotIndex,
    UINT32 aValue
)
{
    UINT32  thisThreadIndex;
    UINT32 *pTls;

    if (K2OS_MAX_NUM_TLS_SLOTS <= aSlotIndex)
        return K2STAT_ERROR_OUT_OF_BOUNDS;

    if (0 == (sgSlotBitfield & (1 << aSlotIndex)))
        return K2STAT_ERROR_NOT_IN_USE;

    thisThreadIndex = CRT_GET_CURRENT_THREAD_INDEX;

    pTls = (UINT32 *)(K2OS_UVA_TLSAREA_BASE + (thisThreadIndex * K2_VA32_MEMPAGE_BYTES));

    pTls[aSlotIndex] = aValue;

    return K2STAT_NO_ERROR;
}

K2STAT
K2OS_Tls_GetValue(
    UINT32 aSlotIndex,
    UINT32 *apRetValue
)
{
    UINT32  thisThreadIndex;
    UINT32 *pTls;

    if (K2OS_MAX_NUM_TLS_SLOTS <= aSlotIndex)
        return K2STAT_ERROR_OUT_OF_BOUNDS;

    if (0 == (sgSlotBitfield & (1 << aSlotIndex)))
        return K2STAT_ERROR_NOT_IN_USE;

    thisThreadIndex = CRT_GET_CURRENT_THREAD_INDEX;

    pTls = (UINT32 *)(K2OS_UVA_TLSAREA_BASE + (thisThreadIndex * K2_VA32_MEMPAGE_BYTES));

    *apRetValue = pTls[aSlotIndex];

    return K2STAT_NO_ERROR;
}

