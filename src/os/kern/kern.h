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
#include "kernexec.h"
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

typedef struct _K2OSKERN_CRITSEC            K2OSKERN_CRITSEC;

typedef struct _K2OSKERN_OBJ_HEADER         K2OSKERN_OBJ_HEADER;

typedef struct _K2OSKERN_OBJ_SEGMENT        K2OSKERN_OBJ_SEGMENT;
typedef struct _K2OSKERN_OBJ_DLX            K2OSKERN_OBJ_DLX;
typedef struct _K2OSKERN_OBJ_PROCESS        K2OSKERN_OBJ_PROCESS;
typedef struct _K2OSKERN_OBJ_EVENT          K2OSKERN_OBJ_EVENT;
typedef struct _K2OSKERN_OBJ_NAME           K2OSKERN_OBJ_NAME;
typedef struct _K2OSKERN_OBJ_THREAD         K2OSKERN_OBJ_THREAD;
typedef struct _K2OSKERN_OBJ_SEM            K2OSKERN_OBJ_SEM;
typedef struct _K2OSKERN_OBJ_INTR           K2OSKERN_OBJ_INTR;
typedef struct _K2OSKERN_OBJ_MAILBOX        K2OSKERN_OBJ_MAILBOX;
typedef struct _K2OSKERN_OBJ_MAILSLOT       K2OSKERN_OBJ_MAILSLOT;
typedef struct _K2OSKERN_OBJ_MSG            K2OSKERN_OBJ_MSG;

typedef struct _K2OSKERN_PHYSTRACK_PAGE     K2OSKERN_PHYSTRACK_PAGE;
typedef struct _K2OSKERN_PHYSTRACK_FREE     K2OSKERN_PHYSTRACK_FREE;

typedef union _K2OSKERN_OBJ_WAITABLE K2OSKERN_OBJ_WAITABLE;
union _K2OSKERN_OBJ_WAITABLE
{
    K2OSKERN_OBJ_HEADER *   mpHdr;
    K2OSKERN_OBJ_PROCESS *  mpProc;
    K2OSKERN_OBJ_EVENT *    mpEvent;
    K2OSKERN_OBJ_NAME *     mpName;
    K2OSKERN_OBJ_THREAD *   mpThread;
    K2OSKERN_OBJ_SEM *      mpSem;
    K2OSKERN_OBJ_INTR *     mpIntr;
    K2OSKERN_OBJ_MAILBOX *  mpMailbox;
    K2OSKERN_OBJ_MSG *      mpMsg;
};

/* --------------------------------------------------------------------------------- */

typedef enum _KernCpuCoreEventType KernCpuCoreEventType;
enum _KernCpuCoreEventType
{
    KernCpuCoreEvent_None = 0,
    KernCpuCoreEvent_SchedulerCall,
    KernCpuCoreEvent_SchedTimerFired,
    KernCpuCoreEvent_Ici_Wakeup,
    KernCpuCoreEvent_Ici_Stop,
    KernCpuCoreEvent_Ici_TlbInv,
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

#define K2OSKERN_SCHED_CPUCORE_EXECFLAG_CHANGED         0x80000000
#define K2OSKERN_SCHED_CPUCORE_EXECFLAG_QUANTUM_ZERO    0x00000001

typedef struct _K2OSKERN_SCHED_CPUCORE K2OSKERN_SCHED_CPUCORE;
struct _K2OSKERN_SCHED_CPUCORE
{
    UINT64                  mLastStopAbsTimeMs;
    K2OSKERN_OBJ_THREAD *   mpRunThread;
    K2LIST_LINK             CpuCoreListLink;
    UINT32                  mCoreActivePrio;
    UINT32                  mExecFlags;
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
    K2OSKERN_OBJ_THREAD * volatile      mpAssignThread;
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
#define K2OSKERN_OBJ_FLAG_EMBEDDED      0x40000000

struct _K2OSKERN_OBJ_HEADER
{
    K2OS_ObjectType     mObjType;
    UINT32              mObjFlags;
    INT32 volatile      mRefCount;
    K2TREE_NODE         ObjTreeNode;
    K2OSKERN_OBJ_NAME * mpName;
    K2LIST_ANCHOR       WaitEntryPrioList;
};

/* --------------------------------------------------------------------------------- */

typedef enum _KernSchedItemType KernSchedItemType;
enum _KernSchedItemType
{
    KernSchedItem_Invalid = 0,
    KernSchedItem_SchedTimer,
    KernSchedItem_EnterDebug,

    KernSchedItem_ThreadExit,
    KernSchedItem_ThreadWaitAny,
    KernSchedItem_PurgePT,
    KernSchedItem_InvalidateTlb,
    KernSchedItem_SemRelease,
    KernSchedItem_ThreadCreate,
    KernSchedItem_EventChange,
    KernSchedItem_SlotBlock,
    KernSchedItem_SlotBoxes,
    KernSchedItem_SlotPurge,
    KernSchedItem_MboxRecv,
    KernSchedItem_MboxRespond,
    KernSchedItem_MboxPurge,
    KernSchedItem_MsgSend,
    KernSchedItem_MsgAbort,
    KernSchedItem_MsgReadResp,

    // more here
    KernSchedItemType_Count
};

typedef struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_EXIT        K2OSKERN_SCHED_ITEM_ARGS_THREAD_EXIT;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_WAIT        K2OSKERN_SCHED_ITEM_ARGS_THREAD_WAIT;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_PURGE_PT           K2OSKERN_SCHED_ITEM_ARGS_PURGE_PT;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_INVALIDATE_TLB     K2OSKERN_SCHED_ITEM_ARGS_INVALIDATE_TLB;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_SEM_RELEASE        K2OSKERN_SCHED_ITEM_ARGS_SEM_RELEASE;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_CREATE      K2OSKERN_SCHED_ITEM_ARGS_THREAD_CREATE;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_EVENT_CHANGE       K2OSKERN_SCHED_ITEM_ARGS_EVENT_CHANGE;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_SLOT_BLOCK         K2OSKERN_SCHED_ITEM_ARGS_SLOT_BLOCK;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_SLOT_BOXES         K2OSKERN_SCHED_ITEM_ARGS_SLOT_BOXES;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_SLOT_PURGE         K2OSKERN_SCHED_ITEM_ARGS_SLOT_PURGE;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_MBOX_RECV          K2OSKERN_SCHED_ITEM_ARGS_MBOX_RECV;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_MBOX_RESPOND       K2OSKERN_SCHED_ITEM_ARGS_MBOX_RESPOND;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_MBOX_PURGE         K2OSKERN_SCHED_ITEM_ARGS_MBOX_PURGE;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_MSG_SEND           K2OSKERN_SCHED_ITEM_ARGS_MSG_SEND;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_MSG_ABORT          K2OSKERN_SCHED_ITEM_ARGS_MSG_ABORT;
typedef struct _K2OSKERN_SCHED_ITEM_ARGS_MSG_READ_RESP      K2OSKERN_SCHED_ITEM_ARGS_MSG_READ_RESP;

typedef enum _KernSchedTimerItemType KernSchedTimerItemType;
enum _KernSchedTimerItemType
{
    KernSchedTimerItemType_Error = 0,
    KernSchedTimerItemType_Wait,
    KernSchedTimerItemType_Alarm
};

struct _K2OSKERN_SCHED_TIMERITEM
{
    KernSchedTimerItemType      mType;
    UINT32                      mOnQueue;
    K2OSKERN_SCHED_TIMERITEM *  mpNext;
    UINT64                      mDeltaT;
};

struct _K2OSKERN_SCHED_WAITENTRY
{
    UINT8                   mMacroIndex;        /* my index inside my MACROWAIT - allows calc of MACROWAIT address */
    UINT8                   mUnused1;
    UINT16                  mStickyPulseStatus;
    K2LIST_LINK             WaitPrioListLink;
    K2OSKERN_OBJ_WAITABLE   mWaitObj;
};

struct _K2OSKERN_SCHED_MACROWAIT
{
    K2OSKERN_OBJ_THREAD *       mpWaitingThread;    /* thread that is using this macrowait */
    UINT32                      mNumEntries;
    BOOL                        mWaitAll;
    UINT32                      mWaitResult;
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
    UINT32                      mWaitResult;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_PURGE_PT
{
    UINT32  mPtIndex;
    UINT32  mPtPhysOut;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_INVALIDATE_TLB
{
    K2OSKERN_OBJ_PROCESS *  mpProc;
    UINT32                  mVirtAddr;
    UINT32                  mPageCount;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_SEM_RELEASE
{
    K2OSKERN_OBJ_SEM *  mpSem;
    UINT32              mCount;
    UINT32              mRetNewCount;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_THREAD_CREATE
{
    K2OSKERN_OBJ_THREAD *   mpThread;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_EVENT_CHANGE
{
    K2OSKERN_OBJ_EVENT *    mpEvent;
    BOOL                    mSetReset;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_SLOT_BLOCK
{
    K2OSKERN_OBJ_MAILSLOT * mpMailslot;
    BOOL                    mSetBlock;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_SLOT_BOXES
{
    BOOL                    mAttach;
    K2OSKERN_OBJ_MAILSLOT * mpMailslot;
    UINT32                  mBoxCount;
    K2OSKERN_OBJ_MAILBOX ** mppMailbox;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_SLOT_PURGE
{
    K2OSKERN_OBJ_MAILSLOT * mpMailslot;
    UINT32                  mObjCount;
    K2OSKERN_OBJ_HEADER **  mppObj;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_MBOX_RECV
{
    K2OSKERN_OBJ_MAILBOX *  mpMailbox;
    UINT32                  mRetRequestId;
    K2OS_MSGIO *            mpRetMsgIo;
    K2OSKERN_OBJ_MSG *      mpRetMsgRecv;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_MBOX_RESPOND
{
    K2OSKERN_OBJ_MAILBOX *  mpMailbox;
    UINT32                  mRequestId;
    K2OS_MSGIO const *      mpRespMsgIo;
    K2OSKERN_OBJ_MSG *      mpRetMsg1;
    K2OSKERN_OBJ_MSG *      mpRetMsg2;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_MBOX_PURGE
{
    K2OSKERN_OBJ_MAILBOX *  mpMailbox;
    K2OSKERN_OBJ_MSG *      mpRetMsgPurge;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_MSG_SEND
{
    K2OSKERN_OBJ_MAILSLOT * mpSlot;
    K2OSKERN_OBJ_MSG *      mpMsg;
    K2OS_MSGIO const *      mpIo;
    BOOL                    mResponseRequired;
    K2OSKERN_OBJ_MSG *      mpRetRelMsg;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_MSG_ABORT
{
    K2OSKERN_OBJ_MSG *  mpMsgInOut;
};

struct _K2OSKERN_SCHED_ITEM_ARGS_MSG_READ_RESP
{
    K2OSKERN_OBJ_MSG *  mpMsg;
    K2OS_MSGIO *        mpIo;
    BOOL                mClear;
};

union _K2OSKERN_SCHED_ITEM_ARGS
{
    K2OSKERN_SCHED_ITEM_ARGS_THREAD_EXIT        ThreadExit;      
    K2OSKERN_SCHED_ITEM_ARGS_THREAD_WAIT        ThreadWait;      
    K2OSKERN_SCHED_ITEM_ARGS_PURGE_PT           PurgePt;
    K2OSKERN_SCHED_ITEM_ARGS_INVALIDATE_TLB     InvalidateTlb;
    K2OSKERN_SCHED_ITEM_ARGS_SEM_RELEASE        SemRelease;
    K2OSKERN_SCHED_ITEM_ARGS_THREAD_CREATE      ThreadCreate;
    K2OSKERN_SCHED_ITEM_ARGS_EVENT_CHANGE       EventChange;
    K2OSKERN_SCHED_ITEM_ARGS_SLOT_BLOCK         SlotBlock;
    K2OSKERN_SCHED_ITEM_ARGS_SLOT_BOXES         SlotBoxes;
    K2OSKERN_SCHED_ITEM_ARGS_SLOT_PURGE         SlotPurge;
    K2OSKERN_SCHED_ITEM_ARGS_MBOX_RECV          MboxRecv;
    K2OSKERN_SCHED_ITEM_ARGS_MBOX_RESPOND       MboxRespond;
    K2OSKERN_SCHED_ITEM_ARGS_MBOX_PURGE         MboxPurge;
    K2OSKERN_SCHED_ITEM_ARGS_MSG_SEND           MsgSend;
    K2OSKERN_SCHED_ITEM_ARGS_MSG_ABORT          MsgAbort;
    K2OSKERN_SCHED_ITEM_ARGS_MSG_READ_RESP      MsgReadResp;
};

struct _K2OSKERN_SCHED_ITEM
{
    K2OSKERN_CPUCORE_EVENT volatile CpuCoreEvent;
    KernSchedItemType               mSchedItemType;
    K2OSKERN_SCHED_ITEM *           mpPrev;
    K2OSKERN_SCHED_ITEM *           mpNext;
    UINT32                          mSchedCallResult;
    K2OSKERN_SCHED_ITEM_ARGS        Args;
};

typedef enum _KernThreadLifeStage KernThreadLifeStage;
enum _KernThreadLifeStage
{
    KernThreadLifeStage_Init = 0,
    KernThreadLifeStage_Instantiated,
    KernThreadLifeStage_Run,            // not signalled at this stage or before
    KernThreadLifeStage_Exited,         // signalled at this stage or higher
    KernThreadLifeStage_Killed,
    KernThreadLifeStage_Cleanup
};

typedef enum _KernThreadRunState KernThreadRunState;
enum _KernThreadRunState
{
    KernThreadRunState_None=0,
    KernThreadRunState_Transition,
    KernThreadRunState_Ready,
    KernThreadRunState_Running,
    KernThreadRunState_Blocked_CS,
    KernThreadRunState_Waiting
};

#define KERNTHREAD_STOP_FLAG_NONE       0   // not stopped
#define KERNTHREAD_STOP_FLAG_DEBUG      1   // process in debug state
#define KERNTHREAD_STOP_FLAG_PAGEFAULT  2   // normal pagefault
#define KERNTHREAD_STOP_FLAG_EXCEPTION  4   // unhandled exception
#define KERNTHREAD_STOP_FLAG_PROC_EXIT  8   // process exiting

typedef struct _KernThreadState KernThreadState;
struct _KernThreadState
{
    KernThreadLifeStage mLifeStage;
    KernThreadRunState  mRunState;
    UINT32              mStopFlags; // nonzero = not stopped
};

typedef struct _K2OSKERN_SCHED_THREAD K2OSKERN_SCHED_THREAD;
struct _K2OSKERN_SCHED_THREAD
{
    UINT32              mBasePrio;
    UINT32              mQuantumLeft;
    UINT64              mAbsTimeAtStop;
    UINT64              mTotalRunTimeMs;
    K2OS_THREADATTR     Attr;       // current priority, affinity mask, quantum
    K2OSKERN_SCHED_ITEM Item;
    UINT32              mThreadActivePrio;
    UINT32              mLastRunCoreIx;
    K2LIST_LINK         ReadyListLink;
    K2LIST_ANCHOR       OwnedCritSecList;
    KernThreadState     State;
};

typedef struct _K2OSKERN_SCHED K2OSKERN_SCHED;
struct _K2OSKERN_SCHED
{
    UINT64                          mCurrentAbsTime;

    K2LIST_ANCHOR                   CpuCorePrioList;
    UINT32                          mIdleCoreCount;

    K2LIST_ANCHOR                   ReadyThreadsByPrioList[K2OS_THREADPRIO_LEVELS];
    UINT32                          mReadyThreadCount;

    UINT32                          mSysWideThreadCount;
    UINT32                          mNextThreadId;

    UINT32 volatile                 mReq;
    K2OSKERN_SCHED_ITEM * volatile  mpPendingItemListHead;

    K2OSKERN_SCHED_ITEM             SchedTimerSchedItem;
    K2OSKERN_SCHED_TIMERITEM *      mpTimerItemQueue;

    K2OSKERN_CPUCORE *              mpSchedulingCore;
    
    K2OSKERN_SCHED_ITEM *           mpActiveItem;
    K2OSKERN_OBJ_THREAD *           mpActiveItemThread;

    K2OSKERN_OBJ_PROCESS *          mpTlbInvProc;
    UINT32                          mTlbInv1_Base;
    UINT32                          mTlbInv1_PageCount;
    UINT32                          mTlbInv2_Base;
    UINT32                          mTlbInv2_PageCount;
    INT32 volatile                  mTlbCores;

    K2OSKERN_IRQ_CONFIG             SysTickDevIrqConfig;
    K2OS_TOKEN                      mTokSysTickIntr;
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_EVENT
{
    K2OSKERN_OBJ_HEADER     Hdr;
    BOOL                    mIsAutoReset;
    BOOL                    mIsSignalled;
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_CRITSEC
{
    K2OSKERN_SEQLOCK        SeqLock;
    UINT32                  mWaitingThreadsCount;   // only accessed under seqlock

    K2OSKERN_OBJ_THREAD *   mpOwner;
    K2LIST_LINK             ThreadOwnedCritSecLink;
    UINT32                  mRecursionCount;

    K2OSKERN_OBJ_EVENT      Event;
};

/* --------------------------------------------------------------------------------- */

//
// these coexist in the same UINT32 with K2OS_MEMPAGE_ATTR_xxx
//
#define K2OSKERN_SEG_ATTR_TYPE_DLX_PART     0x00010000
#define K2OSKERN_SEG_ATTR_TYPE_PROCESS      0x00020000
#define K2OSKERN_SEG_ATTR_TYPE_THREAD       0x00030000
#define K2OSKERN_SEG_ATTR_TYPE_HEAP_TRACK   0x00040000
#define K2OSKERN_SEG_ATTR_TYPE_SPARSE       0x00050000
#define K2OSKERN_SEG_ATTR_TYPE_DEVMAP       0x00060000
#define K2OSKERN_SEG_ATTR_TYPE_PHYSBUF      0x00070000
#define K2OSKERN_SEG_ATTR_TYPE_DLX_PAGE     0x00080000
#define K2OSKERN_SEG_ATTR_TYPE_SEG_SLAB     0x00090000
#define K2OSKERN_SEG_ATTR_TYPE_COUNT        0x000A0000
#define K2OSKERN_SEG_ATTR_TYPE_MASK         0x000F0000

typedef struct _K2OSKERN_SEGMENT_INFO_THREAD K2OSKERN_SEGMENT_INFO_THREAD;
struct _K2OSKERN_SEGMENT_INFO_THREAD
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
    UINT32                  mPhysDeviceAddr;
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
    K2OSKERN_SEGMENT_INFO_THREAD        Thread;
    K2OSKERN_SEGMENT_INFO_DEVICEMAP     DeviceMap;
    K2OSKERN_SEGMENT_INFO_PHYSBUF       PhysBuf;
    K2OSKERN_SEGMENT_INFO_USER          User;
};

struct _K2OSKERN_OBJ_SEGMENT
{
    K2OSKERN_OBJ_HEADER     Hdr;

    K2OSKERN_OBJ_PROCESS *  mpProc;
    K2TREE_NODE             ProcSegTreeNode;    // base address is tree node UserVal

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

typedef struct _K2OSKERN_TOKEN_INUSE K2OSKERN_TOKEN_INUSE;
struct _K2OSKERN_TOKEN_INUSE
{
    K2OSKERN_OBJ_HEADER *   mpObjHdr;
    UINT32                  mTokValue;
};

typedef union _K2OSKERN_TOKEN K2OSKERN_TOKEN;
union _K2OSKERN_TOKEN
{
    K2OSKERN_TOKEN_INUSE    InUse;
    K2LIST_LINK             FreeLink;
};

K2_STATIC_ASSERT(sizeof(K2OSKERN_TOKEN) == sizeof(K2LIST_LINK));
#define K2OSKERN_TOKENS_PER_PAGE   (K2_VA32_MEMPAGE_BYTES / sizeof(K2OSKERN_TOKEN))

typedef struct _K2OSKERN_TOKEN_PAGE_HDR K2OSKERN_TOKEN_PAGE_HDR;
struct _K2OSKERN_TOKEN_PAGE_HDR
{
    UINT32 mPageIndex;     // index of this page in the process sparse token page array
    UINT32 mInUseCount;    // count of tokens in this page that are in use
};

typedef union _K2OSKERN_TOKEN_PAGE K2OSKERN_TOKEN_PAGE;
union _K2OSKERN_TOKEN_PAGE
{
    K2OSKERN_TOKEN_PAGE_HDR    Hdr;
    K2OSKERN_TOKEN             Tokens[K2OSKERN_TOKENS_PER_PAGE];
};

#define K2OSKERN_TOKEN_SALT_MASK       0xFFF00000
#define K2OSKERN_TOKEN_SALT_DEC        0x00100000
#define K2OSKERN_TOKEN_MAX_VALUE       0x000FFFFF

#define K2OSKERN_MAX_TOKENS_PER_PROC   ((K2OSKERN_TOKEN_MAX_VALUE + 1) / sizeof(K2OSKERN_TOKENS_PER_PAGE))

K2_STATIC_ASSERT(sizeof(K2OSKERN_TOKEN_PAGE) == K2_VA32_MEMPAGE_BYTES);

typedef enum _KernProcState KernProcState;
enum _KernProcState
{
    KernProcState_Invalid = 0,
    KernProcState_Init,
    KernProcState_Exec,
    KernProcState_Stopped,
    KernProcState_Dying,
    KernProcState_Done, // signaled
    KernProcState_Purge,
    //this one goes last
    KernProcState_Count
};

struct _K2OSKERN_OBJ_PROCESS
{
    K2OSKERN_OBJ_HEADER     Hdr;
    KernProcState           mState;
    UINT32                  mId;
    UINT32                  mTransTableKVA;     // VIRT of root trans table
    UINT32                  mTransTableRegVal;  // PHYS(+) of root trans table
    UINT32                  mVirtMapKVA;        // VIRT of PTE array
    K2OSKERN_OBJ_SEGMENT    SegObjSelf;
    K2OSKERN_OBJ_SEGMENT    SegObjPrimaryThreadStack;
    K2OSKERN_OBJ_DLX        PrimaryModule;

    K2OSKERN_SEQLOCK        ThreadListSeqLock;
    K2LIST_ANCHOR           ThreadList;

    K2OS_CRITSEC            TlsMaskSec;
    UINT32                  mTlsMask;

    K2OSKERN_SEQLOCK        SegTreeSeqLock;
    K2TREE_ANCHOR           SegTree;

    K2OSKERN_SEQLOCK        TokSeqLock;
    K2OSKERN_TOKEN_PAGE **  mppTokPages;
    UINT32                  mTokPageCount;
    K2LIST_ANCHOR           TokFreeList;
    UINT32                  mTokSalt;
    UINT32                  mTokCount;

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

    BOOL                        mTlbFlushNeeded;
    UINT32                      mTlbFlushBase;
    UINT32                      mTlbFlushPages;

    K2OSKERN_OBJ_SEGMENT *      mpWorkSeg;
    K2OSKERN_OBJ_SEGMENT *      mpThreadCreateSeg;

    K2_EXCEPTION_TRAP *         mpKernExTrapStack;
    K2_EXCEPTION_TRAP *         mpUserExTrapStack;
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

struct _K2OSKERN_OBJ_SEM
{
    K2OSKERN_OBJ_HEADER Hdr;
    UINT32              mMaxCount;
    UINT32              mCurCount;
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_NAME
{
    K2OSKERN_OBJ_HEADER     Hdr;
    K2TREE_NODE             NameTreeNode;
    char                    NameBuffer[K2OS_NAME_MAX_LEN + 1];
    K2OS_CRITSEC            OwnerSec;
    K2OSKERN_OBJ_HEADER *   mpObject;
    K2OSKERN_OBJ_EVENT      Event_IsOwned;
};

/* --------------------------------------------------------------------------------- */

struct _K2OSKERN_OBJ_INTR
{
    K2OSKERN_OBJ_HEADER     Hdr;
    BOOL                    mIsSignalled;
    K2TREE_NODE             IntrTreeNode;
    K2OSKERN_IRQ_CONFIG     IrqConfig;
    K2OSKERN_pf_IntrHandler mfHandler;
    void *                  mpHandlerContext;
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
#define K2OSKERN_VMNODE_RESERVED_FACS               18
#define K2OSKERN_VMNODE_RESERVED_XFACS              19

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

typedef struct _K2OSKERN_SEGSLAB K2OSKERN_SEGSLAB;
#define SEGSTORE_SLAB_OVERHEAD  (sizeof(K2OSKERN_OBJ_SEGMENT) + sizeof(UINT64) + sizeof(K2OSKERN_SEGSLAB *))
#define SEGSTORE_OBJ_BYTES      (K2_VA32_MEMPAGE_BYTES - SEGSTORE_SLAB_OVERHEAD)
#define SEGSTORE_OBJ_COUNT      (SEGSTORE_OBJ_BYTES / sizeof(K2OSKERN_OBJ_SEGMENT))
K2_STATIC_ASSERT(SEGSTORE_OBJ_COUNT < 64);
struct _K2OSKERN_SEGSLAB
{
    K2OSKERN_OBJ_SEGMENT    This;                       // must be entry 0
    UINT8                   SegStore[SEGSTORE_OBJ_BYTES];   // must be right after This for alignment
    K2OSKERN_SEGSLAB *      mpNextSlab;
    UINT64                  mUseMask;                    // bit 0 always set
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_SEGSLAB) == K2_VA32_MEMPAGE_BYTES);

/* --------------------------------------------------------------------------------- */

#define K2OSKERN_MAILBOX_REQUESTSEQ_MASK   0xFFFFFFC0
K2_STATIC_ASSERT((K2OSKERN_MAILBOX_REQUESTSEQ_MASK + K2OS_MAX_NUM_SLOT_MAILBOXES) == 0);

#define K2OSKERN_MAILBOX_EMPTY                     0
#define K2OSKERN_MAILBOX_IN_SERVICE_WITH_MSG       1
#define K2OSKERN_MAILBOX_IN_SERVICE_NO_MSG         2
#define K2OSKERN_MAILBOX_IN_SERVICE_MSG_ABANDONED  3

struct _K2OSKERN_OBJ_MAILBOX
{
    K2OSKERN_OBJ_HEADER     Hdr;

    UINT16                  mState;
    UINT16                  mId;

    K2OSKERN_OBJ_MAILSLOT * mpSlot;
    K2LIST_LINK             SlotListLink;

    K2OSKERN_OBJ_MSG *      mpInSvcMsg;         // if mState is IN_SERVICE this will not be NULL
    K2OS_MSGIO              InSvcSendIo;        // msg content at send
    UINT32                  mInSvcRequestId;

    K2OSKERN_OBJ_EVENT      Event;
};

struct _K2OSKERN_OBJ_MAILSLOT
{
    K2OSKERN_OBJ_HEADER Hdr;

    K2LIST_ANCHOR       PendingMsgList;

    UINT32              mNextRequestSeq;
    BOOL                mBlocked;

    K2LIST_ANCHOR       EmptyMailboxList;
    K2LIST_ANCHOR       FullMailboxList;
};

#define K2OSKERN_MSG_FLAG_RESPONSE_REQUIRED    1
struct _K2OSKERN_OBJ_MSG
{
    K2OSKERN_OBJ_HEADER     Hdr;

    K2OSKERN_OBJ_MAILSLOT * mpPendingOnSlot;
    K2LIST_LINK             SlotPendingMsgListLink;

    K2OSKERN_OBJ_MAILBOX *  mpSittingInMailbox;

    UINT32                  mFlags;

    volatile UINT32         mRequestId;

    K2OS_MSGIO              Io;

    K2OSKERN_OBJ_EVENT      Event;
};

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

    //
    // segment object slabs
    //
    K2OS_CRITSEC                        SegSec;    
    K2LIST_ANCHOR                       SegFreeList;
    K2OSKERN_SEGSLAB *                  mpSegSlabs;


    // dlxsupp.c - 
    K2OSKERN_OBJ_DLX                    DlxCrt;
    K2OSKERN_OBJ_DLX                    DlxHal;
    K2OSKERN_OBJ_DLX                    DlxAcpi;
    K2OSKERN_OBJ_DLX                    DlxExec;
    // kernel dlx is in gpProc0 process object

    // sched.c
    K2OSKERN_SCHED                      Sched;

    // debugger
    BOOL                                mDebuggerActive;

    // objects and name
    K2OSKERN_SEQLOCK                    ObjTreeSeqLock;
    K2TREE_ANCHOR                       ObjTree;
    K2TREE_ANCHOR                       NameTree;

    // interrupts
    K2OSKERN_SEQLOCK                    IntrTreeSeqLock;
    K2TREE_ANCHOR                       IntrTree;

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
UINT32 KernDlx_FindClosestSymbol(K2OSKERN_OBJ_PROCESS *apCurProc, UINT32 aAddr, char *apRetSymName, UINT32 aRetSymNameBufLen);

/* --------------------------------------------------------------------------------- */

void   KernMem_Start(void);

UINT32 KernMem_CountPT(UINT32 aVirtAddr, UINT32 aPageCount);

K2STAT KernMem_VirtAllocToThread(K2OSKERN_OBJ_THREAD *apCurThread, UINT32 aUseAddr, UINT32 aPageCount, BOOL aTopDown);
void   KernMem_VirtFreeFromThread(K2OSKERN_OBJ_THREAD *apCurThread);

K2STAT KernMem_PhysAllocToThread(K2OSKERN_OBJ_THREAD *apCurThread, UINT32 aPageCount, KernPhys_Disp aDisp, BOOL aForPageTables);
void   KernMem_PhysFreeFromThread(K2OSKERN_OBJ_THREAD *apCurThread);

K2STAT KernMem_SegAllocToThread(K2OSKERN_OBJ_THREAD *apCurThread);
K2STAT KernMem_SegFreeFromThread(K2OSKERN_OBJ_THREAD *apCurThread);
void   KernMem_SegDispose(K2OSKERN_OBJ_SEGMENT *apSeg);

K2STAT KernMem_CreateSegmentFromThread(K2OSKERN_OBJ_THREAD *apCurThread, K2OSKERN_OBJ_SEGMENT *apSrc, K2OSKERN_OBJ_SEGMENT *apDst);

K2STAT KernMem_MapSegPagesFromThread(K2OSKERN_OBJ_THREAD *apCurThread, K2OSKERN_OBJ_SEGMENT *apSrc, UINT32 aSegOffset, UINT32 aPageCount, UINT32 aPageAttrFlags);
void   KernMem_UnmapSegPagesToThread(K2OSKERN_OBJ_THREAD *apCurThread, K2OSKERN_OBJ_SEGMENT *apSrc, UINT32 aSegOffset, UINT32 aPageCount, BOOL aClearNp);

/* --------------------------------------------------------------------------------- */

// these return virtual address of PTE changed
void   KernMap_MakeOnePresentPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aPhysAddr, UINTN aMapAttr);
void   KernMap_MakeOneNotPresentPage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aNpFlags, UINT32 aContent);
UINT32 KernMap_BreakOnePage(UINT32 aVirtMapBase, UINT32 aVirtAddr, UINT32 aNpFlags);

void   KernMap_MakeOnePageFromThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apPhysPageOwner, KernPhysPageList aTargetList);
UINT32 KernMap_BreakOnePageToThread(K2OSKERN_OBJ_THREAD *apCurThread, void *apMatchPageOwner, KernPhysPageList aMatchList, UINT32 aNpFlags);

BOOL   KernMap_SegRangeNotMapped(K2OSKERN_OBJ_THREAD *apCurThread, K2OSKERN_OBJ_SEGMENT *apSeg, UINT32 aPageOffset, UINT32 aPageCount);
BOOL   KernMap_SegRangeMapped(K2OSKERN_OBJ_THREAD *apCurThread, K2OSKERN_OBJ_SEGMENT *apSeg, UINT32 aPageOffset, UINT32 aPageCount);

void   KernMap_FindMapped(K2OSKERN_OBJ_THREAD *apCurThread, UINT32 aVirtAddr, UINT32 aPageCount, UINT32 *apScanIx, UINT32 *apFoundCount);

/* --------------------------------------------------------------------------------- */

UINT32  KernArch_MakePTE(UINT32 aPhysAddr, UINT32 aPageMapAttr);
void    KernArch_BreakMapTransitionPageTable(UINT32 *apRetVirtAddrPT, UINT32 *apRetPhysAddrPT);
void    KernArch_InvalidateTlbPageOnThisCore(UINT32 aVirtAddr);
BOOL    KernArch_VerifyPteKernHasAccessAttr(UINT32 aPTE, UINT32 aMustHaveAttr);
UINT32 *KernArch_Translate(K2OSKERN_OBJ_PROCESS *apProc, UINT32 aVirtAddr, BOOL *apRetPtPresent, UINT32 *apRetPte, UINT32 *apRetAccessAttr);
void    KernArch_PrepareThread(K2OSKERN_OBJ_THREAD *apThread);
void    KernArch_LaunchCores(void);
void    KernArch_ThreadCallSched(void);
void    KernArch_MonitorSwitchToProcZero(K2OSKERN_CPUCORE *apThisCore);
void    KernArch_SwitchFromMonitorToThread(K2OSKERN_CPUCORE *apThisCore);
void    KernArch_SendIci(UINT32 aCurCoreIx, BOOL aSendToSpecific, UINT32 aTargetCpuIx);

void    KernArch_InstallDevIntrHandler(K2OSKERN_OBJ_INTR *apIntr);
void    KernArch_SetDevIntrMask(K2OSKERN_OBJ_INTR *apIntr, BOOL aMask);
void    KernArch_RemoveDevIntrHandler(K2OSKERN_OBJ_INTR *apIntr);

UINT32  KernArch_DevIrqToVector(UINT32 aDevIrq);
UINT32  KernArch_VectorToDevIrq(UINT32 aVector);

void    KernArch_Panic(K2OSKERN_CPUCORE *apThisCore, BOOL aDumpStack);

/* --------------------------------------------------------------------------------- */

void    KernSegment_Dump(K2OSKERN_OBJ_SEGMENT *apSeg);

/* --------------------------------------------------------------------------------- */

void    KernProc_Dump(K2OSKERN_OBJ_PROCESS *apProc);

/* --------------------------------------------------------------------------------- */

void                    KernThread_Dump(K2OSKERN_OBJ_THREAD *apThread);
void   K2_CALLCONV_REGS KernThread_Entry(K2OSKERN_OBJ_THREAD *apThisThread);
K2STAT                  KernThread_Kill(K2OSKERN_OBJ_THREAD *apThread, UINT32 aForcedExitCode);
K2STAT                  KernThread_SetAttr(K2OSKERN_OBJ_THREAD *apThread, K2OS_THREADATTR const *apNewAttr);
void                    KernThread_Instantiate(K2OSKERN_OBJ_THREAD *apThisThread, K2OSKERN_OBJ_PROCESS *apProc, K2OS_THREADCREATE const *apCreate);
K2STAT                  KernThread_Start(K2OSKERN_OBJ_THREAD *apThisThread, K2OSKERN_OBJ_THREAD *apThread);
K2STAT                  KernThread_Dispose(K2OSKERN_OBJ_THREAD *apThread);
UINT32                  KernThread_WaitOne(K2OSKERN_OBJ_HEADER *apObjHdr, UINT32 aTimeoutMs);

UINT32 K2_CALLCONV_REGS K2OSKERN_Thread0(void *apArg);

/* --------------------------------------------------------------------------------- */

void KernSched_AddCurrentCore(void);
void KernSched_Check(K2OSKERN_CPUCORE *apThisCore);
void KernSched_RespondToCallFromThread(K2OSKERN_CPUCORE *apThisCore);

typedef BOOL(*KernSched_pf_Handler)(void);

BOOL KernSched_Exec_ThreadExit(void);
BOOL KernSched_Exec_ThreadWaitAny(void);
BOOL KernSched_Exec_EnterDebug(void);
BOOL KernSched_Exec_PurgePT(void);
BOOL KernSched_Exec_InvalidateTlb(void);
BOOL KernSched_Exec_SemRelease(void);
BOOL KernSched_Exec_ThreadCreate(void);
BOOL KernSched_Exec_EventChange(void);
BOOL KernSched_Exec_SlotBlock(void);
BOOL KernSched_Exec_SlotBoxes(void);
BOOL KernSched_Exec_SlotPurge(void);
BOOL KernSched_Exec_MboxRecv(void);
BOOL KernSched_Exec_MboxRespond(void);
BOOL KernSched_Exec_MboxPurge(void);
BOOL KernSched_Exec_MsgSend(void);
BOOL KernSched_Exec_MsgAbort(void);
BOOL KernSched_Exec_MsgReadResp(void);

BOOL KernSched_TimePassed(UINT64 aSchedTime);

void KernSched_TimerFired(K2OSKERN_CPUCORE *apThisCore);
void KernSched_TlbInvalidateAcrossCores(void);
void KernSched_PerCpuTlbInvEvent(K2OSKERN_CPUCORE *apThisCore);
void KernSched_StartSysTick(K2OSKERN_IRQ_CONFIG const * apConfig);
void KernSched_ArmSchedTimer(UINT32 aMsFromNow);
void KernSched_MakeThreadActive(K2OSKERN_OBJ_THREAD *apThread, BOOL aEndOfListAtPrio);
void KernSched_MakeThreadInactive(K2OSKERN_OBJ_THREAD *apThread, KernThreadRunState aNewState);

BOOL KernSched_AddTimerItem(K2OSKERN_SCHED_TIMERITEM *apItem, UINT64 aAbsWaitStartTime, UINT64 aWaitMs);
void KernSched_DelTimerItem(K2OSKERN_SCHED_TIMERITEM *apItem);
BOOL KernSched_RunningThreadQuantumExpired(K2OSKERN_CPUCORE *apCore, K2OSKERN_OBJ_THREAD *apRunningThread);
void KernSched_StopThread(K2OSKERN_OBJ_THREAD *apThread, K2OSKERN_CPUCORE *apCpuCore, KernThreadRunState aNewRunState, BOOL aSetCoreIdle);
void KernSched_PreemptCore(K2OSKERN_CPUCORE *apCore, K2OSKERN_OBJ_THREAD *apRunningThread, KernThreadRunState aNewState, K2OSKERN_OBJ_THREAD *apReadyThread);

BOOL KernSched_EventChange(K2OSKERN_OBJ_EVENT *apEvent, BOOL aSignal);

void KernSched_EndThreadWait(K2OSKERN_SCHED_MACROWAIT *apWait, UINT32 aWaitResult);

BOOL KernSched_CheckSignalOne_SatisfyAll(K2OSKERN_SCHED_MACROWAIT *apWait, K2OSKERN_SCHED_WAITENTRY *apEntry);

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

/* --------------------------------------------------------------------------------- */

K2STAT KernObj_AddName(K2OSKERN_OBJ_NAME *apNewName, K2OSKERN_OBJ_NAME **appRetActual);
K2STAT KernObj_Add(K2OSKERN_OBJ_HEADER *apObjHdr, K2OSKERN_OBJ_NAME *apObjName);
K2STAT KernObj_AddRef(K2OSKERN_OBJ_HEADER *apHdr);
K2STAT KernObj_Release(K2OSKERN_OBJ_HEADER *apHdr);

/* --------------------------------------------------------------------------------- */

K2STAT KernName_Define(K2OSKERN_OBJ_NAME *apName, char const *apString, K2OSKERN_OBJ_NAME **appRetActual);
K2STAT KernName_TokenToAddRefObject(K2OS_TOKEN aNameToken, K2OSKERN_OBJ_HEADER **appRetObj);
void   KernName_Dispose(K2OSKERN_OBJ_NAME *apNameObj);

/* --------------------------------------------------------------------------------- */

K2STAT KernEvent_Create(K2OSKERN_OBJ_EVENT *apEvent, K2OSKERN_OBJ_NAME *apName, BOOL aAutoReset, BOOL aInitialState);
K2STAT KernEvent_Change(K2OSKERN_OBJ_EVENT *apEvtObj, BOOL aSetReset);
void   KernEvent_Dispose(K2OSKERN_OBJ_EVENT *apEvtObj);

/* --------------------------------------------------------------------------------- */

K2STAT      KernTok_CreateNoAddRef(UINT32 aObjCount, K2OSKERN_OBJ_HEADER **appObjHdr, K2OS_TOKEN *apRetTokens);
K2STAT      KernTok_TranslateToAddRefObjs(UINT32 aTokenCount, K2OS_TOKEN const *apTokens, K2OSKERN_OBJ_HEADER **appRetObjHdrs);
K2OS_TOKEN  KernTok_CreateFromAddRefOfNamedObject(K2OS_TOKEN aNameToken, K2OS_ObjectType aObjType);

/* --------------------------------------------------------------------------------- */

K2STAT KernSem_Create(K2OSKERN_OBJ_SEM *apSem, K2OSKERN_OBJ_NAME *apName, UINT32 aMaxCount, UINT32 aInitCount);
K2STAT KernSem_Release(K2OSKERN_OBJ_SEM *apSem, UINT32 aCount, UINT32 *apRetNewCount);
void   KernSem_Dispose(K2OSKERN_OBJ_SEM *apSem);

/* --------------------------------------------------------------------------------- */

K2STAT KernMailslot_Create(K2OSKERN_OBJ_MAILSLOT *apMailslot, K2OSKERN_OBJ_NAME *apname, UINT32 aMailboxCount, K2OSKERN_OBJ_MAILBOX **appMailbox, BOOL aInitBlocked);
K2STAT KernMailslot_SetBlock(K2OSKERN_OBJ_MAILSLOT *apMailslot, BOOL aBlock);
void   KernMailslot_Dispose(K2OSKERN_OBJ_MAILSLOT *apMailslot);

K2STAT KernMailbox_Create(K2OSKERN_OBJ_MAILBOX *apMailbox, UINT16 aId);
K2STAT KernMailbox_Recv(K2OSKERN_OBJ_MAILBOX *apMailbox, K2OS_MSGIO * apRetMsgIo, UINT32 *apRetRequestId);
K2STAT KernMailbox_Respond(K2OSKERN_OBJ_MAILBOX *apMailbox, UINT32 aRequestId, K2OS_MSGIO const *apRetRespIo);
void   KernMailbox_Dispose(K2OSKERN_OBJ_MAILBOX *apMailbox);

K2STAT KernMsg_Create(K2OSKERN_OBJ_MSG *apMsg);
K2STAT KernMsg_Send(K2OSKERN_OBJ_MAILSLOT *apMailslot, K2OSKERN_OBJ_MSG *apMsg, K2OS_MSGIO const *apMsgIo, BOOL aResponseRequired);
K2STAT KernMsg_Abort(K2OSKERN_OBJ_MSG *apMsg);
K2STAT KernMsg_ReadResponse(K2OSKERN_OBJ_MSG *apMsg, K2OS_MSGIO * apRetRespIo, BOOL aClear);
void   KernMsg_Dispose(K2OSKERN_OBJ_MSG *apMsg);

/* --------------------------------------------------------------------------------- */

#endif // __KERN_H