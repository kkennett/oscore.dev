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
#include <lib/k2elf32.h>
#include "../../shared/lib/k2dlxsupp/dlx_struct.h"
#include "syscallid.h"

/* --------------------------------------------------------------------------------- */

typedef struct _K2OSKERN_CPUCORE            K2OSKERN_CPUCORE;
typedef struct _K2OSKERN_COREMEMORY         K2OSKERN_COREMEMORY;
typedef struct _K2OSKERN_OBJ_HEADER         K2OSKERN_OBJ_HEADER;
typedef struct _K2OSKERN_OBJ_PROCESS        K2OSKERN_OBJ_PROCESS;
typedef struct _K2OSKERN_OBJ_THREAD         K2OSKERN_OBJ_THREAD;
typedef struct _K2OSKERN_OBJ_INTR           K2OSKERN_OBJ_INTR;
typedef struct _K2OSKERN_OBJ_NOTIFY         K2OSKERN_OBJ_NOTIFY;

typedef struct _K2OSKERN_PHYSTRACK_PAGE     K2OSKERN_PHYSTRACK_PAGE;
typedef struct _K2OSKERN_PHYSTRACK_FREE     K2OSKERN_PHYSTRACK_FREE;

/* --------------------------------------------------------------------------------- */

typedef enum _KernObj KernObj;
enum _KernObj
{
    KernObj_Error = 0,

    KernObj_Process,
    KernObj_Thread,
    KernObj_Notify,

    KernObj_Count
};

#define K2OSKERN_OBJ_FLAG_PERMANENT     0x80000000
#define K2OSKERN_OBJ_FLAG_EMBEDDED      0x40000000

typedef void (*K2OSKERN_pf_ObjCleanup)(K2OSKERN_OBJ_HEADER *apObj);

struct _K2OSKERN_OBJ_HEADER
{
    UINT32                  mObjType;
    UINT32                  mObjFlags;
    union
    {
        INT32 volatile                  mRefCount;
        K2OSKERN_OBJ_HEADER * volatile  mpNextCleanup;
    };
    K2OSKERN_pf_ObjCleanup  mfCleanup;
    K2TREE_NODE             ObjTreeNode;
};

/* --------------------------------------------------------------------------------- */

typedef enum _KernIciCode KernIciCode;
enum _KernIciCode
{
    KernIciCode_None,               // 0
    KernIciCode_Migrated_Thread,    // 1

    KernIciCode_Count               // 2
};

typedef struct _K2OSKERN_OUT_ICI K2OSKERN_OUT_ICI;
struct _K2OSKERN_OUT_ICI
{
    UINT32          mTargetCoreMask;
    KernIciCode     mCode;
    K2LIST_LINK     ListLink;
};

/* --------------------------------------------------------------------------------- */

typedef enum _KernThreadState KernThreadState;
enum _KernThreadState
{
    KernThreadState_None = 0,
    KernThreadState_Init,
    KernThreadState_OnCoreRunList,
    KernThreadState_WaitingOnNotify,
    KernThreadState_Migrating
};

#if K2_TARGET_ARCH_IS_INTEL
typedef struct _K2OSKERN_THREAD_CONTEXT  K2OSKERN_THREAD_CONTEXT;
struct _K2OSKERN_THREAD_CONTEXT
{
    UINT32      DS;                     // last pushed by int common
    X32_PUSHA   REGS;                   // pushed by int common - 8 32-bit words
    UINT32      Exception_Vector;       // pushed by int stub
    UINT32      Exception_ErrorCode;    // pushed auto by CPU or by int stub
    UINT32      EIP;                    // pushed auto by CPU  (iret instruction starts restore from here)
    UINT32      CS;                     // pushed auto by CPU
    UINT32      EFLAGS;                 // pushed auto by CPU
    UINT32      ESP;                    // pushed auto by CPU : only present when CS = USER
    UINT32      SS;                     // pushed first, auto by CPU : only present when CS = USER
};
#elif K2_TARGET_ARCH_IS_ARM
typedef struct _K2OSKERN_THREAD_CONTEXT  K2OSKERN_THREAD_CONTEXT;
struct _K2OSKERN_THREAD_CONTEXT
{
    UINT32      SPSR;
    UINT32      R[16];
};
#else
#error Unknown Arch
#endif

struct _K2OSKERN_OBJ_THREAD
{
    K2OSKERN_OBJ_HEADER     Hdr;

    K2OSKERN_OBJ_PROCESS *  mpProc;

    UINT32                  mIx;
    KernThreadState         mState;

    UINT32                  mTlsPagePhys;
    UINT32 *                mpTlsPage;

    K2LIST_LINK             ProcThreadListLink;

    UINT32                  mAffinityMask;

    K2OSKERN_THREAD_CONTEXT Context;

    UINT32                  mSysCall_Arg1;
    UINT32                  mSysCall_Arg2;
    UINT32                  mSysCall_Result;
    K2STAT                  mSysCall_Status;

    K2OSKERN_CPUCORE volatile * mpCurrentCore;
    K2LIST_LINK                 CpuRunListLink;
    K2OSKERN_CPUCORE volatile * mpLastRunCore;
    K2OSKERN_OBJ_THREAD *       mpMigratedNext;
    K2OSKERN_OUT_ICI            MigrateIci;

    K2LIST_LINK             NotifyWaitListLink;
};

#if K2_TARGET_ARCH_IS_ARM
#define K2OSKERN_CURRENT_THREAD_INDEX A32_ReadTPIDRPRW()
#elif K2_TARGET_ARCH_IS_INTEL
#define K2OSKERN_CURRENT_THREAD_INDEX X32_GetFSData(0)
#else
#error !!!Unsupported Architecture
#endif

static inline K2OSKERN_OBJ_THREAD *
K2OSKERN_GetThisCoreCurrentThread(
    void
)
{
    register UINT32 threadIx = K2OSKERN_CURRENT_THREAD_INDEX;
    return (threadIx == 0) ?
        NULL :
        ((K2OSKERN_OBJ_THREAD *)
            (*((UINT32 *)(K2OS_KVA_THREADPTRS_BASE +
                (sizeof(UINT32) * threadIx)))));
}

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_CPUCORE
{
#if K2_TARGET_ARCH_IS_INTEL
    /* must be first thing in struct */
    X32_TSS                         TSS;
#endif

    UINT32                          mCoreIx;

    BOOL                            mIsExecuting;
    BOOL                            mIsIdle;

    UINT32 volatile                 mIciFromOtherCore[K2OS_MAX_CPU_COUNT];
    K2LIST_ANCHOR                   IciOutList;

    K2LIST_ANCHOR                   RunList;
    K2OSKERN_OBJ_THREAD             IdleThread;
    BOOL                            mThreadChanged;
    K2OSKERN_OBJ_THREAD * volatile  mpMigratedHead;
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

typedef enum _KernNotifyState KernNotifyState;
enum _KernNotifyState
{
    KernNotifyState_Idle,
    KernNotifyState_Waiting,
    KernNotifyState_Active
};

struct _K2OSKERN_OBJ_NOTIFY
{
    K2OSKERN_OBJ_HEADER Hdr;
    K2OSKERN_SEQLOCK    Lock;
    struct
    {
        UINT32          mDataWord;
        KernNotifyState mState;
        K2LIST_ANCHOR   WaitingThreadList;
    } Locked;
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

    K2OSKERN_OBJ_THREAD     InitialThread;
    K2OSKERN_SEQLOCK        ThreadListSeqLock;
    K2LIST_ANCHOR           ThreadList;

    K2LIST_ANCHOR *         mpUserDlxList;   // points into user space k2oscrt.dlx data segment
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

//
// handler returns TRUE if the kernel should check the core
// for activity.  otherwise returns FALSE to return to the
// interrupted activity (thread or idle)
//
typedef BOOL (*K2OSKERN_pf_IntrHandler)(void *apContext);

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

typedef enum _KernCpuExecReason KernCpuExecReason;
enum _KernCpuExecReason
{
    KernCpuExecReason_Invalid,      // 0
    KernCpuExecReason_CoreStart,    // 1
    KernCpuExecReason_Exception,    // 2
    KernCpuExecReason_Ici,          // 3
    KernCpuExecReason_Irq,          // 4
    KernCpuExecReason_SystemCall,   // 5

    KernCpuExecReason_Count         // 6
};

typedef struct _KERN_USERCRT_INFO KERN_USERCRT_INFO;
struct _KERN_USERCRT_INFO
{
    UINT32          mPageTablesCount;
    UINT32          mTextPagesCount;
    UINT32          mReadPagesCount;
    UINT32          mDataPagesVirtAddr;
    UINT32          mDataPagesCount;
    UINT32          mSymPagesCount;
    UINT8 const *   mpDataSrc;
    UINT32          mDataSrcBytes;
    UINT32          mEntrypoint;
};

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

    UINT32 volatile         mNextThreadSlotIx;
    UINT32                  mLastThreadSlotIx;

    K2ROFS const *          mpROFS;

    K2OSKERN_SEQLOCK        ObjTreeSeqLock;
    K2TREE_ANCHOR           ObjTree;
    K2OSKERN_OBJ_HEADER * volatile mpFreedObjChain;
    // K2OSKERN_OBJ_SIGNAL  FreedObjSignal;

    //
    // init data for each process' user crt segment
    //
    KERN_USERCRT_INFO       UserCrtInfo;

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
void    KernArch_InstallPageTable(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddrPtMaps, UINT32 aPhysPageAddr, BOOL aZeroPageAfterMap);
void    KernArch_UserInit(void);
void    KernArch_FlushCache(UINT32 aVirtAddr, UINT32 aSizeBytes);
void    KernArch_ResumeThread(K2OSKERN_CPUCORE volatile * apThisCore);
void    KernArch_CpuIdle(K2OSKERN_CPUCORE volatile *apThisCore);
BOOL    KernArch_PollIrq(K2OSKERN_CPUCORE volatile *apThisCore);
void    KernArch_DumpThreadContext(K2OSKERN_OBJ_THREAD *apThread);
void    KernArch_SendIci(K2OSKERN_CPUCORE volatile *apThisCore, UINT32 aTargetMask);

void    KernMap_MakeOnePresentPage(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 aPhysAddr, UINTN aPageMapAttr);
UINT32  KernMap_BreakOnePage(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 aNpFlags);

void    KernDbg_EarlyInit(void);
void    KernDbg_FindClosestSymbol(K2OSKERN_OBJ_PROCESS * apCurProc, UINT32 aAddr, char *apRetSymName, UINT32 aRetSymNameBufLen);
UINT32  KernDbg_OutputWithArgs(char const *apFormat, VALIST aList);

void    KernPhys_Init(void);

void    KernProc_Init(void);
void    KernProc_InitOne(K2OSKERN_OBJ_PROCESS *apProc);

UINT32  KernSched_PickCoreForThread(K2OSKERN_OBJ_THREAD *apThread);

void    KernCpu_Init(void);
void    __attribute__((noreturn)) KernCpu_Exec(K2OSKERN_CPUCORE volatile *apThisCore, KernCpuExecReason aReason);
BOOL    KernCpu_ProcessIcis(K2OSKERN_CPUCORE volatile *apThisCore);
void    KernCpu_Reschedule(K2OSKERN_CPUCORE volatile *apThisCore);
void    KernCpu_LatchIciToSend(K2OSKERN_CPUCORE volatile * apThisCore, K2OSKERN_OUT_ICI *apIciOut);
void    KernCpu_MigrateThread(K2OSKERN_CPUCORE volatile *apThisCore, UINT32 aTargetCoreIx, K2OSKERN_OBJ_THREAD *apThread);

void    KernThread_InitOne(K2OSKERN_OBJ_THREAD *apThread);
void    KernThread_Exception(K2OSKERN_CPUCORE volatile *apThisCore);
void    KernThread_SystemCall(K2OSKERN_CPUCORE volatile *apThisCore);
void    KernThread_SysCall_SignalNotify(K2OSKERN_CPUCORE volatile *apThisCore, K2OSKERN_OBJ_THREAD *apCurThread);
void    KernThread_SysCall_WaitForNotify(K2OSKERN_CPUCORE volatile * apThisCore, K2OSKERN_OBJ_THREAD * apCurThread, BOOL aNonBlocking);

void    KernUser_Init(void);

BOOL    KernIntr_OnIci(K2OSKERN_CPUCORE volatile * apThisCore, UINT32 aSrcCoreIx);
BOOL    KernIntr_OnIrq(K2OSKERN_CPUCORE volatile * apThisCore, K2OSKERN_OBJ_INTR *apIntr);
BOOL    KernIntr_OnSystemCall(K2OSKERN_CPUCORE volatile * apThisCore, K2OSKERN_OBJ_THREAD * apCallingThread, UINT32 *apRetFastResult);

void    KernObj_Init(void);
void    KernObj_Add(K2OSKERN_OBJ_HEADER *apObjHdr);
UINT32  KernObj_AddRef(K2OSKERN_OBJ_HEADER *apObjHdr);
UINT32  KernObj_Release(K2OSKERN_OBJ_HEADER *apObjHdr);

void    KernNotify_InitOne(K2OSKERN_OBJ_NOTIFY *apNotify);
UINT32  KernNotify_Signal(K2OSKERN_CPUCORE volatile * apThisCore, K2OSKERN_OBJ_NOTIFY * apNotify, UINT32 aSignalBits);

/* --------------------------------------------------------------------------------- */

#endif // __KERN_H