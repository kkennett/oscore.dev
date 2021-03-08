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

static UINT64 volatile sgSlotBitfield = 0;  

K2STAT 
K2OS_Tls_AllocSlot(
    UINT32 *apRetNewIndex
)
{
    UINT64 oldVal;
    UINT64 nextBit;
    UINT64 alt;

    do
    {
        oldVal = sgSlotBitfield;
        K2_CpuReadBarrier();
        if (0xFFFFFFFFFFFFFFFFull == oldVal)
            return K2STAT_ERROR_OUT_OF_RESOURCES;

        //
        // find first set bit
        //
        nextBit = ~oldVal;
        alt = nextBit - 1;
        nextBit = oldVal | ((nextBit | alt) ^ alt);

    } while (oldVal != K2ATOMIC_CompareExchange64(&sgSlotBitfield, nextBit, oldVal));

    K2BIT_GetLowestPos64(alt, apRetNewIndex);

    return K2STAT_NO_ERROR;
}

K2STAT
K2OS_Tls_FreeSlot(
    UINT32 aSlotIndex
)
{
    UINT64 oldVal;
    UINT64 newVal;

    if (K2OS_NUM_TLS_SLOTS <= aSlotIndex)
        return K2STAT_ERROR_OUT_OF_BOUNDS;

    do
    {
        oldVal = sgSlotBitfield;
        K2_CpuReadBarrier();
        if (0 == (oldVal & (1ull << aSlotIndex)))
            return K2STAT_ERROR_NOT_IN_USE;
        newVal = oldVal & ~(1ull << aSlotIndex);
    } while (oldVal != K2ATOMIC_CompareExchange64(&sgSlotBitfield, newVal, oldVal));

    return K2STAT_NO_ERROR;
}

K2STAT
K2OS_Tls_SetValue(
    UINT32 aSlotIndex,
    UINT32 aValue
)
{
    K2OS_USER_THREAD_PAGE * pThreadPage;

    if (K2OS_NUM_TLS_SLOTS <= aSlotIndex)
        return K2STAT_ERROR_OUT_OF_BOUNDS;

    if (0 == (sgSlotBitfield & (1ull << aSlotIndex)))
        return K2STAT_ERROR_NOT_IN_USE;

    pThreadPage = (K2OS_USER_THREAD_PAGE *)(K2OS_UVA_TLSAREA_BASE + (CRT_GET_CURRENT_THREAD_INDEX * K2_VA32_MEMPAGE_BYTES));

    pThreadPage->mTlsValue[aSlotIndex] = aValue;

    return K2STAT_NO_ERROR;
}

K2STAT
K2OS_Tls_GetValue(
    UINT32 aSlotIndex,
    UINT32 *apRetValue
)
{
    K2OS_USER_THREAD_PAGE * pThreadPage;

    if (K2OS_NUM_TLS_SLOTS <= aSlotIndex)
        return K2STAT_ERROR_OUT_OF_BOUNDS;

    if (0 == (sgSlotBitfield & (1ull << aSlotIndex)))
        return K2STAT_ERROR_NOT_IN_USE;

    pThreadPage = (K2OS_USER_THREAD_PAGE *)(K2OS_UVA_TLSAREA_BASE + (CRT_GET_CURRENT_THREAD_INDEX * K2_VA32_MEMPAGE_BYTES));

    *apRetValue = pThreadPage->mTlsValue[aSlotIndex];

    return K2STAT_NO_ERROR;
}

