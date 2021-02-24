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

static UINT32
sGetOnePhysicalPage(
    K2OSKERN_OBJ_HEADER *apPageOwner
)
{
    K2TREE_NODE *               pTreeNode;
    UINT32                      userVal;
    K2OSKERN_PHYSTRACK_PAGE *   pTrackPage;

    pTreeNode = K2TREE_FirstNode(&gData.PhysFreeTree);
    K2_ASSERT(pTreeNode != NULL);
    userVal = pTreeNode->mUserVal;
    K2TREE_Remove(&gData.PhysFreeTree, pTreeNode);
    pTrackPage = (K2OSKERN_PHYSTRACK_PAGE*)pTreeNode;
    if ((userVal >> K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL) > 1)
    {
        // some space was left in the smallest node in the tree so
        // put that space back on the physical memory tree
        pTreeNode++;
        pTreeNode->mUserVal = userVal - (1 << K2OSKERN_PHYSTRACK_PAGE_COUNT_SHL);
        K2TREE_Insert(&gData.PhysFreeTree, pTreeNode->mUserVal, pTreeNode);
    }
    // put the page on the overhead list
    pTrackPage->mFlags = (KernPhysPageList_KOver << K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) | (userVal & K2OSKERN_PHYSTRACK_PROP_MASK);
    pTrackPage->mpOwnerObject = apPageOwner;
    K2LIST_AddAtTail(&gData.PhysPageList[KernPhysPageList_KOver], &pTrackPage->ListLink);

    return K2OS_PHYSTRACK_TO_PHYS32((UINT32)pTrackPage);
}

void 
KernUser_Init(
    void
)
{
    UINT32                      coreIx;
    UINT32                      coreIdleThreadIx;
    UINT32                      chkOld;
    UINT32 *                    pThreadPtrs;
    UINT32                      tlsPageVirtOffset;
    BOOL                        ptePresent;
    UINT32                      pte;
    K2OSKERN_CPUCORE volatile * pCore;
    K2ROFS_FILE const *         pFile;
    K2ELF32PARSE                parse;
    K2STAT                      stat;
    Elf32_Shdr const *          pDlxInfoHdr;
    DLX_INFO const *            pDlxInfo;

    pThreadPtrs = ((UINT32 *)K2OS_KVA_THREADPTRS_BASE);

    //
    // create idle threads, one for each existing core
    //
    for (coreIx = 0; coreIx < gData.LoadInfo.mCpuCoreCount; coreIx++)
    {
        //
        // pull the next thread index off the threads index free list
        //
        do
        {
            coreIdleThreadIx = gData.mNextThreadSlotIx;
            chkOld = K2ATOMIC_CompareExchange(&gData.mNextThreadSlotIx, pThreadPtrs[coreIdleThreadIx], coreIdleThreadIx);
        } while (chkOld != coreIdleThreadIx);
        K2_ASSERT(coreIdleThreadIx != 0);
        K2_ASSERT(coreIdleThreadIx < K2OS_MAX_THREAD_COUNT);
        K2_ASSERT(0 != gData.mNextThreadSlotIx);

        pThreadPtrs[coreIdleThreadIx] = 0;

        pCore = K2OSKERN_COREIX_TO_CPUCORE(coreIx);

        pCore->IdleThread.Hdr.mObjType = KernObj_Thread;
        pCore->IdleThread.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT | K2OSKERN_OBJ_FLAG_EMBEDDED;
        pCore->IdleThread.mpProc = gpProc1;
        pCore->IdleThread.mIx = coreIdleThreadIx;

        K2LIST_AddAtTail(&gpProc1->ThreadList, (K2LIST_LINK *)&pCore->IdleThread.ProcThreadListLink);

        //
        // set the thread pointer in the thread pointers page to the new thread
        //
        pThreadPtrs[coreIdleThreadIx] = (UINT32)&pCore->IdleThread;

        //
        // allocate a physical page for the idle thread's TLS area and stack
        // this page is never freed so we can use it for the stack without caring about it
        //
        pCore->IdleThread.mTlsPagePhys = sGetOnePhysicalPage((K2OSKERN_OBJ_HEADER *)&pCore->IdleThread.Hdr);
        ;
        //
        // virt offset in the process' user space is the page index from zero
        //
        tlsPageVirtOffset = K2_VA32_MEMPAGE_BYTES * coreIdleThreadIx;

        //
        // map the TLS thread into the global kernel TLS mapping pagetable
        // (which is aliased to K2OS_KVA_PROC1_TLS_PAGETABLE)
        //
        KernMap_MakeOnePresentPage(gpProc1, tlsPageVirtOffset, pCore->IdleThread.mTlsPagePhys, K2OS_MAPTYPE_USER_DATA);

        //
        // assert that the kernel mode mapping just had its page appear magically at the right place since
        // they share a pagetable mapping the TLS area
        //
        KernArch_Translate(
            gpProc1,
            K2OS_KVA_THREAD_TLS_BASE + tlsPageVirtOffset,
            NULL,
            &ptePresent,
            &pte,
            NULL);
        K2_ASSERT(ptePresent);
        K2_ASSERT((pte & K2_VA32_PAGEFRAME_MASK) == pCore->IdleThread.mTlsPagePhys);

        //
        // clear out the TLS page at its target user space address
        //
        K2MEM_Zero((void *)tlsPageVirtOffset, K2_VA32_MEMPAGE_BYTES);

        //
        // set up the idle thread and add it to the run list.  context not set yet
        //
        K2LIST_AddAtTail((K2LIST_ANCHOR *)&pCore->RunList, (K2LIST_LINK *)&pCore->IdleThread.CpuRunListLink);
    }

    //
    // map the pagetable for the public API page in the user space
    //
    pte = sGetOnePhysicalPage(NULL);
    KernArch_InstallPageTable(gpProc1, K2OS_UVA_PUBLICAPI_PAGETABLE_BASE, pte, TRUE);

    //
    // map the public API page into the kernel R/W and into user space R/O
    //
    pte = sGetOnePhysicalPage(NULL);
    KernMap_MakeOnePresentPage(gpProc1, K2OS_UVA_PUBLICAPI_BASE, pte, K2OS_MAPTYPE_USER_READ);
    KernMap_MakeOnePresentPage(gpProc1, K2OS_KVA_PUBLICAPI_BASE, pte, K2OS_MAPTYPE_KERN_DATA);

    //
    // "load" k2oscrt.dlx into process 1
    //
    pFile = K2ROFSHELP_SubFile(gData.mpROFS, K2ROFS_ROOTDIR(gData.mpROFS), "k2oscrt.dlx");
    K2_ASSERT(NULL != pFile);
//    K2OSKERN_Debug("k2oscrt.dlx is at %08X for %d bytes\n", K2ROFS_FILEDATA(gData.mpROFS, pFile), pFile->mSizeBytes);

    stat = K2ELF32_Parse(K2ROFS_FILEDATA(gData.mpROFS, pFile), pFile->mSizeBytes, &parse);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    K2_ASSERT(0 == (parse.mpRawFileData->e_flags & DLX_EF_KERNEL_ONLY));

    K2_ASSERT(DLX_ET_DLX == parse.mpRawFileData->e_type);
    K2_ASSERT(DLX_ELFOSABI_K2 == parse.mpRawFileData->e_ident[EI_OSABI]);
    K2_ASSERT(DLX_ELFOSABIVER_DLX == parse.mpRawFileData->e_ident[EI_ABIVERSION]);

    K2_ASSERT(3 < parse.mpRawFileData->e_shnum);

    K2_ASSERT(DLX_SHN_SHSTR == parse.mpRawFileData->e_shstrndx);

    pDlxInfoHdr = K2ELF32_GetSectionHeader(&parse, DLX_SHN_DLXINFO);
    K2_ASSERT(NULL != pDlxInfoHdr);
    K2_ASSERT((DLX_SHF_TYPE_DLXINFO | SHF_ALLOC) == pDlxInfoHdr->sh_flags);

    pDlxInfo = K2ELF32_GetSectionData(&parse, DLX_SHN_DLXINFO);
    K2_ASSERT(NULL != pDlxInfo);

    K2_ASSERT(0 == pDlxInfo->mImportCount);

    K2_ASSERT(K2OS_UVA_LOW_BASE == pDlxInfo->SegInfo[DlxSeg_Text].mLinkAddr);
    chkOld = pDlxInfo->SegInfo[DlxSeg_Text].mLinkAddr + K2_ROUNDUP(pDlxInfo->SegInfo[DlxSeg_Text].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
    K2_ASSERT(chkOld == pDlxInfo->SegInfo[DlxSeg_Read].mLinkAddr);
    chkOld = pDlxInfo->SegInfo[DlxSeg_Read].mLinkAddr + K2_ROUNDUP(pDlxInfo->SegInfo[DlxSeg_Read].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
    K2_ASSERT(chkOld == pDlxInfo->SegInfo[DlxSeg_Data].mLinkAddr);
    chkOld = pDlxInfo->SegInfo[DlxSeg_Data].mLinkAddr + K2_ROUNDUP(pDlxInfo->SegInfo[DlxSeg_Data].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
    K2_ASSERT(chkOld == pDlxInfo->SegInfo[DlxSeg_Sym].mLinkAddr);
    chkOld = pDlxInfo->SegInfo[DlxSeg_Sym].mLinkAddr + K2_ROUNDUP(pDlxInfo->SegInfo[DlxSeg_Sym].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);

    //
    // going to map from K2OS_UVA_LOW_BASE tp chkOld - map pagetables for this area
    //
//    K2OSKERN_Debug("k2oscrt.dlx taking up %08X->%08X in user space\n", K2OS_UVA_LOW_BASE, chkOld);
    chkOld -= K2OS_UVA_LOW_BASE;
    coreIdleThreadIx = chkOld;
    coreIx = K2OS_UVA_LOW_BASE;
    do
    {
        pte = sGetOnePhysicalPage(&gpProc1->Hdr);
        KernArch_InstallPageTable(gpProc1, coreIx, pte, TRUE);
        if (chkOld < K2_VA32_PAGETABLE_MAP_BYTES)
            break;
        coreIx += K2_VA32_PAGETABLE_MAP_BYTES;
        chkOld -= K2_VA32_PAGETABLE_MAP_BYTES;
    } while (1);

    //
    // alloc the physical pages for all the segments and map them as R/W data
    //
    chkOld = K2OS_UVA_LOW_BASE;
    coreIx = coreIdleThreadIx;
    do
    {
        pte = sGetOnePhysicalPage(&gpProc1->Hdr);
//        K2OSKERN_Debug("MAKE %08X->%08X USER_DATA, ZERO\n", chkOld, pte);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pte, K2OS_MAPTYPE_USER_DATA);   // user PT, so kern mapping doesn't work in x32
        K2MEM_Zero((void *)chkOld, K2_VA32_MEMPAGE_BYTES);
        chkOld += K2_VA32_MEMPAGE_BYTES;
        coreIx -= K2_VA32_MEMPAGE_BYTES;
    } while (coreIx > 0);

    // 
    // copy in content for each segment
    //
    for (coreIx = DlxSeg_Text; coreIx <= DlxSeg_Sym; coreIx++)
    {
//        K2OSKERN_Debug("Copy %08X <- %08X, %d\n", 
//            (void *)pDlxInfo->SegInfo[coreIx].mLinkAddr,
//            ((UINT8 *)parse.mpRawFileData) + pDlxInfo->SegInfo[coreIx].mFileOffset,
//            pDlxInfo->SegInfo[coreIx].mFileBytes);
        K2MEM_Copy(
            (void *)pDlxInfo->SegInfo[coreIx].mLinkAddr,
            ((UINT8 *)parse.mpRawFileData) + pDlxInfo->SegInfo[coreIx].mFileOffset,
            pDlxInfo->SegInfo[coreIx].mFileBytes);
    }
//    K2OSKERN_Debug("Flush(%08X, %08X)\n", K2OS_UVA_LOW_BASE, coreIdleThreadIx);
    KernArch_FlushCache(K2OS_UVA_LOW_BASE, coreIdleThreadIx);

    //
    // break and re-make the proper mapping for text, read, sym segments
    //
    chkOld = K2OS_UVA_LOW_BASE;
    coreIx = K2_ROUNDUP(pDlxInfo->SegInfo[DlxSeg_Text].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
    do
    {
        pte = KernMap_BreakOnePage(gpProc1, chkOld, 0);
//        K2OSKERN_Debug("BRAK %08X (was %08X), INVALIDATE\n", chkOld, pte);
        KernArch_InvalidateTlbPageOnThisCore(chkOld);
//        K2OSKERN_Debug("MAKE %08X->%08X USER_TEXT\n", chkOld, pte);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pte, K2OS_MAPTYPE_USER_TEXT);
        chkOld += K2_VA32_MEMPAGE_BYTES;
        coreIx -= K2_VA32_MEMPAGE_BYTES;
    } while (coreIx > 0);

    coreIx = K2_ROUNDUP(pDlxInfo->SegInfo[DlxSeg_Read].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
    do
    {
        pte = KernMap_BreakOnePage(gpProc1, chkOld, 0);
//        K2OSKERN_Debug("BRAK %08X (was %08X), INVALIDATE\n", chkOld, pte);
        KernArch_InvalidateTlbPageOnThisCore(chkOld);
//        K2OSKERN_Debug("MAKE %08X->%08X USER_READ\n", chkOld, pte);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pte, K2OS_MAPTYPE_USER_READ);
        chkOld += K2_VA32_MEMPAGE_BYTES;
        coreIx -= K2_VA32_MEMPAGE_BYTES;
    } while (coreIx > 0);

    coreIx = K2_ROUNDUP(pDlxInfo->SegInfo[DlxSeg_Data].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
    chkOld += coreIx;

    coreIx = K2_ROUNDUP(pDlxInfo->SegInfo[DlxSeg_Sym].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);
    do
    {
        pte = KernMap_BreakOnePage(gpProc1, chkOld, 0);
//        K2OSKERN_Debug("BRAK %08X (was %08X), INVALIDATE\n", chkOld, pte);
        KernArch_InvalidateTlbPageOnThisCore(chkOld);
//        K2OSKERN_Debug("MAKE %08X->%08X USER_READ\n", chkOld, pte);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pte, K2OS_MAPTYPE_USER_READ);
        chkOld += K2_VA32_MEMPAGE_BYTES;
        coreIx -= K2_VA32_MEMPAGE_BYTES;
    } while (coreIx > 0);

#if 0
    K2OSKERN_Debug("  SEGMENTS:\n");
    for (coreIx = 0; coreIx < DlxSeg_Count; coreIx++)
    {
        K2OSKERN_Debug("    %d: crc %08X file %08X offs %08X link %08X mem %08X\n",
            coreIx,
            pDlxInfo->SegInfo[coreIx].mCRC32,
            pDlxInfo->SegInfo[coreIx].mFileBytes,
            pDlxInfo->SegInfo[coreIx].mFileOffset,
            pDlxInfo->SegInfo[coreIx].mLinkAddr,
            pDlxInfo->SegInfo[coreIx].mMemActualBytes);
    }
    K2OSKERN_Debug("  ENTRY: %08X\n", parse.mpRawFileData->e_entry);
#endif

    //
    //  fill in public api page, and set idle threads' entry points and stack pointers in their contexts
    // 
    KernArch_UserInit(parse.mpRawFileData->e_entry);
}

