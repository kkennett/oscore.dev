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
KernUser_Init(
    void
)
{
    UINT32                      coreIx;
    UINT32                      crtVirtBytes;
    UINT32                      chkOld;
    UINT32                      pagePhys;
    K2OSKERN_CPUCORE volatile * pCore;
    K2ROFS_FILE const *         pFile;
    K2ELF32PARSE                parse;
    K2STAT                      stat;
    Elf32_Shdr const *          pDlxInfoHdr;
    DLX_INFO const *            pDlxInfo;
    K2OSKERN_OBJ_THREAD *       pFirstThread;
    UINT32                      firstThreadIx;
    UINT32 *                    pThreadPtrs;
    UINT32                      tlsPageVirtOffset;
    BOOL                        ptePresent;
    UINT32                      pte;

    pThreadPtrs = ((UINT32 *)K2OS_KVA_THREADPTRS_BASE);

    //
    // create 'fake' idle threads, one for each existing core.  these don't have valid indexes
    //
    for (coreIx = 0; coreIx < gData.LoadInfo.mCpuCoreCount; coreIx++)
    {
        pCore = K2OSKERN_COREIX_TO_CPUCORE(coreIx);

        KernThread_InitOne((K2OSKERN_OBJ_THREAD *)&pCore->IdleThread);
        pCore->IdleThread.Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT | K2OSKERN_OBJ_FLAG_EMBEDDED;
        pCore->IdleThread.mpProc = gpProc1;

        KernObj_Add((K2OSKERN_OBJ_HEADER *)&pCore->IdleThread.Hdr);

        K2LIST_AddAtTail(&gpProc1->ThreadList, (K2LIST_LINK *)&pCore->IdleThread.ProcThreadListLink);

        //
        // idle threads don't get a TLS page because they can actually never
        // execute any code.
        //

        //
        // add idle thread to core's run list.  it never leaves that list
        //
        pCore->IdleThread.mpCurrentCore = pCore;
        pCore->IdleThread.mState = KernThreadState_OnCoreRunList;
        K2LIST_AddAtTail((K2LIST_ANCHOR *)&pCore->RunList, (K2LIST_LINK *)&pCore->IdleThread.CpuRunListLink);
    }

    //
    // create the initial thread for process 1
    //
    pFirstThread = &gpProc1->InitialThread;

    //
    // pull the next thread index off the threads index free list
    //
    do
    {
        firstThreadIx = gData.mNextThreadSlotIx;
        chkOld = K2ATOMIC_CompareExchange(&gData.mNextThreadSlotIx, pThreadPtrs[firstThreadIx], firstThreadIx);
    } while (chkOld != firstThreadIx);
    K2_ASSERT(firstThreadIx != 0);
    K2_ASSERT(firstThreadIx < K2OS_MAX_THREAD_COUNT);
    K2_ASSERT(0 != gData.mNextThreadSlotIx);

    pThreadPtrs[firstThreadIx] = (UINT32)pFirstThread;

    KernThread_InitOne(pFirstThread);
    pFirstThread->Hdr.mObjFlags = K2OSKERN_OBJ_FLAG_PERMANENT | K2OSKERN_OBJ_FLAG_EMBEDDED;
    pFirstThread->mpProc = gpProc1;
    pFirstThread->mIx = firstThreadIx;
    pFirstThread->mpKernRwViewOfUserThreadPage = (K2OS_USER_THREAD_PAGE *)
            (K2OS_KVA_THREAD_TLS_BASE + (firstThreadIx * K2_VA32_MEMPAGE_BYTES));

    KernObj_Add(&pFirstThread->Hdr);

    K2LIST_AddAtTail(&gpProc1->ThreadList, (K2LIST_LINK *)&pFirstThread->ProcThreadListLink);

    //
    // allocate a physical page for the first thread's TLS area
    //
    pFirstThread->mTlsPagePhys = KernPhys_AllocOneKernelPage((K2OSKERN_OBJ_HEADER *)&pFirstThread->Hdr);

    //
    // virt offset in the process' user space is the page index from zero
    //
    tlsPageVirtOffset = K2_VA32_MEMPAGE_BYTES * firstThreadIx;

    //
    // map the TLS thread into the global kernel TLS mapping pagetable
    // (which is aliased to K2OS_KVA_PROC1_TLS_PAGETABLE)
    //
    KernMap_MakeOnePresentPage(gpProc1, tlsPageVirtOffset, pFirstThread->mTlsPagePhys, K2OS_MAPTYPE_USER_DATA);

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
    K2_ASSERT((pte & K2_VA32_PAGEFRAME_MASK) == pFirstThread->mTlsPagePhys);

    //
    // clear out the TLS page at its target user space address
    //
    K2MEM_Zero((void *)tlsPageVirtOffset, K2_VA32_MEMPAGE_BYTES);

    //
    // add first thread to core 0 list at the head
    //
    pCore = K2OSKERN_COREIX_TO_CPUCORE(0);
    pFirstThread->mpCurrentCore = pCore;
    pFirstThread->mState = KernThreadState_OnCoreRunList;
    K2LIST_AddAtHead((K2LIST_ANCHOR *)&pCore->RunList, (K2LIST_LINK *)&pFirstThread->CpuRunListLink);

    //
    // map the pagetable for the public API page and the ROFS in the user address space
    //
    pagePhys = KernPhys_AllocOneKernelPage(NULL);
    KernArch_InstallPageTable(gpProc1, K2OS_UVA_PUBLICAPI_PAGETABLE_BASE, pagePhys, TRUE);

    //
    // map the public API page into the kernel R/W and into user space R/O
    //
    pagePhys = KernPhys_AllocOneKernelPage(NULL);
    KernMap_MakeOnePresentPage(gpProc1, K2OS_UVA_PUBLICAPI_BASE, pagePhys, K2OS_MAPTYPE_USER_TEXT);
    KernMap_MakeOnePresentPage(gpProc1, K2OS_KVA_PUBLICAPI_BASE, pagePhys, K2OS_MAPTYPE_KERN_DATA);
    K2MEM_Zero((void *)K2OS_KVA_PUBLICAPI_BASE, K2_VA32_MEMPAGE_BYTES);
    KernArch_FlushCache(K2OS_KVA_PUBLICAPI_BASE, K2_VA32_MEMPAGE_BYTES);

    //
    // ROFS in kernel is already mapped.
    // map ROFS into proc 1 as read-only data right underneath the public api page.
    // the address of the ROFS is the first argument to the k2oscrt inital thread
    //
    coreIx = gData.mpROFS->mSectorCount;
    coreIx *= K2ROFS_SECTOR_BYTES;
    coreIx = K2_ROUNDUP(coreIx, K2_VA32_MEMPAGE_BYTES);
    K2_ASSERT(coreIx <= (K2_VA32_PAGETABLE_MAP_BYTES - K2_VA32_MEMPAGE_BYTES));
    chkOld = K2OS_UVA_PUBLICAPI_BASE - coreIx;
    pagePhys = gData.LoadInfo.mBuiltinRofsPhys;
    do
    {
//        K2OSKERN_Debug("MAKE %08X->%08X USER_READ (ROFS)\n", chkOld, pagePhys);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pagePhys, K2OS_MAPTYPE_USER_READ);
        chkOld += K2_VA32_MEMPAGE_BYTES;
        pagePhys += K2_VA32_MEMPAGE_BYTES;
        coreIx -= K2_VA32_MEMPAGE_BYTES;
    } while (coreIx > 0);

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

    coreIx = (pDlxInfo->SegInfo[DlxSeg_Text].mMemActualBytes + (K2_VA32_MEMPAGE_BYTES - 1)) & K2_VA32_PAGEFRAME_MASK;
    gData.UserCrtInfo.mTextPagesCount = coreIx / K2_VA32_MEMPAGE_BYTES;
    chkOld = pDlxInfo->SegInfo[DlxSeg_Text].mLinkAddr + coreIx;
    K2_ASSERT(chkOld == pDlxInfo->SegInfo[DlxSeg_Read].mLinkAddr);

    coreIx = (pDlxInfo->SegInfo[DlxSeg_Read].mMemActualBytes + (K2_VA32_MEMPAGE_BYTES - 1)) & K2_VA32_PAGEFRAME_MASK;
    gData.UserCrtInfo.mReadPagesCount = coreIx / K2_VA32_MEMPAGE_BYTES;
    chkOld = pDlxInfo->SegInfo[DlxSeg_Read].mLinkAddr + coreIx;
    K2_ASSERT(chkOld == pDlxInfo->SegInfo[DlxSeg_Data].mLinkAddr);

    coreIx = (pDlxInfo->SegInfo[DlxSeg_Data].mMemActualBytes + (K2_VA32_MEMPAGE_BYTES - 1)) & K2_VA32_PAGEFRAME_MASK;
    gData.UserCrtInfo.mDataPagesCount = coreIx / K2_VA32_MEMPAGE_BYTES;
    chkOld = pDlxInfo->SegInfo[DlxSeg_Data].mLinkAddr + coreIx;
    K2_ASSERT(chkOld == pDlxInfo->SegInfo[DlxSeg_Sym].mLinkAddr);

    coreIx = (pDlxInfo->SegInfo[DlxSeg_Sym].mMemActualBytes + (K2_VA32_MEMPAGE_BYTES - 1)) & K2_VA32_PAGEFRAME_MASK;
    gData.UserCrtInfo.mSymPagesCount = coreIx / K2_VA32_MEMPAGE_BYTES;
    chkOld = pDlxInfo->SegInfo[DlxSeg_Sym].mLinkAddr + coreIx;

    //
    // going to map from K2OS_UVA_LOW_BASE tp chkOld - map pagetables for this area
    //
//    K2OSKERN_Debug("k2oscrt.dlx taking up %08X->%08X in user space\n", K2OS_UVA_LOW_BASE, chkOld);
    chkOld -= K2OS_UVA_LOW_BASE;
    crtVirtBytes = chkOld;
    gData.UserCrtInfo.mPageTablesCount = 0;
    coreIx = K2OS_UVA_LOW_BASE;
    do
    {
        pagePhys = KernPhys_AllocOneKernelPage(&gpProc1->Hdr);
        KernArch_InstallPageTable(gpProc1, coreIx, pagePhys, TRUE);
        gData.UserCrtInfo.mPageTablesCount++;
        if (chkOld <= K2_VA32_PAGETABLE_MAP_BYTES)
            break;
        coreIx += K2_VA32_PAGETABLE_MAP_BYTES;
        chkOld -= K2_VA32_PAGETABLE_MAP_BYTES;
    } while (1);

    //
    // alloc the physical pages for all the segments and map them as R/W data
    //
    chkOld = K2OS_UVA_LOW_BASE;
    coreIx = crtVirtBytes;
    do
    {
        pagePhys = KernPhys_AllocOneKernelPage(&gpProc1->Hdr);
//        K2OSKERN_Debug("MAKE %08X->%08X USER_DATA, ZERO\n", chkOld, pagePhys);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pagePhys, K2OS_MAPTYPE_USER_DATA);   // user PT, so kern mapping doesn't work in x32
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
//    K2OSKERN_Debug("Flush(%08X, %08X)\n", K2OS_UVA_LOW_BASE, crtVirtBytes);
    KernArch_FlushCache(K2OS_UVA_LOW_BASE, crtVirtBytes);

    //
    // break and re-make the proper mapping for text, read, sym segments
    //
    chkOld = K2OS_UVA_LOW_BASE;
    coreIx = gData.UserCrtInfo.mTextPagesCount * K2_VA32_MEMPAGE_BYTES;
    do
    {
        pagePhys = KernMap_BreakOnePage(gpProc1, chkOld, 0);
//        K2OSKERN_Debug("BRAK %08X (was %08X), INVALIDATE\n", chkOld, pagePhys);
        KernArch_InvalidateTlbPageOnThisCore(chkOld);
//        K2OSKERN_Debug("MAKE %08X->%08X USER_TEXT\n", chkOld, pagePhys);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pagePhys, K2OS_MAPTYPE_USER_TEXT);
        chkOld += K2_VA32_MEMPAGE_BYTES;
        coreIx -= K2_VA32_MEMPAGE_BYTES;
    } while (coreIx > 0);

    coreIx = gData.UserCrtInfo.mReadPagesCount * K2_VA32_MEMPAGE_BYTES;
    do
    {
        pagePhys = KernMap_BreakOnePage(gpProc1, chkOld, 0);
//        K2OSKERN_Debug("BRAK %08X (was %08X), INVALIDATE\n", chkOld, pagePhys);
        KernArch_InvalidateTlbPageOnThisCore(chkOld);
//        K2OSKERN_Debug("MAKE %08X->%08X USER_READ\n", chkOld, pagePhys);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pagePhys, K2OS_MAPTYPE_USER_READ);
        chkOld += K2_VA32_MEMPAGE_BYTES;
        coreIx -= K2_VA32_MEMPAGE_BYTES;
    } while (coreIx > 0);

    gData.UserCrtInfo.mDataPagesVirtAddr = chkOld;
    gData.UserCrtInfo.mpDataSrc = ((UINT8 const *)parse.mpRawFileData) + pDlxInfo->SegInfo[DlxSeg_Data].mFileOffset;
    gData.UserCrtInfo.mDataSrcBytes = pDlxInfo->SegInfo[DlxSeg_Data].mFileBytes;
    chkOld += gData.UserCrtInfo.mDataPagesCount * K2_VA32_MEMPAGE_BYTES;

    coreIx = gData.UserCrtInfo.mSymPagesCount * K2_VA32_MEMPAGE_BYTES;
    do
    {
        pagePhys = KernMap_BreakOnePage(gpProc1, chkOld, 0);
//        K2OSKERN_Debug("BRAK %08X (was %08X), INVALIDATE\n", chkOld, pagePhys);
        KernArch_InvalidateTlbPageOnThisCore(chkOld);
//        K2OSKERN_Debug("MAKE %08X->%08X USER_READ\n", chkOld, pagePhys);
        KernMap_MakeOnePresentPage(gpProc1, chkOld, pagePhys, K2OS_MAPTYPE_USER_READ);
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
    gData.UserCrtInfo.mEntrypoint = parse.mpRawFileData->e_entry;
    KernArch_UserInit();
}

