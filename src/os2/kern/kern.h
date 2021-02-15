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

#ifndef __KERN_H
#define __KERN_H

#include <k2oshal.h>
#include "kerndef.inc"
#include "kernshared.h"

/* --------------------------------------------------------------------------------- */

typedef struct _K2OSKERN_CPUCORE            K2OSKERN_CPUCORE;
typedef struct _K2OSKERN_COREMEMORY         K2OSKERN_COREMEMORY;
typedef struct _K2OSKERN_OBJ_HEADER         K2OSKERN_OBJ_HEADER;
typedef struct _K2OSKERN_OBJ_PROCESS        K2OSKERN_OBJ_PROCESS;
typedef struct _K2OSKERN_OBJ_INTR           K2OSKERN_OBJ_INTR;

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_CPUCORE
{
#if K2_TARGET_ARCH_IS_INTEL
    /* must be first thing in struct */
    X32_TSS                             TSS;
#endif
    UINT32                              mCoreIx;

    BOOL                                mIsExecuting;

};

#define K2OSKERN_COREMEMORY_STACKS_BYTES  ((K2_VA32_MEMPAGE_BYTES - sizeof(K2OSKERN_CPUCORE)) + (K2_VA32_MEMPAGE_BYTES * 3))

struct _K2OSKERN_COREMEMORY
{
    K2OSKERN_CPUCORE    CpuCore;  /* must be first thing in struct */
    UINT8               mStacks[K2OSKERN_COREMEMORY_STACKS_BYTES];
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_COREMEMORY) == (4 * K2_VA32_MEMPAGE_BYTES));

#define K2OSKERN_COREIX_TO_CPUCORE(x)       ((K2OSKERN_CPUCORE volatile *)(K2OS_KVA_COREMEMORY_BASE + ((x) * K2_VA32_MEMPAGE_BYTES)))
#define K2OSKERN_GET_CURRENT_CPUCORE        K2OSKERN_COREIX_TO_CPUCORE(K2OSKERN_GetCpuIndex())

#define K2OSKERN_COREIX_TO_COREMEMORY(x)    ((K2OSKERN_COREMEMORY *)(K2OS_KVA_COREMEMORY_BASE + ((x) * K2_VA32_MEMPAGE_BYTES)))
#define K2OSKERN_GET_CURRENT_COREMEMORY     K2OSKERN_COREIX_TO_COREPAGE(K2OSKERN_GetCpuIndex())

/* --------------------------------------------------------------------------------- */

#define K2OSKERN_OBJ_FLAG_PERMANENT     0x80000000
#define K2OSKERN_OBJ_FLAG_EMBEDDED      0x40000000

typedef void (*K2OSKERN_pf_ObjDispose)(K2OSKERN_OBJ_HEADER *apObj);

struct _K2OSKERN_OBJ_HEADER
{
    UINT32                  mObjType;
    UINT32                  mObjFlags;
    INT32 volatile          mRefCount;
    K2OSKERN_pf_ObjDispose  Dispose;

    K2TREE_NODE             ObjTreeNode;
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_PROCESS
{
    K2OSKERN_OBJ_HEADER     Hdr;
    UINT32                  mId;
    UINT32                  mTransTableKVA;     // VIRT of root trans table
    UINT32                  mTransTableRegVal;  // PHYS(+) of root trans table
    UINT32                  mVirtMapKVA;        // VIRT of PTE array for user space

};

#define gpProc1 ((K2OSKERN_OBJ_PROCESS * const)K2OS_KVA_PROC1_BASE)

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_INTR
{
    K2OSKERN_OBJ_HEADER     Hdr;

    K2TREE_NODE             IntrTreeNode;

    K2OSKERN_IRQ_CONFIG     IrqConfig;

    K2OSKERN_pf_IntrHandler mfHandler;

    void *                  mpHandlerContext;

    //
    // TBD - notify for interrupt
    //
};

/* --------------------------------------------------------------------------------- */

typedef enum _KernInitStage KernInitStage;
enum _KernInitStage
{
    KernInitStage_dlx_entry = 0,
    KernInitStage_Before_Virt,
    KernInitStage_After_Virt,
    KernInitStage_Before_Hal,
    KernInitStage_At_Hal_Entry,
    KernInitStage_After_Hal,
    KernInitStage_Before_Launch_Cores,
    KernInitStage_Threaded,
    KernInitStage_MemReady,
    KernInitStage_MultiThreaded,
    // should be last entry
    KernInitStage_Count
};

/* --------------------------------------------------------------------------------- */

typedef struct _KERN_DATA KERN_DATA;
struct _KERN_DATA
{
    // 
    // fundamentals
    //
    K2OSKERN_SHARED *                   mpShared;

    K2OSKERN_SEQLOCK                    DebugSeqLock;

    UINT32                              mCpuCount;

    KernInitStage                       mKernInitStage;

    INT32 volatile                      mCoresInPanic;

    K2DLXSUPP_HOST                      DlxHost;
    DLX *                               mpDlxKern;
    DLX *                               mpDlxHal;

    // arch specific
#if K2_TARGET_ARCH_IS_ARM
    UINT32                              mA32VectorPagePhys;
    UINT32                              mA32StartAreaPhys;      // K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT
#endif
#if K2_TARGET_ARCH_IS_INTEL
    UINT32                              mX32IoApicCount;
#endif
};

extern KERN_DATA gData;

/* --------------------------------------------------------------------------------- */

void KernInit_Arch(void);


void   K2_CALLCONV_REGS KernEx_Assert(char const * apFile, int aLineNum, char const * apCondition);
K2STAT K2_CALLCONV_REGS KernEx_TrapDismount(K2_EXCEPTION_TRAP *apTrap);
BOOL   K2_CALLCONV_REGS KernEx_TrapMount(K2_EXCEPTION_TRAP *apTrap);

UINT32 KernDebug_OutputWithArgs(char const *apFormat, VALIST aList);
void   KernPanic_Ici(K2OSKERN_CPUCORE volatile * apThisCore);

void    KernArch_SendIci(UINT32 aCurCoreIx, BOOL aSendToSpecific, UINT32 aTargetCpuIx);

void    KernArch_Panic(K2OSKERN_CPUCORE volatile *apThisCore, BOOL aDumpStack);
void KernArch_LaunchCores(void);
UINT32  KernArch_DevIrqToVector(UINT32 aDevIrq);
UINT32  KernArch_VectorToDevIrq(UINT32 aVector);
void KernArch_InvalidateTlbPageOnThisCore(UINT32 aVirtAddr);

UINT32 KernArch_MakePTE(UINT32 aPhysAddr, UINT32 aPageMapAttr);
void  KernArch_WritePTE(BOOL aIsMake, UINT32 aVirtAddr, UINT32* pPTE, UINT32 aPTE);


void   KernDlxSupp_AtReInit(DLX *apDlx, UINT32 aModulePageLinkAddr, K2DLXSUPP_HOST_FILE *apInOutHostFile);
UINT32 KernDlx_FindClosestSymbol(K2OSKERN_OBJ_PROCESS *apCurProc, UINT32 aAddr, char *apRetSymName, UINT32 aRetSymNameBufLen);

void K2_CALLCONV_REGS KernExec(void);

void KernInit_Stage(KernInitStage aStage);

void   KernMap_MakeOnePresentPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aPhysAddr, UINTN aMapAttr);
void   KernMap_MakeOneNotPresentPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aNpFlags, UINT32 aContent);
UINT32 KernMap_BreakOnePage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aNpFlags);


/* --------------------------------------------------------------------------------- */

#endif // __KERN_H