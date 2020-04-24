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

void KernIntr_QueueCpuCoreEvent(K2OSKERN_CPUCORE * apThisCore, K2OSKERN_CPUCORE_EVENT volatile * apCoreEvent)
{
    K2OSKERN_CPUCORE_EVENT *    pNext;
    UINT32                      old;

    do {
        pNext = apThisCore->mpPendingEventListHead;
        apCoreEvent->mpNext = pNext;
        old = K2ATOMIC_CompareExchange(
            (UINT32 volatile *)&apThisCore->mpPendingEventListHead,
            (UINT32)apCoreEvent,
            (UINT32)pNext);
    } while (old != (UINT32)pNext);
}

K2STAT
K2OSKERN_InstallIntrHandler(
    K2OSKERN_IRQ_CONFIG const * apConfig,
    K2OSKERN_pf_IntrHandler     aHandler,
    void *                      apContext,
    K2OS_TOKEN *                apRetTokIntr
)
{
    K2OSKERN_OBJ_INTR *     pIntr;
    BOOL                    disp;
    K2STAT                  stat;
    K2TREE_NODE *           pTreeNode;
    K2OS_TOKEN              tokIntr;
    K2OSKERN_OBJ_HEADER *   pObjHdr;

    K2_ASSERT(apConfig != NULL);
    K2_ASSERT(aHandler != NULL);
    K2_ASSERT(apRetTokIntr != NULL);

    pIntr = (K2OSKERN_OBJ_INTR *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_INTR));
    if (pIntr == NULL)
    {
        return K2STAT_ERROR_OUT_OF_MEMORY;
    }

    tokIntr = NULL;

    K2MEM_Zero(pIntr, sizeof(K2OSKERN_OBJ_INTR));
    pIntr->Hdr.mObjType = K2OS_Obj_Interrupt;
    pIntr->Hdr.mRefCount = 1;
    K2LIST_Init(&pIntr->Hdr.WaitEntryPrioList);
    pIntr->mIsSignalled = FALSE;
    pIntr->mfHandler = aHandler;
    pIntr->mpHandlerContext = apContext;
    pIntr->IntrTreeNode.mUserVal = apConfig->mSourceIrq;
    pIntr->IrqConfig = *apConfig;

    disp = K2OSKERN_SeqIntrLock(&gData.IntrTreeSeqLock);

    pTreeNode = K2TREE_Find(&gData.IntrTree, pIntr->IntrTreeNode.mUserVal);
    if (pTreeNode != NULL)
    {
        stat = K2STAT_ERROR_ALREADY_EXISTS;
    }
    else
    {
        KernArch_InstallDevIntrHandler(pIntr);
        K2TREE_Insert(&gData.IntrTree, pIntr->IntrTreeNode.mUserVal, &pIntr->IntrTreeNode);
        stat = K2STAT_NO_ERROR;
    }

    K2OSKERN_SeqIntrUnlock(&gData.IntrTreeSeqLock, disp);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        stat = K2OS_HeapFree(pIntr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
    }
    else
    {
        pObjHdr = &pIntr->Hdr;
        stat = KernTok_CreateNoAddRef(1, &pObjHdr, &tokIntr);
        K2_ASSERT(tokIntr != NULL);

        //
        // interrupts can occur immediately, so write to the token return value
        // before we unmask interrupts

        *apRetTokIntr = tokIntr;
        K2_CpuWriteBarrier();
        KernArch_SetDevIntrMask(pIntr, FALSE);
    }

    return stat;
}

K2STAT
K2OSKERN_SetIntrMask(
    K2OS_TOKEN  aTokIntr,
    BOOL        aMask
)
{
    K2STAT              stat;
    K2OSKERN_OBJ_INTR * pIntr;

    stat = KernTok_TranslateToAddRefObjs(1, &aTokIntr, (K2OSKERN_OBJ_HEADER **)&pIntr);
    if (K2STAT_IS_ERROR(stat))
        return stat;

    if (pIntr->Hdr.mObjType == K2OS_Obj_Interrupt)
    {
        KernArch_SetDevIntrMask(pIntr, aMask);
    }

    KernObj_Release(&pIntr->Hdr);

    return stat;


}
