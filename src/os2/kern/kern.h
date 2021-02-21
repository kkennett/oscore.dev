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

#include <k2oshal.h>    // includes k2oskern.h
#include "kerndef.inc"
#include <lib/k2rofshelp.h>

/* --------------------------------------------------------------------------------- */

typedef struct _K2OSKERN_CPUCORE            K2OSKERN_CPUCORE;
typedef struct _K2OSKERN_COREMEMORY         K2OSKERN_COREMEMORY;
typedef struct _K2OSKERN_OBJ_HEADER         K2OSKERN_OBJ_HEADER;
typedef struct _K2OSKERN_OBJ_PROCESS        K2OSKERN_OBJ_PROCESS;
typedef struct _K2OSKERN_OBJ_INTR           K2OSKERN_OBJ_INTR;

typedef struct _K2OSKERN_PHYSTRACK_PAGE     K2OSKERN_PHYSTRACK_PAGE;
typedef struct _K2OSKERN_PHYSTRACK_FREE     K2OSKERN_PHYSTRACK_FREE;

/* --------------------------------------------------------------------------------- */

typedef enum _KernObj KernObj;
enum _KernObj
{
    KernObj_Error = 0,

    KernObj_Process,

    KernObj_Count
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_CPUCORE
{
#if K2_TARGET_ARCH_IS_INTEL
    /* must be first thing in struct */
    X32_TSS         TSS;
#endif

    UINT32          mCoreIx;

    BOOL            mIsExecuting;

    UINT32 volatile mIciFromOtherCore[K2OS_MAX_CPU_COUNT];
};

#define K2OSKERN_COREMEMORY_STACKS_BYTES  ((K2_VA32_MEMPAGE_BYTES - sizeof(K2OSKERN_CPUCORE)) + (K2_VA32_MEMPAGE_BYTES * 3))

struct _K2OSKERN_COREMEMORY
{
    K2OSKERN_CPUCORE    CpuCore;  /* must be first thing in struct */
    UINT8               mStacks[K2OSKERN_COREMEMORY_STACKS_BYTES];
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_COREMEMORY) == (4 * K2_VA32_MEMPAGE_BYTES));

#define K2OSKERN_COREIX_TO_CPUCORE(x)       ((K2OSKERN_CPUCORE volatile *)(K2OS_KVA_COREMEMORY_BASE + ((x) * (K2_VA32_MEMPAGE_BYTES * 4))))
#define K2OSKERN_GET_CURRENT_CPUCORE        K2OSKERN_COREIX_TO_CPUCORE(K2OSKERN_GetCpuIndex())

#define K2OSKERN_COREIX_TO_COREMEMORY(x)    ((K2OSKERN_COREMEMORY *)(K2OS_KVA_COREMEMORY_BASE + ((x) * (K2_VA32_MEMPAGE_BYTES * 4))))
#define K2OSKERN_GET_CURRENT_COREMEMORY     K2OSKERN_COREIX_TO_COREPAGE(K2OSKERN_GetCpuIndex())

/* --------------------------------------------------------------------------------- */

typedef enum _KernPhysPageList KernPhysPageList;
enum _KernPhysPageList
{
    KernPhysPageList_Error,         //  0        

    KernPhysPageList_Unusable,      //  1       EFI_Reserved, EFI_ACPI_NVS
    KernPhysPageList_Trans,         //  2       TRANSITION
    KernPhysPageList_Paging,        //  3       PAGING
    KernPhysPageList_Proc,          //  4       PROCESS, THREAD STACKS
    KernPhysPageList_KOver,         //  5       PHYS_TRACK, ZERO, CORES, EFI_MAP, 
    KernPhysPageList_Free_Dirty,    //  6
    KernPhysPageList_Free_Clean,    //  7 
    KernPhysPageList_KText,         //  8       KERNEL.ELF TEXT, EFI Runtime Code
    KernPhysPageList_KRead,         //  9       KERNEl.ELF READ
    KernPhysPageList_KData,         //  10      KERNEL.ELF DATA, EFI Runtime Data
    KernPhysPageList_UText,         //  11
    KernPhysPageList_URead,         //  12
    KernPhysPageList_UData,         //  13
    KernPhysPageList_General,       //  14
    KernPhysPageList_DeviceU,       //  15
    KernPhysPageList_DeviceC,       //  16

    KernPhysPageList_Count,         //  17

    KernPhysPageList_Thread_Working = 26,     // temporarily set as thread working page
    KernPhysPageList_Thread_PtWorking = 27,   // temporarily set as thread pt working page
    KernPhysPageList_Thread_Dirty  = 28,      // temporarily on thread dirty list
    KernPhysPageList_Thread_Clean  = 29,      // temporarily on thread clean list
    KernPhysPageList_Thread_PtDirty =30,      // temporarily on thread pagetable dirty list
    KernPhysPageList_Thread_PtClean =31,      // temporarily on thread pagetable clean list
    // 31 is max
};

K2_STATIC_ASSERT((K2OSKERN_PHYSTRACK_PAGE_LIST_MASK >> K2OSKERN_PHYSTRACK_PAGE_LIST_SHL) >= KernPhysPageList_Count);

struct _K2OSKERN_PHYSTRACK_PAGE
{
    UINT32      mFlags;
    void *      mpOwnerObject;
    K2LIST_LINK ListLink;
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_PHYSTRACK_PAGE) == K2OS_PHYSTRACK_BYTES);
K2_STATIC_ASSERT(sizeof(K2OSKERN_PHYSTRACK_PAGE) == sizeof(K2OS_PHYSTRACK_UEFI));
K2_STATIC_ASSERT(sizeof(K2OSKERN_PHYSTRACK_PAGE) == sizeof(K2TREE_NODE));

struct _K2OSKERN_PHYSTRACK_FREE
{
    UINT32      mFlags; // must be exact same location as mUserVal
    UINT32      mTreeNode_ParentBal;
    UINT32      mTreeNode_LeftChild;
    UINT32      mTreeNode_RightChild;
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_PHYSTRACK_FREE) == K2OS_PHYSTRACK_BYTES);
K2_STATIC_ASSERT(sizeof(K2OSKERN_PHYSTRACK_FREE) == sizeof(K2OS_PHYSTRACK_UEFI));
K2_STATIC_ASSERT(sizeof(K2OSKERN_PHYSTRACK_FREE) == sizeof(K2TREE_NODE));

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
    UINT32                  mVirtMapKVA;        // VIRT of PTE array
    K2LIST_LINK             ProcListLink;
};

#define gpProc1 ((K2OSKERN_OBJ_PROCESS * const)K2OS_KVA_PROC1)

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_IRQ_LINE_CONFIG
{
    BOOL    mIsEdgeTriggered;
    BOOL    mIsActiveLow;
    BOOL    mShareConfig;
    BOOL    mWakeConfig;
};
typedef struct _K2OSKERN_IRQ_LINE_CONFIG K2OSKERN_IRQ_LINE_CONFIG;

struct _K2OSKERN_IRQ_CONFIG
{
    UINT32                      mSourceIrq;
    UINT32                      mDestCoreIx;
    K2OSKERN_IRQ_LINE_CONFIG    Line;
};
typedef struct _K2OSKERN_IRQ_CONFIG K2OSKERN_IRQ_CONFIG;

typedef UINT32 (*K2OSKERN_pf_IntrHandler)(void *apContext);

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

typedef struct _KERN_DATA KERN_DATA;
struct _KERN_DATA
{
    // 
    // fundamentals
    //
    K2OS_UEFI_LOADINFO      LoadInfo;

    K2OSKERN_SEQLOCK        DebugSeqLock;
    INT32 volatile          mCoresInPanic;

    K2OSKERN_SEQLOCK        ProcListSeqLock;
    K2LIST_ANCHOR           ProcList;

    K2OSKERN_SEQLOCK        PhysMemSeqLock;
    K2TREE_ANCHOR           PhysFreeTree;
    K2LIST_ANCHOR           PhysPageList[KernPhysPageList_Count];

#if K2_TARGET_ARCH_IS_ARM
    // arch specific - used in common phys init code
    UINT32                  mA32VectorPagePhys;
    UINT32                  mA32StartAreaPhys;      // K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT
#endif
};

extern KERN_DATA gData;

/* --------------------------------------------------------------------------------- */

void    K2_CALLCONV_REGS __attribute__((noreturn)) Kern_Main(K2OS_UEFI_LOADINFO const *apLoadInfo);

void    K2_CALLCONV_REGS KernEx_Assert(char const * apFile, int aLineNum, char const * apCondition);
K2STAT  K2_CALLCONV_REGS KernEx_TrapDismount(K2_EXCEPTION_TRAP *apTrap);
BOOL    K2_CALLCONV_REGS KernEx_TrapMount(K2_EXCEPTION_TRAP *apTrap);
void    K2_CALLCONV_REGS KernEx_RaiseException(K2STAT aExceptionCode);

void    KernArch_InitAtEntry(void);
UINT32  KernArch_DevIrqToVector(UINT32 aDevIrq);
UINT32  KernArch_VectorToDevIrq(UINT32 aVector);
void    KernArch_Panic(K2OSKERN_CPUCORE volatile *apThisCore, BOOL aDumpStack);
UINT32  KernArch_MakePTE(UINT32 aPhysAddr, UINT32 aPageMapAttr);
void    KernArch_BreakMapTransitionPageTable(UINT32 *apRetVirtAddrPT, UINT32 *apRetPhysAddrPT);
void    KernArch_InvalidateTlbPageOnThisCore(UINT32 aVirtAddr);
void    KernArch_LaunchCpuCores(void);
UINT32* KernArch_Translate(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32* apRetPDE, BOOL *apRetPtPresent, UINT32 *apRetPte, UINT32 *apRetMemPageAttr);
void    KernArch_InstallPageTable(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddrPtMaps, UINT32 aPhysPageAddr);

void    KernMap_MakeOnePresentPage(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 aPhysAddr, UINTN aPageMapAttr);
UINT32  KernMap_BreakOnePage(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 aNpFlags);

UINT32  KernDbg_FindClosestSymbol(K2OSKERN_OBJ_PROCESS * apCurProc, UINT32 aAddr, char *apRetSymName, UINT32 aRetSymNameBufLen);
UINT32  KernDbg_OutputWithArgs(char const *apFormat, VALIST aList);

void    KernPhys_Init(void);

void    KernProc_Init(void);

void    KernCpu_Init(void);
void    __attribute__((noreturn)) KernCpu_Exec(K2OSKERN_CPUCORE volatile *apThisCore);

void    KernUser_Init(void);

/* --------------------------------------------------------------------------------- */

#endif // __KERN_H