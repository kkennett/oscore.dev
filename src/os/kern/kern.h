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
#include <lib/k2heap.h>
#include <lib/k2ramheap.h>
#include <lib/k2bit.h>

/* --------------------------------------------------------------------------------- */

//
// sanity checks
//
#if K2_TARGET_ARCH_IS_ARM
K2_STATIC_ASSERT(K2OSKERN_PTE_PRESENT_BIT == A32_PTE_PRESENT);
K2_STATIC_ASSERT(K2OS_KVA_ARCHSPEC_BASE == A32_HIGH_VECTORS_ADDRESS);
#else
K2_STATIC_ASSERT(K2OSKERN_PTE_PRESENT_BIT == X32_PTE_PRESENT);
#endif

/* --------------------------------------------------------------------------------- */

typedef struct _K2OSKERN_CPUCORE_EVENT      K2OSKERN_CPUCORE_EVENT;
typedef struct _K2OSKERN_CPUCORE            K2OSKERN_CPUCORE;
typedef struct _K2OSKERN_COREPAGE           K2OSKERN_COREPAGE;

typedef struct _K2OSKERN_SCHED_TIMERITEM    K2OSKERN_SCHED_TIMERITEM;
typedef struct _K2OSKERN_SCHED_WAITENTRY    K2OSKERN_SCHED_WAITENTRY;
typedef struct _K2OSKERN_SCHED_MACROWAIT    K2OSKERN_SCHED_MACROWAIT;
typedef struct _K2OSKERN_SCHED_ITEM         K2OSKERN_SCHED_ITEM;
typedef union  _K2OSKERN_SCHED_ITEM_ARGS    K2OSKERN_SCHED_ITEM_ARGS;

typedef struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_EXIT        K2OSKERN_SCHED_ITEM_ARGS_THREAD_EXIT;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_WAIT        K2OSKERN_SCHED_ITEM_ARGS_THREAD_WAIT;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_CONTENDED_CRITSEC  K2OSKERN_SCHED_ITEM_ARGS_CONTENDED_CRITSEC;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_INVALIDATE_TLB     K2OSKERN_SCHED_ITEM_ARGS_INVALIDATE_TLB;

typedef struct _K2OSKERN_CRITSEC            K2OSKERN_CRITSEC;

typedef struct _K2OSKERN_OBJ_HEADER         K2OSKERN_OBJ_HEADER;

typedef struct _K2OSKERN_OBJ_SEGMENT        K2OSKERN_OBJ_SEGMENT;
typedef struct _K2OSKERN_OBJ_DLX            K2OSKERN_OBJ_DLX;
typedef struct _K2OSKERN_OBJ_PROCESS        K2OSKERN_OBJ_PROCESS;
typedef struct _K2OSKERN_OBJ_EVENT          K2OSKERN_OBJ_EVENT;
typedef struct _K2OSKERN_OBJ_NAME           K2OSKERN_OBJ_NAME;
typedef struct _K2OSKERN_OBJ_THREAD         K2OSKERN_OBJ_THREAD;

typedef struct _K2OSKERN_PHYSTRACK_PAGE     K2OSKERN_PHYSTRACK_PAGE;
typedef struct _K2OSKERN_PHYSTRACK_FREE     K2OSKERN_PHYSTRACK_FREE;

/* --------------------------------------------------------------------------------- */

typedef enum _KernCpuCoreEventType KernCpuCoreEventType;
enum _KernCpuCoreEventType
{
    KernCpuCoreEvent_None = 0,
    KernCpuCoreEvent_SchedulerCall,
    KernCpuCoreEvent_SchedTimerFired,
    KernCpuCoreEvent_Ici_Wakeup,
    KernCpuCoreEvent_Ici_Stop,
    KernCpuCoreEvent_Ici_TlbInvPage,
    KernCpuCoreEvent_Ici_PageDirUpdate,
    KernCpuCoreEvent_Ici_Panic,
    KernCpuCoreEvent_Ici_Debug,

    KernCpuCoreEventType_Count
};

struct _K2OSKERN_CPUCORE_EVENT
{
    KernCpuCoreEventType                mEventType;
    UINT32                              mSrcCoreIx;
    UINT64                              mEventAbsTimeMs;
    K2OSKERN_CPUCORE_EVENT volatile *   mpNext;
};

typedef struct _K2OSKERN_SCHED_CPUCORE K2OSKERN_SCHED_CPUCORE;
struct _K2OSKERN_SCHED_CPUCORE
{
    K2OSKERN_OBJ_THREAD *   mpRunThread;
    UINT64                  mLastStopAbsTimeMs;
    K2LIST_LINK             PrioListLink;
    UINT32                  mActivePrio;
};

struct _K2OSKERN_CPUCORE
{
#if K2_TARGET_ARCH_IS_INTEL
    /* must be first thing in struct */
    X32_TSS                             TSS;
#endif
    UINT32                              mCoreIx;

    BOOL volatile                       mIsExecuting;
    BOOL                                mIsInMonitor;
    BOOL volatile                       mIsIdle;
    BOOL                                mGoToMonitorAfterIntr;

    K2OSKERN_OBJ_PROCESS *              mpActiveProc;
    K2OSKERN_OBJ_THREAD * volatile      mpActiveThread;

    K2OSKERN_SCHED_CPUCORE              Sched;

    K2OSKERN_CPUCORE_EVENT * volatile   mpPendingEventListHead;

    K2OSKERN_CPUCORE_EVENT volatile     IciFromOtherCore[K2OS_MAX_CPU_COUNT];
};

#define K2OSKERN_COREPAGE_STACKS_BYTES  (K2_VA32_MEMPAGE_BYTES - sizeof(K2OSKERN_CPUCORE))

struct _K2OSKERN_COREPAGE
{
    K2OSKERN_CPUCORE    CpuCore;  /* must be first thing in struct */
    UINT8               mStacks[K2OSKERN_COREPAGE_STACKS_BYTES];
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_COREPAGE) == K2_VA32_MEMPAGE_BYTES);

#define K2OSKERN_COREIX_TO_CPUCORE(x)   ((K2OSKERN_CPUCORE *)(K2OS_KVA_COREPAGES_BASE + ((x) * K2_VA32_MEMPAGE_BYTES)))
#define K2OSKERN_GET_CURRENT_CPUCORE    K2OSKERN_COREIX_TO_CPUCORE(K2OSKERN_GetCpuIndex())

#define K2OSKERN_COREIX_TO_COREPAGE(x)  ((K2OSKERN_COREPAGE *)(K2OS_KVA_COREPAGES_BASE + ((x) * K2_VA32_MEMPAGE_BYTES)))
#define K2OSKERN_GET_CURRENT_COREPAGE   K2OSKERN_COREIX_TO_COREPAGE(K2OSKERN_GetCpuIndex())

/* --------------------------------------------------------------------------------- */

#define K2OSKERN_OBJ_FLAG_PERMANENT     0x80000000

struct _K2OSKERN_OBJ_HEADER
{
    K2OS_ObjectType     mObjType;
    UINT32              mObjFlags;
    INT32 volatile      mRefCount;
    K2LIST_ANCHOR       WaitingThreadsPrioList;
};

/* --------------------------------------------------------------------------------- */

typedef enum _KernSchedItemType KernSchedItemType;
enum _KernSchedItemType
{
    KernSchedItem_Invalid = 0,
    KernSchedItem_SchedTimer,
    KernSchedItem_EnterDebug,
    KernSchedItem_ThreadExit,
    KernSchedItem_ThreadWait,
    KernSchedItem_Contended_Critsec,
    KernSchedItem_Destroy_Critsec,
    // more here
    KernSchedItemType_Count
};

struct _K2OSKERN_SCHED_TIMERITEM
{
    UINT16                      mIsSchedTimerItem;
    UINT16                      mOnQueue;
    K2OSKERN_SCHED_TIMERITEM *  mpNext;
    UINT64                      mAbsTimeMs;
};

struct _K2OSKERN_SCHED_WAITENTRY
{
    UINT8                   mMacroIndex;        /* my index inside my MACROWAIT - allows calc of MACROWAIT address */
    UINT8                   mUnused1;
    UINT16                  mStickyPulseStatus;
    K2LIST_LINK             WaitPrioListLink;
    K2OSKERN_OBJ_HEADER *   mpObj;
};

struct _K2OSKERN_SCHED_MACROWAIT
{
    K2OSKERN_OBJ_THREAD *       mpWaitingThread;    /* thread that is using this macrowait */
    UINT32                      mNumEntries;
    BOOL                        mWaitAll;
    K2OSKERN_SCHED_TIMERITEM    SchedTimerItem;    /* of type 'MACROWAIT' (may be unused) */
    K2OSKERN_SCHED_WAITENTRY    SchedWaitEntry[1]; /* 'mNumEntries' entries */
};

struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_EXIT
{
    BOOL    mNormal;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_WAIT
{
    UINT32                      mTimeoutMs;
    K2OSKERN_SCHED_MACROWAIT *  mpMacroWait;
    UINT32                      mMacroResult;   // index in macro that caused thread release
};

struct _K2OSKERN_SCHED_ITEM_ARGS_CONTENDED_CRITSEC
{
    // thread can only trap on one Critsec at a time */
    // which is pThread->Sched.mpActionSec
    BOOL  mEntering;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_INVALIDATE_TLB
{
    K2OSKERN_OBJ_PROCESS *  mpProc;
    UINT32                  mVirtAddr;
    UINT32                  mPageCount;
};

union _K2OSKERN_SCHED_ITEM_ARGS
{
    K2OSKERN_SCHED_ITEM_ARGS_THREAD_EXIT        ThreadExit;      
    K2OSKERN_SCHED_ITEM_ARGS_THREAD_WAIT        ThreadWait;      
    K2OSKERN_SCHED_ITEM_ARGS_CONTENDED_CRITSEC  ContendedCritSec;
    K2OSKERN_SCHED_ITEM_ARGS_INVALIDATE_TLB     InvalidateTlb;
};

struct _K2OSKERN_SCHED_ITEM
{
    K2OSKERN_CPUCORE_EVENT volatile CpuCoreEvent;
    KernSchedItemType               mSchedItemType;
    K2OSKERN_SCHED_ITEM *           mpPrev;
    K2OSKERN_SCHED_ITEM *           mpNext;
    UINT32                          mResult;
    K2OSKERN_SCHED_ITEM_ARGS        Args;
};

#define K2OS_THREAD_STATE_INIT  0x1000

typedef struct _K2OSKERN_SCHED_THREAD K2OSKERN_SCHED_THREAD;
struct _K2OSKERN_SCHED_THREAD
{
    K2OS_ThreadState    mState;
    UINT32              mBasePrio;
    UINT32              mAffinity;
    UINT64              mLastAbsTimeMs;
    UINT32              mRechargeQuantum;
    UINT32              mQuantumLeft;
    K2OSKERN_SCHED_ITEM Item;
    K2OSKERN_CRITSEC *  mpActionSec;
    UINT32              mActivePrio;
    K2LIST_LINK         PrioListLink;
    K2LIST_ANCHOR       OwnedCritSecList;
};

typedef struct _K2OSKERN_SCHED K2OSKERN_SCHED;
struct _K2OSKERN_SCHED
{
    K2LIST_ANCHOR                   CpuCorePrioList;
    K2LIST_ANCHOR                   ReadyThreadPrioList;

    UINT64                          mAbsTimeMs;

    UINT32 volatile                 mReq;
    K2OSKERN_SCHED_ITEM * volatile  mpPendingItemListHead;

    K2OSKERN_SCHED_TIMERITEM        SchedTimerSchedTimerItem;
    K2OSKERN_SCHED_ITEM             SchedTimerSchedItem;
    K2OSKERN_SCHED_TIMERITEM *      mpTimerItemQueue;

    K2OSKERN_CPUCORE *              mpSchedulingCore;
    
    K2OSKERN_SCHED_ITEM *           mpActiveItem;
    K2OSKERN_OBJ_THREAD *           mpActiveItemThread;
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_CRITSEC
{
    // high 16 of lockval is count of threads entering
    // low 16 is id of current owner
    // only atomic operationg are used to change this value
    UINT32 volatile         mLockVal;                
    UINT32                  mRecursionCount;         
    K2OSKERN_OBJ_THREAD *   mpOwner;
    K2LIST_LINK             ThreadOwnedCritSecLink;  
    K2LIST_ANCHOR           WaitingThreadPrioList;   
    UINT32                  mSentinel;
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_CRITSEC) <= K2OS_MAX_CACHELINE_BYTES);

/* --------------------------------------------------------------------------------- */

//
// these coexist in the same UINT32 with K2OS_MEMPAGE_ATTR_xxx
//
#define K2OS_SEG_ATTR_TYPE_DLX_PART     0x00010000
#define K2OS_SEG_ATTR_TYPE_PROCESS      0x00020000
#define K2OS_SEG_ATTR_TYPE_THREAD       0x00030000
#define K2OS_SEG_ATTR_TYPE_HEAP_TRACK   0x00040000
#define K2OS_SEG_ATTR_TYPE_USER         0x00050000
#define K2OS_SEG_ATTR_TYPE_DEVMAP       0x00060000
#define K2OS_SEG_ATTR_TYPE_PHYSBUF      0x00070000
#define K2OS_SEG_ATTR_TYPE_DLX_PAGE     0x00080000
#define K2OS_SEG_ATTR_TYPE_COUNT        0x00090000
#define K2OS_SEG_ATTR_TYPE_MASK         0x000F0000

typedef struct _K2OSKERN_SEGMENT_INFO_THREADSTACK K2OSKERN_SEGMENT_INFO_THREADSTACK;
struct _K2OSKERN_SEGMENT_INFO_THREADSTACK
{
    K2OSKERN_OBJ_THREAD *   mpThread;
};

typedef struct _K2OSKERN_SEGMENT_INFO_PROCESS K2OSKERN_SEGMENT_INFO_PROCESS;
struct _K2OSKERN_SEGMENT_INFO_PROCESS
{
    K2OSKERN_OBJ_PROCESS *  mpProc;
};

typedef struct _K2OSKERN_SEGMENT_INFO_DLX_PART K2OSKERN_SEGMENT_INFO_DLX_PART;
struct _K2OSKERN_SEGMENT_INFO_DLX_PART
{
    K2OSKERN_OBJ_DLX *      mpDlxObj;
    UINT32                  mSegmentIndex;
    DLX_SEGMENT_INFO        DlxSegmentInfo; // 5 * sizeof(UINT32)
};

typedef struct _K2OSKERN_SEGMENT_INFO_DEVICEMAP K2OSKERN_SEGMENT_INFO_DEVICEMAP;
struct _K2OSKERN_SEGMENT_INFO_DEVICEMAP
{
    UINT32                  mEfiMapIndex;
};

typedef struct _K2OSKERN_SEGMENT_INFO_PHYSBUF K2OSKERN_SEGMENT_INFO_PHYSBUF;
struct _K2OSKERN_SEGMENT_INFO_PHYSBUF
{
    UINT32                  mPhysAddr;
};

typedef struct _K2OSKERN_SEGMENT_INFO_USER K2OSKERN_SEGMENT_INFO_USER;
struct _K2OSKERN_SEGMENT_INFO_USER
{
    K2OSKERN_OBJ_PROCESS *  mpProc;
};

typedef struct _K2OSKERN_SEGMENT_INFO_DLX_PAGE K2OSKERN_SEGMENT_INFO_DLX_PAGE;
struct _K2OSKERN_SEGMENT_INFO_DLX_PAGE
{
    K2OSKERN_OBJ_DLX *      mpDlxObj;
};

typedef union _K2OSKERN_SEGMENT_INFO K2OSKERN_SEGMENT_INFO;
union _K2OSKERN_SEGMENT_INFO
{
    K2OSKERN_SEGMENT_INFO_DLX_PAGE      DlxPage;
    K2OSKERN_SEGMENT_INFO_DLX_PART      DlxPart;
    K2OSKERN_SEGMENT_INFO_PROCESS       Process;
    K2OSKERN_SEGMENT_INFO_THREADSTACK   ThreadStack;
    K2OSKERN_SEGMENT_INFO_DEVICEMAP     DeviceMap;
    K2OSKERN_SEGMENT_INFO_PHYSBUF       PhysBuf;
    K2OSKERN_SEGMENT_INFO_USER          User;
};

struct _K2OSKERN_OBJ_SEGMENT
{
    K2OSKERN_OBJ_HEADER     Hdr;
    K2TREE_NODE             SegTreeNode;    // base address is tree node UserVal
    UINT32                  mPagesBytes;
    UINT32                  mSegAndMemPageAttr;
    K2OSKERN_SEGMENT_INFO   Info;
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_DLX
{
    K2OSKERN_OBJ_HEADER     Hdr;
    DLX *                   mpDlx;
    K2OSKERN_OBJ_SEGMENT    PageSeg;
    K2OSKERN_OBJ_SEGMENT    SegObj[DlxSeg_Count];
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_PROCESS
{
    K2OSKERN_OBJ_HEADER     Hdr;
    UINT32                  mId;
    UINT32                  mTransTableKVA;     // VIRT of root trans table
    UINT32                  mTransTableRegVal;  // PHYS(+) of root trans table
    UINT32                  mVirtMapKVA;        // VIRT of PTE array
    K2OSKERN_OBJ_SEGMENT    SegObjSelf;
    K2OSKERN_OBJ_SEGMENT    SegObjPrimaryThreadStack;
    K2OSKERN_OBJ_DLX        PrimaryModule;

    K2OS_CRITSEC            ThreadListSec;
    K2LIST_ANCHOR           ThreadList;

    K2OS_CRITSEC            TlsMaskSec;
    UINT32                  mTlsMask;

    K2OSKERN_SEQLOCK        SegTreeSeqLock;
    K2TREE_ANCHOR           SegTree;

    K2LIST_LINK             ProcListLink;
};

#define gpProc0 ((K2OSKERN_OBJ_PROCESS * const)K2OS_KVA_PROC0_BASE)

/* --------------------------------------------------------------------------------- */

typedef struct _K2OSKERN_THREAD_ENV K2OSKERN_THREAD_ENV;
struct _K2OSKERN_THREAD_ENV
{
    UINT32  mId;
    K2STAT  mLastStatus;
    UINT32  mTlsValue[32];       // same # of bits as UINT32, since that is TlsMask in process
};

struct _K2OSKERN_OBJ_THREAD
{
    K2OSKERN_OBJ_HEADER         Hdr;

    K2OSKERN_OBJ_PROCESS *      mpProc;
    K2LIST_LINK                 ProcThreadListLink;

    K2OSKERN_OBJ_SEGMENT *      mpStackSeg;

    K2OS_THREADINFO             Info;

    BOOL                        mIsKernelThread;
    BOOL                        mIsInKernelMode;

    K2OSKERN_THREAD_ENV         Env;

    K2OSKERN_SCHED_THREAD       Sched;

    UINT32                      mStackPtr_Kernel;
    UINT32                      mStackPtr_User;

    K2LIST_ANCHOR               WorkPages_Dirty;
    K2LIST_ANCHOR               WorkPages_Clean;
    K2LIST_ANCHOR               WorkPtPages_Dirty;
    K2LIST_ANCHOR               WorkPtPages_Clean;
    UINT32                      mWorkVirt_Range;
    UINT32                      mWorkVirt_PageCount;

    UINT32                      mWorkMapAddr;
    UINT32                      mWorkMapAttr;
    K2OSKERN_PHYSTRACK_PAGE *   mpWorkPage;
    K2OSKERN_PHYSTRACK_PAGE *   mpWorkPtPage;

    K2_EXCEPTION_TRAP *         mpKernExTrapStack;
};

#define K2OSKERN_THREAD_KERNSTACK_BYTECOUNT (K2_VA32_MEMPAGE_BYTES - sizeof(K2OSKERN_OBJ_THREAD))
typedef struct _K2OSKERN_THREAD_PAGE K2OSKERN_THREAD_PAGE;
struct _K2OSKERN_THREAD_PAGE
{
    UINT8               mKernStack[K2OSKERN_THREAD_KERNSTACK_BYTECOUNT];
    K2OSKERN_OBJ_THREAD Thread;
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_THREAD_PAGE) == K2_VA32_MEMPAGE_BYTES);

#define K2OSKERN_THREAD_FROM_THREAD_PAGE(addr)      (&((K2OSKERN_THREAD_PAGE *)(addr))->Thread)
#define K2OSKERN_THREAD_PAGE_FROM_THREAD(pThread)   K2_GET_CONTAINER(K2OSKERN_THREAD_PAGE, pThread, Thread)

#define gpThread0 (&((K2OSKERN_THREAD_PAGE *)(K2OS_KVA_THREAD0_AREA_HIGH - K2_VA32_MEMPAGE_BYTES))->Thread)

#if K2_TARGET_ARCH_IS_ARM
#define K2OSKERN_CURRENT_THREAD  ((K2OSKERN_OBJ_THREAD *)A32_ReadTPIDRPRW())
#elif K2_TARGET_ARCH_IS_INTEL
#define K2OSKERN_CURRENT_THREAD  ((K2OSKERN_OBJ_THREAD *)X32_GetFSData(0))
#else
#error !!!Unsupported Architecture
#endif

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_NAME
{
    K2OSKERN_OBJ_HEADER     Hdr;
    char                    NameBuffer[K2OS_NAME_MAX_LEN];
    K2OSKERN_OBJ_PROCESS *  mpOwnerProc;
    K2OSKERN_OBJ_HEADER *   mpNamedObject;
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_EVENT
{
    K2OSKERN_OBJ_HEADER     Hdr;
    BOOL                    mIsAutoReset;
    BOOL                    mIsSignaled;
};

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
    KernPhysPageList_Non_KText,     //  8       TEXT, EFI_Run_Code
    KernPhysPageList_Non_KRead,     //  9       READ
    KernPhysPageList_Non_KData,     //  10      DATA, DLX, LOADER, EFI_Run_Data
    KernPhysPageList_Non_UText,     //  11
    KernPhysPageList_Non_URead,     //  12
    KernPhysPageList_Non_UData,     //  13
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

#define K2OSKERN_VMNODE_TYPE_SEGMENT                0       // MUST BE TYPE ZERO
#define K2OSKERN_VMNODE_TYPE_RESERVED               1
#define K2OSKERN_VMNODE_TYPE_UNKNOWN                2
#define K2OSKERN_VMNODE_TYPE_SPARE                  3

#define K2OSKERN_VMNODE_RESERVED_ERROR              0
#define K2OSKERN_VMNODE_RESERVED_PUBLICAPI          1
#define K2OSKERN_VMNODE_RESERVED_ARCHSPEC           2
#define K2OSKERN_VMNODE_RESERVED_COREPAGES          3
#define K2OSKERN_VMNODE_RESERVED_VAMAP              4
#define K2OSKERN_VMNODE_RESERVED_PHYSTRACK          5
#define K2OSKERN_VMNODE_RESERVED_TRANSTAB           6
#define K2OSKERN_VMNODE_RESERVED_TRANSTAB_HOLE      7
#define K2OSKERN_VMNODE_RESERVED_EFIMAP             8
#define K2OSKERN_VMNODE_RESERVED_PTPAGECOUNT        9
#define K2OSKERN_VMNODE_RESERVED_LOADER             10
#define K2OSKERN_VMNODE_RESERVED_EFI_RUN_TEXT       11
#define K2OSKERN_VMNODE_RESERVED_EFI_RUN_DATA       12
#define K2OSKERN_VMNODE_RESERVED_DEBUG              13
#define K2OSKERN_VMNODE_RESERVED_FW_TABLES          14
#define K2OSKERN_VMNODE_RESERVED_UNKNOWN            15
#define K2OSKERN_VMNODE_RESERVED_EFI_MAPPED_IO      16
#define K2OSKERN_VMNODE_RESERVED_CORECLEARPAGES     17

typedef struct _K2OSKERN_VMNODE_NONSEG K2OSKERN_VMNODE_NONSEG;
struct _K2OSKERN_VMNODE_NONSEG
{
    UINT32  mType             : 2;      // 0 means segment and the mpSegment pointer will be valid
    UINT32  mNodeInfo         : 30;     // depends on value of mNodeIsNotSegment;
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_VMNODE_NONSEG) == sizeof(UINT32));

typedef struct _K2OSKERN_VMNODE K2OSKERN_VMNODE;
struct _K2OSKERN_VMNODE
{
    K2HEAP_NODE     HeapNode;
    union {
        K2OSKERN_OBJ_SEGMENT *  mpSegment;
        K2OSKERN_VMNODE_NONSEG  NonSeg;
    };
};

/* --------------------------------------------------------------------------------- */

typedef enum _KernPhys_Disp KernPhys_Disp;
enum _KernPhys_Disp
{
    KernPhys_Disp_Uncached = 0,
    KernPhys_Disp_Cached_WriteThrough,
    KernPhys_Disp_Cached,
    // goes last
    KernPhys_Disp_Count
};

typedef struct _K2OSKERN_HEAPTRACKPAGE K2OSKERN_HEAPTRACKPAGE;

#define TRACK_BYTES     (K2_VA32_MEMPAGE_BYTES - (sizeof(K2OSKERN_OBJ_SEGMENT) + sizeof(K2OSKERN_HEAPTRACKPAGE *)))
#define TRACK_PER_PAGE  (TRACK_BYTES / sizeof(K2OSKERN_VMNODE))

struct _K2OSKERN_HEAPTRACKPAGE
{
    K2OSKERN_OBJ_SEGMENT        SegObj;
    K2OSKERN_HEAPTRACKPAGE *    mpNextPage;
    UINT8                       TrackSpace[TRACK_BYTES];
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_HEAPTRACKPAGE) == K2_VA32_MEMPAGE_BYTES);

/* --------------------------------------------------------------------------------- */

typedef enum _KernInitStage KernInitStage;
enum _KernInitStage
{
    KernInitStage_dlx_entry = 0,
/*temp to get debug */  KernInitStage_At_Hal_Entry,
    KernInitStage_Before_Virt,
    KernInitStage_After_Virt,
    KernInitStage_Before_Hal,
//    KernInitStage_At_Hal_Entry,
    KernInitStage_After_Hal,
    KernInitStage_Before_Launch_Cores,

// should be last entry
    KernInitStage_Threaded,
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
    K2OSKERN_SEQLOCK                    ProcListSeqLock;
    K2LIST_ANCHOR                       ProcList;

    //
    // physical memory (PHYSTRACK)
    //
    K2OSKERN_SEQLOCK                    PhysMemSeqLock;
    K2TREE_ANCHOR                       PhysFreeTree;
    K2LIST_ANCHOR                       PhysPageList[KernPhysPageList_Count];

    //
    // virtual memory (allocation, not mapping)
    //
    K2OS_CRITSEC                        KernVirtHeapSec;    // virt memory allocated, searched, or freed
    K2HEAP_ANCHOR                       KernVirtHeap;
    K2OSKERN_SEQLOCK                    KernVirtMapLock;    // pagetables/pagedir changed

    //
    // ramheap and tracking
    //
    K2OS_RAMHEAP                        RamHeap;            // heap
    K2LIST_ANCHOR                       HeapTrackFreeList;  // list of free ramheap tracking structures
    K2OSKERN_HEAPTRACKPAGE *            mpTrackPages;       // list of ramheap tracking structure pages
    K2OSKERN_HEAPTRACKPAGE *            mpNextTrackPage;    // free ramheap tracking structure page, all ready to go

    // dlxsupp.c - 
    K2OSKERN_OBJ_DLX                    DlxCrt;
    K2OSKERN_OBJ_DLX                    DlxHal;
    K2OSKERN_OBJ_DLX                    DlxAcpi;
    // kernel dlx is in gpProc0 process object

    // sched.c
    K2OSKERN_SCHED                      Sched;

    // debugger
    BOOL                                mDebuggerActive;

    // arch specific
#if K2_TARGET_ARCH_IS_ARM
    UINT32                              mA32VectorPagePhys;
    UINT32                              mA32StartAreaPhys;      // K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT
#endif
#if K2_TARGET_ARCH_IS_INTEL
#endif
};

extern KERN_DATA gData;

/* --------------------------------------------------------------------------------- */

void   K2_CALLCONV_REGS KernEx_Assert(char const * apFile, int aLineNum, char const * apCondition);
BOOL   K2_CALLCONV_REGS KernEx_TrapMount(K2_EXCEPTION_TRAP *apTrap);
K2STAT K2_CALLCONV_REGS KernEx_TrapDismount(K2_EXCEPTION_TRAP *apTrap);
void   K2_CALLCONV_REGS KernEx_RaiseException(K2STAT aExceptionCode);

UINT32 KernDebug_OutputWithArgs(char const *apFormat, VALIST aList);
void   KernPanic_Ici(K2OSKERN_CPUCORE* apThisCore);

/* --------------------------------------------------------------------------------- */

K2STAT KernDlx_CritSec(BOOL aEnter);
K2STAT KernDlx_Open(char const * apDlxName, UINT32 aDlxNameLen, K2DLXSUPP_OPENRESULT *apRetResult);
void   KernDlx_AtReInit(DLX *apDlx, UINT32 aModulePageLinkAddr, K2DLXSUPP_HOST_FILE *apInOutHostFile);
K2STAT KernDlx_ReadSectors(K2DLXSUPP_HOST_FILE aHostFile, void *apBuffer, UINT32 aSectorCount);
K2STAT KernDlx_Prepare(K2DLXSUPP_HOST_FILE aHostFile, DLX_INFO *apInfo, UINT32 aInfoSize, BOOL aKeepSymbols, K2DLXSUPP_SEGALLOC *apRetAlloc);
BOOL   KernDlx_PreCallback(K2DLXSUPP_HOST_FILE aHostFile, BOOL aIsLoad);
K2STAT KernDlx_PostCallback(K2DLXSUPP_HOST_FILE aHostFile, K2STAT aUserStatus);
K2STAT KernDlx_Finalize(K2DLXSUPP_HOST_FILE aHostFile, K2DLXSUPP_SEGALLOC *apUpdateAlloc);
K2STAT KernDlx_Purge(K2DLXSUPP_HOST_FILE aHostFile);

/* --------------------------------------------------------------------------------- */

void   KernMem_Start(void);

UINT32 KernMem_CountPT(UINT32 aVirtAddr, UINT32 aPageCount);

K2STAT KernMem_VirtAllocToThread(UINT32 aUseAddr, UINT32 aPageCount, BOOL aTopDown);
void   KernMem_VirtFreeFromThread(void);

K2STAT KernMem_PhysAllocToThread(UINT32 aPageCount, KernPhys_Disp aDisp, BOOL aForPageTables);
void   KernMem_PhysFreeFromThread(void);

K2STAT KernMem_CreateSegmentFromThread(K2OSKERN_OBJ_SEGMENT *apSrc, K2OSKERN_OBJ_SEGMENT *apDst);

/* --------------------------------------------------------------------------------- */

// these return virtual address of PTE changed
UINT32 KernMap_MakeOnePage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aPhysAddr, UINTN aMapType);
UINT32 KernMap_BreakOnePage(UINT32 aVirtMapBase, UINT32 aVirtAddr);
void   KernMap_FromThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apPhysPageOwner, KernPhysPageList aTargetList);

/* --------------------------------------------------------------------------------- */

UINT32  KernArch_MakePTE(UINT32 aPhysAddr, UINT32 aPageMapAttr);
void    KernArch_MapPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aPhysAddr, UINT32 aPageMapAttr);
void    KernArch_MapPageTable(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 aPhysAddrPT);
void    KernArch_BreakMapPageTable(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, UINT32 *apRetVirtAddrPT, UINT32 *apRetPhysAddrPT);
void    KernArch_InvalidateTlbPageOnThisCore(UINT32 aVirtAddr);
BOOL    KernArch_VerifyPteKernAccessAttr(UINT32 aPTE, UINT32 aMustHaveAttr);
UINT32 *KernArch_Translate(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, BOOL *apRetPtPresent, UINT32 *apRetPte, UINT32 *apRetAccessAttr);
void    KernArch_PrepareThread(K2OSKERN_OBJ_THREAD *apThread, UINT32 aUserModeStackPtr);
void    KernArch_LaunchCores(void);
void    KernArch_ThreadCallSched(void);
void    KernArch_MonitorSwitchToProcZero(K2OSKERN_CPUCORE *apThisCore);
void    KernArch_SwitchFromMonitorToThread(K2OSKERN_CPUCORE *apThisCore);
void    KernArch_SendIci(UINT32 aCurCoreIx, BOOL aSendToSpecific, UINT32 aTargetCpuIx);
void    KernArch_ArmSchedTimer(UINT32 aMsFromNow);

/* --------------------------------------------------------------------------------- */

void    KernSegment_Dump(K2OSKERN_OBJ_SEGMENT *apSeg);

/* --------------------------------------------------------------------------------- */

void    KernProc_Dump(K2OSKERN_OBJ_PROCESS *apProc);

/* --------------------------------------------------------------------------------- */

void                  KernThread_Dump(K2OSKERN_OBJ_THREAD *apThread);
void K2_CALLCONV_REGS KernThread_Entry(K2OSKERN_OBJ_THREAD *apThisThread);

/* --------------------------------------------------------------------------------- */

void KernSched_AddCurrentCore(void);
void KernSched_Check(K2OSKERN_CPUCORE *apThisCore);
void KernSched_CallFromThread(K2OSKERN_CPUCORE *apThisCore);
void KernSched_Exec_ThreadExit(void);
void KernSched_Exec_ThreadWait(void);
void KernSched_Exec_Contended_CritSec(void);
void KernSched_Exec_EnterDebug(void);
BOOL KernSched_TimePassedUntil(UINT64 aTimeNow);
void KernSched_TimerFired(K2OSKERN_CPUCORE *apThisCore);

/* --------------------------------------------------------------------------------- */

void KernCpuCore_DrainEvents(K2OSKERN_CPUCORE *apThisCore);
void KernCpuCore_SendIciToOneCore(K2OSKERN_CPUCORE *apThisCore, UINT32 aTargetCoreIx, KernCpuCoreEventType aEventType);
void KernCpuCore_SendIciToAllOtherCores(K2OSKERN_CPUCORE *apThisCore, KernCpuCoreEventType aEventType);

/* --------------------------------------------------------------------------------- */

void KernMontior_OneTimeInit(void);
void KernMonitor_Run(void);

/* --------------------------------------------------------------------------------- */

BOOL KernDebug_Service(BOOL aExecutedSchedItems);

/* --------------------------------------------------------------------------------- */

void K2_CALLCONV_REGS KernExec(void);

void KernInit_Stage(KernInitStage aStage);
void KernInit_Arch(void);
void KernInit_DlxHost(void);
void KernInit_Mem(void);
void KernInit_Process(void);
void KernInit_Thread(void);
void KernInit_Sched(void);
void KernInit_Hal(void);
void KernInit_CpuCore(void);

/* --------------------------------------------------------------------------------- */

void KernIntr_QueueCpuCoreEvent(K2OSKERN_CPUCORE * apThisCore, K2OSKERN_CPUCORE_EVENT volatile * apCoreEvent);
void KernIntr_RecvIrq(K2OSKERN_CPUCORE *apThisCore, UINT32 aIrqNum);
void KernIntr_RecvIci(K2OSKERN_CPUCORE *apThisCore, UINT32 aSendingCoreIx); 
void KernIntr_Exception(UINT32 aExceptionCode);

/* --------------------------------------------------------------------------------- */


#endif // __KERN_H