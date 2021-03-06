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

void
KernProc_Init(
    void
)
{
    BOOL        ptePresent;
    UINT32      pte;
    UINT32 *    pThreadPtrs;

    K2OSKERN_SeqInit(&gData.ProcListSeqLock);

    K2LIST_Init(&gData.ProcList);

    gpProc1->Hdr.mObjType = KernObj_Process;
    gpProc1->Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT;
    gpProc1->Hdr.mRefCount = 0;
    gpProc1->Hdr.mfCleanup = NULL;
    //
    // this stuff set up in kernel main at the very beginning
    //
    //gpProc1->mId = 1;
    //gpProc1->mTransTableKVA = K2OS_KVA_TRANSTAB_BASE;
    //gpProc1->mTransTableRegVal = gData.LoadInfo.mTransBasePhys;
    //gpProc1->mVirtMapKVA = K2OS_KVA_KERNVAMAP_BASE;

    KernObj_Add(&gpProc1->Hdr);

    K2LIST_AddAtHead(&gData.ProcList, &gpProc1->ProcListLink);

    //
    // map proc 1 tls pagetable.  find the physaddr for the process page and use it to set up to
    // map the first 4MB of proc 1's process area.  proc 1 gets to see everybody's TLS storage
    //
    KernArch_Translate(
        gpProc1,
        K2OS_KVA_PROC1_TLS_PAGETABLE,
        NULL,
        &ptePresent,
        &pte,
        NULL);

    K2_ASSERT(ptePresent);

    // tlspage always maps to 0-4MB
    KernArch_InstallPageTable(gpProc1, 0, pte & K2_VA32_PAGEFRAME_MASK, FALSE);

    // kernel also uses this pagetable so it can see all threads' TLS no 
    // matter what process is active on a cpu 
    KernArch_InstallPageTable(gpProc1, K2OS_KVA_THREAD_TLS_BASE, pte & K2_VA32_PAGEFRAME_MASK, FALSE);

    //
    // make sure threadptrs page is clear
    //
    K2MEM_Zero((void *)K2OS_KVA_THREADPTRS_BASE, K2_VA32_MEMPAGE_BYTES);

    //
    // init threadptrs list to a free list.  thread 0 is never used
    //
    pThreadPtrs = (UINT32 *)K2OS_KVA_THREADPTRS_BASE;
    pThreadPtrs[0] = 0; // never used, not valid
    for (pte = 1; pte < K2OS_MAX_THREAD_COUNT - 1; pte++)
        pThreadPtrs[pte] = pte + 1;
    pThreadPtrs[K2OS_MAX_THREAD_COUNT - 1] = 0;
    gData.mNextThreadSlotIx = 1;
    gData.mLastThreadSlotIx = K2OS_MAX_THREAD_COUNT;
}