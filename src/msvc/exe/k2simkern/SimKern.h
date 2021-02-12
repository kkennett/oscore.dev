#ifndef __SIMKERN_H
#define __SIMKERN_H

#include <lib/k2win32.h>
#include <lib/k2mem.h>
#include <lib/k2list.h>
#include "UserApi.h"

struct  SKSystem;
struct  SKCpu;
struct  SKThread;
struct  SKICI;
struct  SKProcess;
struct  SKNotify;
typedef enum _SKThreadState SKThreadState;
typedef enum _SKNotifyState SKNotifyState;

extern DWORD    theThreadSelfSlot;
extern HANDLE   theWin32ExitSignal;

#define SCHED_TICK_GRANULARITY      10

#define SKICI_CODE_MIGRATED_THREAD  0xFEED0001

struct SKSpinLock
{
    SKSpinLock(void)
    {
        InitializeCriticalSection(&Win32Sec);
    }

    ~SKSpinLock(void)
    {
        DeleteCriticalSection(&Win32Sec);
    }

    CRITICAL_SECTION    Win32Sec;

    void Lock(void)
    {
        EnterCriticalSection(&Win32Sec);
    }

    void Unlock(void)
    {
        LeaveCriticalSection(&Win32Sec);
    }
};

struct SKICI
{
    SKICI(void)
    {
        mpSenderCpu = NULL;
        mCode = 0;
        mpNext = NULL;
    }

    SKCpu *             mpSenderCpu;
    UINT_PTR            mCode;
    SKICI * volatile    mpNext;
};

struct SKProcess
{
    SKProcess(SKSystem *apSystem, UINT32 aId) : mId(aId), mpSystem(apSystem)
    {
        K2LIST_Init(&ThreadList);
        K2MEM_Zero(&SystemProcessListLink, sizeof(SystemProcessListLink));
    }
    UINT32 const        mId;
    SKSystem * const    mpSystem;
    K2LIST_LINK         SystemProcessListLink;
    K2LIST_ANCHOR       ThreadList;
};

typedef enum _SKNotifyState SKNotifyState;
enum _SKNotifyState
{
    SKNotifyState_Idle,
    SKNotifyState_Waiting,
    SKNotifyState_Active
};

struct SKNotify
{
    SKNotify(SKSystem *apSystem) : mpSystem(apSystem)
    {
        mState = SKNotifyState_Idle;
        K2LIST_Init(&WaitingThreadList);
        mDataWord = 0;
    }

    SKSystem * const    mpSystem;
    SKSpinLock          Lock;
    UINT_PTR            mDataWord;
    SKNotifyState       mState;
    K2LIST_ANCHOR       WaitingThreadList;
};

typedef enum _SKTimerItemType SKTimerItemType;
enum _SKTimerItemType
{
    SKTimerItemType_Invalid=0,
    SKTimerItemType_Thread_SleepTimerItem,
};

struct SKTimerItem
{
    SKTimerItemType mType;
    UINT_PTR        mDeltaT;
    SKTimerItem *   mpNext;
};

typedef enum _SKThreadState SKThreadState;
enum _SKThreadState
{
    SKThreadState_Invalid = 0,
    SKThreadState_Inception,
    SKThreadState_OnRunList,
    SKThreadState_WaitingOnNotify,
    SKThreadState_Sleeping,
    SKThreadState_Suspended,
    SKThreadState_Migrating,
    SKThreadState_SendBlocked,  // waiting to send on IPC Endpoint
    SKThreadState_RecvBlocked,  // waiting to recv IPC Endpoint or to recv a Notify
    SKThreadState_Dying,
    SKThreadState_Dead
};

#define THREAD_FLAG_ENTER_SYSTEM_CALL  1        // do not migrate until kernel clear this flag

struct SKThread
{
    SKThread(SKSystem *apSystem) : mpSystem(apSystem)
    {
        mhWin32Thread = NULL;
        mWin32ThreadId = 0;
        mpOwnerProcess = NULL;
        mState = SKThreadState_Inception;
        mRunTime.QuadPart = 0;
        mQuantum = 0;
        mpCurrentCpu = NULL;
        mpCpuMigratedNext = NULL;
        mSysCall_ResultStatus = 0;
        mSysCall_Id = 0;
        mSysCall_Arg1 = 0;
        mSysCall_Arg2 = 0;
        mSysCall_ResultValue = 0;
        mFlags = 0;
        K2MEM_Zero(&ProcessThreadListLink, sizeof(ProcessThreadListLink));
        K2MEM_Zero(&CpuThreadListLink, sizeof(CpuThreadListLink));
        K2MEM_Zero(&BlockedOnListLink, sizeof(BlockedOnListLink));
        mhWin32StartupMutex = NULL;
        mhWin32StartupEvent = NULL;
    }

    SKSystem * const mpSystem;

    HANDLE          mhWin32Thread;
    DWORD           mWin32ThreadId;
    HANDLE          mhWin32StartupMutex;
    HANDLE          mhWin32StartupEvent;

    SKProcess *     mpOwnerProcess;
    K2LIST_LINK     ProcessThreadListLink;

    SKThreadState   mState;
    K2LIST_LINK     BlockedOnListLink;

    UINT32          mFlags;

    K2STAT          mSysCall_ResultStatus;
    UINT_PTR        mSysCall_Id;
    UINT_PTR        mSysCall_Arg1;
    UINT_PTR        mSysCall_Arg2;
    UINT_PTR        mSysCall_ResultValue;

    LARGE_INTEGER   mRunTime;

    DWORD           mQuantum;

    // can only be modified by mpCurrentCpu
    SKCpu *         mpCurrentCpu;
    K2LIST_LINK     CpuThreadListLink;

    SKTimerItem     SleepTimerItem;

    // Migrated threads are NOT on the Cpu's thread list (yet)
    // and will have a NULL mpCurrentCpu
    SKThread * volatile mpCpuMigratedNext;
};

#define SKIRQ_INDEX_TIMER 0

struct SKCpu
{
    SKCpu(void)
    {
        mpSystem = NULL;
        mCpuIndex = (DWORD)-1;
        mhWin32IciEvent = NULL;
        mhWin32IrqEvent = NULL;
        mhWin32SysCallEvent = NULL;
        mhWin32KernelThread = NULL;
        mWin32KernelThreadId = 0;
        mpCurrentThread = NULL;
        mpIdleThread = NULL;
        mSchedTimeout.QuadPart = 0;
        mTimeNow.QuadPart = 0;
        mpPendingIcis = NULL;
        K2LIST_Init(&RunningThreadList);
        mpMigratedHead = NULL;
        mSchedTimerExpired = FALSE;
        mIrqStatus = 0;
        mThreadChanged = FALSE;
    }

    SKSystem *          mpSystem;
    DWORD               mCpuIndex;

    HANDLE              mhWin32IciEvent;
    HANDLE              mhWin32IrqEvent;
    HANDLE              mhWin32SysCallEvent;

    HANDLE              mhWin32KernelThread;
    DWORD               mWin32KernelThreadId;

    SKThread *          mpCurrentThread;

    SKThread *          mpIdleThread;

    BOOL                mSchedTimerExpired;
    LARGE_INTEGER       mSchedTimeout;
    LARGE_INTEGER       mTimeNow;
    BOOL                mThreadChanged;

    UINT32 volatile     mIrqStatus;

    SKICI * volatile    mpPendingIcis;

    // can only be modified by its own kernel thread
    K2LIST_ANCHOR       RunningThreadList;

    SKThread * volatile mpMigratedHead;

    void NB_SendIci(DWORD aTargetCpu, UINT_PTR aCode);
    void SetNextThread(void);

    void OnStartup(void);
    void OnShutdown(void);
    void OnRecvIci(SKCpu *apSenderCpu, UINT_PTR aCode);
    void OnIrqInterrupt(void);
    void OnSchedTimerExpiry(void);
    void OnSystemCall(void);

    void OnIrq_Timer(void);

    void OnReschedule(void);
};

struct SKSystem
{
    SKSystem(UINT_PTR aCpuCount, DWORD aCacheLineLen) : mCpuCount(aCpuCount), mCacheLineBytes(aCacheLineLen)
    {
        mpCpus = NULL;

        mPerfFreq.QuadPart = 0;

        mpProcess0 = NULL;
        K2LIST_Init(&ProcessList);

        mpTimerQueueHead = NULL;
        mhWin32TimerQueueTimer = NULL;
    }

    UINT_PTR const  mCpuCount;
    SKCpu      *    mpCpus;

    DWORD const     mCacheLineBytes;

    LARGE_INTEGER   mPerfFreq;

    SKProcess *     mpProcess0;
    K2LIST_ANCHOR   ProcessList;

    SKSpinLock      TimerQueueLock;
    SKTimerItem *   mpTimerQueueHead;
    HANDLE          mhWin32TimerQueueTimer;

    SKCpu * GetCurrentCpu(void)
    {
        return &mpCpus[GetCurrentProcessorNumber()];
    }

    void NB_SendThreadToCpu(DWORD aTargetCpu, SKThread *apThread);

    void MsToCpuTime(DWORD aMilliseconds, LARGE_INTEGER *apRetCpuTime)
    {
        apRetCpuTime->QuadPart = (((LONGLONG)aMilliseconds) * mPerfFreq.QuadPart) / 1000ll;
    }

    DWORD CpuTimeToMs(LARGE_INTEGER const *apCpuTime)
    {
        return (DWORD)((apCpuTime->QuadPart * 1000ll) / mPerfFreq.QuadPart);
    }

    void SignalTimerItem(SKTimerItem *apTimerItem)
    {
        SKThread *apThread;

        switch (apTimerItem->mType)
        {
        case SKTimerItemType_Thread_SleepTimerItem:
            apThread = K2_GET_CONTAINER(SKThread, apTimerItem, SleepTimerItem);
            K2_ASSERT(SKThreadState_Sleeping == apThread->mState);
            //
            // wake up the thread and throw it at the least busy CPU
            //
            printf("Thread %04X wake from sleep\n", apThread->mWin32ThreadId);
            apThread->mSysCall_ResultStatus = K2STAT_THREAD_WAITED;
            apThread->mSysCall_ResultValue = 0;
            apThread->mState = SKThreadState_Migrating;
            NB_SendThreadToCpu((DWORD)-1, apThread);
            break;
        default:
            K2_ASSERT(0);
            break;
        }
    }

    static VOID CALLBACK sMsTimerCallback(void *apParam, BOOLEAN TimerOrWaitFired)
    {
        SKSystem *pSystem = (SKSystem *)apParam;
        UINT32 irqStatus;
        UINT32 irqStatusNew;
        UINT32 irqStatusResult;
        SKCpu *pCpu;

        pCpu = &pSystem->mpCpus[((UINT32)rand()) % pSystem->mCpuCount];

        do
        {
            irqStatus = pCpu->mIrqStatus;
            if (0 != (irqStatus & (1 << SKIRQ_INDEX_TIMER)))
                return;
            irqStatusNew = irqStatus | (1 << SKIRQ_INDEX_TIMER);
            irqStatusResult = InterlockedCompareExchange(&pCpu->mIrqStatus, irqStatusNew, irqStatus);
        } while (irqStatusResult != irqStatus);

        SetEvent(pCpu->mhWin32IrqEvent);
    }

    K2STAT SysCall(UINT_PTR aCallId, UINT_PTR aArg1, UINT_PTR aArg2, UINT_PTR *apResultValue)
    {
        SKThread *pThisThread = (SKThread *)TlsGetValue(theThreadSelfSlot);

        // set thread flags (do not migrate, do not preempt)
        pThisThread->mSysCall_Id = aCallId;
        pThisThread->mSysCall_Arg1 = aArg1;
        pThisThread->mSysCall_Arg2 = aArg2;

        //
        // this tells the system not to migrate this thread until 
        // it is inside the kernel thread system call code.  the flag
        // will be automatically removed when that happens
        //
        pThisThread->mFlags |= THREAD_FLAG_ENTER_SYSTEM_CALL;

        // mpCurrentCpu will not change here

        // set system call event. win32 thread backing this thread should suspend 
        // since the kernel thread for this real cpu is going to run because
        // it has higher priority.  kernel thread will take over and do the
        // actual system call stuff for this thread, then schedule it again
        // when the system call processing is complete.
        SetEvent(pThisThread->mpCurrentCpu->mhWin32SysCallEvent);

        //
        // enter system call flag should be clear here (cleared by kernel);
        //
        K2_ASSERT(0 == (pThisThread->mFlags & THREAD_FLAG_ENTER_SYSTEM_CALL));

        K2STAT resultStatus = pThisThread->mSysCall_ResultStatus;

        if (!K2STAT_IS_ERROR(resultStatus))
        {
            if (NULL != apResultValue)
                *apResultValue = pThisThread->mSysCall_ResultValue;
        }

        return resultStatus;
    }

    UINT_PTR ThreadFree_NB_SignalNotify(SKNotify *apNotify, UINT_PTR aDataWord)
    {
        SKThread *  pReleasedThread;
        UINT_PTR    result;
        SKCpu *     pCurrentCpu;

        //
        // this function is threadless, and so can never block
        // and can be called from interrupt handlers.  if this is issued
        // by a thread, the capability (rights) to notify this object
        // will already have been checked
        //

        pCurrentCpu = GetCurrentCpu();

        pReleasedThread = NULL;

        apNotify->Lock.Lock();

        if (apNotify->mState != SKNotifyState_Waiting)
        {
            K2_ASSERT(0 == apNotify->WaitingThreadList.mNodeCount);

            if (apNotify->mState == SKNotifyState_Idle)
            {
                apNotify->mDataWord = aDataWord;
                apNotify->mState = SKNotifyState_Active;
            }
            else
            {
                // active. received more notify info
                apNotify->mDataWord |= aDataWord;
            }

            result = apNotify->mDataWord;
        }
        else
        {
            K2_ASSERT(0 != apNotify->WaitingThreadList.mNodeCount);

            pReleasedThread = K2_GET_CONTAINER(SKThread, apNotify->WaitingThreadList.mpHead, BlockedOnListLink);
            
            K2LIST_Remove(&apNotify->WaitingThreadList, &pReleasedThread->BlockedOnListLink);
            
            K2_ASSERT(SKThreadState_WaitingOnNotify == pReleasedThread->mState);
            
            result = aDataWord;
            
            if (0 == apNotify->WaitingThreadList.mNodeCount)
            {
                apNotify->mState = SKNotifyState_Idle;
            }
        }

        apNotify->Lock.Unlock();

        if (NULL != pReleasedThread)
        {
            pReleasedThread->mSysCall_ResultValue = result;
            pReleasedThread->mSysCall_ResultStatus = K2STAT_THREAD_WAITED;
            pReleasedThread->mState = SKThreadState_Migrating;

            apNotify->mpSystem->NB_SendThreadToCpu((DWORD)-1, pReleasedThread);
        }

        return result;
    }

    void InsideSysCall_Sleep(SKCpu *apThisCpu, SKThread *apCallingThread)
    {
        UINT_PTR aMsToSleep = apCallingThread->mSysCall_Arg1;
        SKTimerItem *pTimerItem;
        SKTimerItem *pPrevTimerItem;

        // sleep can never fail
        apCallingThread->mSysCall_ResultStatus = K2STAT_NO_ERROR;

        //
        // latch timer queue item that is part of thread onto the timer queue
        // and if the timer is not running start it
        //
        if (aMsToSleep == 0)
        {
            //
            // move this thread to the end of the run list (after idle thread)
            //
            K2LIST_Remove(&apThisCpu->RunningThreadList, &apCallingThread->CpuThreadListLink);
            K2LIST_AddAtTail(&apThisCpu->RunningThreadList, &apCallingThread->CpuThreadListLink);
            apThisCpu->SetNextThread();
            return;
        }

        //
        // remove from cpu run list and set new current thread
        //
        K2LIST_Remove(&apThisCpu->RunningThreadList, &apCallingThread->CpuThreadListLink);
        apCallingThread->mpCurrentCpu = NULL;
        apCallingThread->mState = SKThreadState_Sleeping;
        printf("Thread %04X Sleeping %d\n", apCallingThread->mWin32ThreadId, aMsToSleep);
        apThisCpu->SetNextThread();

        //
        // latch onto system timer queue
        //
        apCallingThread->SleepTimerItem.mType = SKTimerItemType_Thread_SleepTimerItem;
        apCallingThread->SleepTimerItem.mDeltaT = aMsToSleep;
        apCallingThread->SleepTimerItem.mpNext = NULL;

        TimerQueueLock.Lock();

        if (mpTimerQueueHead == NULL)
        {
            //
            // timer is not running.  add this as the sole item and start the timer
            //
            mpTimerQueueHead = &apCallingThread->SleepTimerItem;
            K2_ASSERT(NULL == mhWin32TimerQueueTimer);
            if (FALSE == CreateTimerQueueTimer(&mhWin32TimerQueueTimer, NULL, sMsTimerCallback, this, SCHED_TICK_GRANULARITY, SCHED_TICK_GRANULARITY, 0))
            {
                printf("CreateTimerQueueTimer failed\n");
                ExitProcess(__LINE__);
            }
        }
        else
        {
            // 
            // timer is already running. just add this in the right place
            //
            pPrevTimerItem = NULL;
            pTimerItem = mpTimerQueueHead;
            do
            {
                if (pTimerItem->mDeltaT > apCallingThread->SleepTimerItem.mDeltaT)
                {
                    //
                    // alter pTimerItem delta and break to add after pPrevTimerItem
                    //
                    pTimerItem->mDeltaT -= apCallingThread->SleepTimerItem.mDeltaT;
                    break;
                }

                apCallingThread->SleepTimerItem.mDeltaT -= pTimerItem->mDeltaT;
                pPrevTimerItem = pTimerItem;
                pTimerItem = pTimerItem->mpNext;

            } while (pTimerItem != NULL);

            apCallingThread->SleepTimerItem.mpNext = pTimerItem;
            if (NULL == pPrevTimerItem)
            {
                mpTimerQueueHead = &apCallingThread->SleepTimerItem;
            }
            else
            {
                pPrevTimerItem->mpNext = &apCallingThread->SleepTimerItem;
            }
        }
        TimerQueueLock.Unlock();
    }

    void InsideSysCall_ThreadExit(SKCpu *apThisCpu, SKThread *apCallingThread)
    {
        printf("THREAD %d EXIT\n", apCallingThread->mWin32ThreadId);
        K2LIST_Remove(&apThisCpu->RunningThreadList, &apCallingThread->CpuThreadListLink);
        apCallingThread->mState = SKThreadState_Dying;
        apThisCpu->SetNextThread();

        //
        // latch thread to cleanup task and set the notify for that task so it can run
        //
        // TODO
    }

    void InsideSysCall_SignalNotify(SKCpu *apThisCpu, SKThread *apCallingThread)
    {
        SKNotify *pNotify = (SKNotify *)apCallingThread->mSysCall_Arg1;
        // result is comined signal dataword at completion of the call, just as FYI
        apCallingThread->mSysCall_ResultValue = ThreadFree_NB_SignalNotify(pNotify, apCallingThread->mSysCall_Arg2);
        apCallingThread->mSysCall_ResultStatus = K2STAT_NO_ERROR;
    }

    void InsideSysCall_WaitForNotify(SKCpu *apThisCpu, SKThread *apCallingThread, bool aNonBlocking)
    {
        SKNotify *pNotify = (SKNotify *)apCallingThread->mSysCall_Arg1;

        //
        // this can only be called by a thread.  the check to see if the thread
        // has the capability (rights) to wait on this object has already been checked
        // by the time you get here.
        //

        pNotify->Lock.Lock();

        if (pNotify->mState == SKNotifyState_Active)
        {
            //
            // we are not going to block
            //
            apCallingThread->mSysCall_ResultValue = pNotify->mDataWord;
            apCallingThread->mSysCall_ResultStatus = K2STAT_NO_ERROR;
            pNotify->mDataWord = 0;
            pNotify->mState = SKNotifyState_Idle;
        }
        else
        {
            if (aNonBlocking)
            {
                apCallingThread->mSysCall_ResultValue = 0;
                apCallingThread->mSysCall_ResultStatus = K2STAT_ERROR_WAIT_FAILED;
            }
            else
            {
                if (pNotify->mState == SKNotifyState_Idle)
                {
                    pNotify->mState = SKNotifyState_Waiting;
                }
                else
                {
                    K2_ASSERT(SKNotifyState_Waiting == pNotify->mState);
                }

                //
                // remove from the run list and set new current thread
                //
                K2LIST_Remove(&apThisCpu->RunningThreadList, &apCallingThread->CpuThreadListLink);
                apCallingThread->mpCurrentCpu = NULL;
                apCallingThread->mState = SKThreadState_WaitingOnNotify;
                K2LIST_AddAtTail(&pNotify->WaitingThreadList, &apCallingThread->BlockedOnListLink);
                apThisCpu->SetNextThread();
            }
        }

        pNotify->Lock.Unlock();
    }
};

#define IPC_CHANNEL_CONSUMER_ACTIVE_FLAG    0x80000000
#define IPC_CHANNEL_COUNT_MASK              0x7FFFFFFF

struct SKIpcChannel
{
    SKIpcChannel(SKSystem *apSystem) : mpSystem(apSystem)
    {
        mEntryBytes = 0;
        mEntryCount = 0;

        mhWin32ConsumerMappingObj = NULL;
        mpConsumerCountAndActiveFlag = NULL; 
        
        mhWin32ProducerMappingObj = NULL;
        mpProducerCount = NULL;
        mpProducerRoViewOfConsumerCountAndActiveFlag = NULL;

        mpConsumerNotify = NULL;

        mhWin32BufferMappingObj = NULL;
        mpConsumerRoViewOfBuffer = NULL;
        mpProducerRwViewOfBuffer = NULL;
        mpSharedOwnerMaskArray = NULL;
        mOwnerMapArraySizeInEntries = 0;
    };

    SKSystem * const        mpSystem;
    UINT32                  mEntryBytes;
    UINT32                  mEntryCount;

    HANDLE                  mhWin32ConsumerMappingObj;
    UINT32 volatile *       mpConsumerCountAndActiveFlag;

    HANDLE                  mhWin32ProducerMappingObj;
    UINT32 volatile *       mpProducerCount;
    UINT32 volatile *       mpProducerRoViewOfConsumerCountAndActiveFlag;

    SKNotify *              mpConsumerNotify;

    HANDLE                  mhWin32BufferMappingObj;
    UINT8 volatile *        mpConsumerRoViewOfBuffer;
    UINT8 volatile *        mpProducerRwViewOfBuffer;
    UINT32 volatile *       mpSharedOwnerMaskArray;     
    UINT32                  mOwnerMapArraySizeInEntries;

    void Setup(UINT32 aEntryBytes, UINT32 aEntryCount)
    {
        DWORD bufferBytes;

        mpConsumerNotify = new SKNotify(mpSystem);
        if (NULL == mpConsumerNotify)
        {
            printf("Failed to create Notify for IpcChannel\n");
            ExitProcess(__LINE__);
        }

        mhWin32ConsumerMappingObj = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(UINT32), NULL);
        if (NULL == mhWin32ConsumerMappingObj)
        {
            printf("Failed to create consumer mapping\n");
            ExitProcess(__LINE__);
        }
        mpConsumerCountAndActiveFlag = (UINT32 volatile *)MapViewOfFile(mhWin32ConsumerMappingObj, FILE_MAP_WRITE, 0, 0, 0);
        if (NULL == mpConsumerCountAndActiveFlag)
        {
            printf("Failed to map consumer mapping\n");
            ExitProcess(__LINE__);
        }
        *mpConsumerCountAndActiveFlag = IPC_CHANNEL_CONSUMER_ACTIVE_FLAG;
        mpProducerRoViewOfConsumerCountAndActiveFlag = (UINT32 volatile *)MapViewOfFile(mhWin32ConsumerMappingObj, FILE_MAP_READ, 0, 0, 0);
        if (NULL == mpProducerRoViewOfConsumerCountAndActiveFlag)
        {
            printf("Failed to map consumer mapping\n");
            ExitProcess(__LINE__);
        }

        mhWin32ProducerMappingObj = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(UINT32), NULL);
        if (NULL == mhWin32ProducerMappingObj)
        {
            printf("Failed to create consumer mapping\n");
            ExitProcess(__LINE__);
        }
        mpProducerCount = (UINT32 volatile *)MapViewOfFile(mhWin32ProducerMappingObj, FILE_MAP_WRITE, 0, 0, 0);
        if (NULL == mpProducerCount)
        {
            printf("Failed to map consumer mapping\n");
            ExitProcess(__LINE__);
        }
        *mpProducerCount = 0;

        aEntryBytes = ((aEntryBytes + (mpSystem->mCacheLineBytes - 1)) / mpSystem->mCacheLineBytes) * mpSystem->mCacheLineBytes;

        bufferBytes = (((aEntryCount + 31) / 32) * 4);
        mOwnerMapArraySizeInEntries = ((bufferBytes + (aEntryBytes - 1)) / aEntryBytes);
        bufferBytes = aEntryBytes * (mOwnerMapArraySizeInEntries + aEntryCount);

        mhWin32BufferMappingObj = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, bufferBytes, NULL);
        if (NULL == mhWin32BufferMappingObj)
        {
            printf("Failed to create file mapping for IpcChannel\n");
            ExitProcess(__LINE__);
        }
        mpProducerRwViewOfBuffer = (UINT8 volatile *)MapViewOfFile(mhWin32BufferMappingObj, FILE_MAP_WRITE, 0, 0, 0);
        if (NULL == mpProducerRwViewOfBuffer)
        {
            printf("Failed to map IpcChannel buffer RW\n");
            ExitProcess(__LINE__);
        }
        K2MEM_Zero((void *)mpProducerRwViewOfBuffer, bufferBytes);
        mpSharedOwnerMaskArray = (UINT32 volatile *)mpProducerRwViewOfBuffer;
        mpConsumerRoViewOfBuffer = (UINT8 volatile *)MapViewOfFile(mhWin32BufferMappingObj, FILE_MAP_READ, 0, 0, 0);
        if (NULL == mpConsumerRoViewOfBuffer)
        {
            printf("Failed to map IpcChannel buffer RO\n");
            ExitProcess(__LINE__);
        }

        mEntryBytes = aEntryBytes;
        mEntryCount = aEntryCount;
    }

    bool Produce(UINT8 const *apData, UINT32 aDataBytes)
    {
        UINT32 slotIndex;
        UINT32 exchangeVal;
        UINT32 valResult;
        UINT32 wordIndex;
        UINT32 bitIndex;
        UINT32 ownerMask;

        //
        // atomically increment producer count if there is room
        //
        do
        {
            slotIndex = *mpProducerCount;
            wordIndex = slotIndex >> 5;
            bitIndex = slotIndex & 0x1F;
            if (0 != (mpSharedOwnerMaskArray[wordIndex] & (1<<bitIndex)))
            {
                //
                // no space to produce into
                //
                return false;
            }
            exchangeVal = slotIndex + 1;
            if (exchangeVal == mEntryCount)
                exchangeVal = 0;
            valResult = InterlockedCompareExchange(mpProducerCount, exchangeVal, slotIndex);
        } while (valResult != slotIndex);

        //
        // we have the target slot number.  do the copy now
        //
        K2MEM_Copy((void *)(mpProducerRwViewOfBuffer + ((slotIndex + mOwnerMapArraySizeInEntries) * mEntryBytes)), apData, aDataBytes);

        //
        // set the slot to 'full' status now
        //
        do
        {
            ownerMask = mpSharedOwnerMaskArray[wordIndex];
            exchangeVal = ownerMask | (1 << bitIndex);
            valResult = InterlockedCompareExchange(&mpSharedOwnerMaskArray[wordIndex], exchangeVal, ownerMask);
        } while (valResult != ownerMask);

        //
        // if the produce and consume counts were the same and now they are not, and the consumer active flag is clear
        // then we need to wake up the consumer by signalling the notify
        //
        if ((*mpProducerRoViewOfConsumerCountAndActiveFlag) == slotIndex)
        {
            //
            // active flag not set so wake the receiver
            //
            mpSystem->SysCall(SYSCALL_ID_SIGNAL_NOTIFY, (UINT_PTR)mpConsumerNotify, 1, NULL);
        }

        return true;
    }

    UINT8 volatile * ConsumeAcquire(UINT32 *apRetKey)
    {
        UINT32 slotIndex;
        UINT32 exchangeVal;
        UINT32 valResult;
        UINT32 wordIndex;
        UINT32 bitIndex;
        bool   doWait;

        K2_ASSERT(apRetKey != NULL);

        //
        // check value at current consume count to see if it is 'full'
        //
        do
        {
            slotIndex = *mpConsumerCountAndActiveFlag;
            wordIndex = (slotIndex & IPC_CHANNEL_COUNT_MASK) >> 5;
            bitIndex = slotIndex & 0x1F;

            if (0 != (mpSharedOwnerMaskArray[wordIndex] & (1 << bitIndex)))
            {
                exchangeVal = (slotIndex & IPC_CHANNEL_COUNT_MASK) + 1;
                if (exchangeVal == mEntryCount)
                    exchangeVal = 0;
                exchangeVal |= IPC_CHANNEL_CONSUMER_ACTIVE_FLAG;
                valResult = InterlockedCompareExchange(mpConsumerCountAndActiveFlag, exchangeVal, slotIndex);
                if (valResult == slotIndex)
                    break;
                // else try again from top of loop
            }
            else
            {
                //
                // next slot is empty, so nothing to consume.  we may need to block this thread
                //
                if (slotIndex & IPC_CHANNEL_CONSUMER_ACTIVE_FLAG)
                {
                    exchangeVal = slotIndex & ~IPC_CHANNEL_CONSUMER_ACTIVE_FLAG;
                    valResult = InterlockedCompareExchange(mpConsumerCountAndActiveFlag, exchangeVal, slotIndex);
                    if (valResult == slotIndex)
                    {
                        //
                        // we successfully cleared the active flag
                        //
                        if (0 != (mpSharedOwnerMaskArray[wordIndex] & (1 << bitIndex)))
                        {
                            // 
                            // slot is full now (set in between first check and after we cleared the active flag
                            // so make sure our active flag is set and go around again
                            //
                            do
                            {
                                slotIndex = *mpConsumerCountAndActiveFlag;
                                if (slotIndex & IPC_CHANNEL_CONSUMER_ACTIVE_FLAG)
                                {
                                    //
                                    // somebody else set the active flag so we can stop trying to do that
                                    //
                                    break;
                                }
                                exchangeVal = slotIndex | IPC_CHANNEL_CONSUMER_ACTIVE_FLAG;
                                valResult = InterlockedCompareExchange(mpConsumerCountAndActiveFlag, exchangeVal, slotIndex);
                            } while (valResult != slotIndex);

                            //
                            // active flag is now set. go around again
                            //
                            doWait = false;
                        }
                        else
                        {
                            //
                            // still nothing for us to consume after we cleared the active flag. so wait
                            //
                            doWait = true;
                        }
                    }
                    else
                    {
                        //
                        // we couldn't clear the active flag.  go around again
                        //
                        doWait = false;
                    }
                }
                else
                {
                    //
                    // active flag is already clear. just wait
                    //
                    doWait = true;
                }

                if (doWait)
                    mpSystem->SysCall(SYSCALL_ID_WAIT_FOR_NOTIFY, (UINT_PTR)mpConsumerNotify, 0, NULL);
            }

        } while (true);

        slotIndex &= ~IPC_CHANNEL_CONSUMER_ACTIVE_FLAG;

        *apRetKey = slotIndex;

        return mpConsumerRoViewOfBuffer + (mEntryBytes * (slotIndex + mOwnerMapArraySizeInEntries));
    }

    void ConsumeRelease(UINT32 aKey)
    {
        UINT32 wordIndex;
        UINT32 bitIndex;

        UINT32 exchangeVal;
        UINT32 valResult;
        UINT32 ownerMask;

        K2_ASSERT(aKey < mEntryCount);

        wordIndex = aKey >> 5;
        bitIndex = aKey & 0x1F;

        //
        // set the slot to 'empty' status so producer can produce into it again
        //
        do
        {
            ownerMask = mpSharedOwnerMaskArray[wordIndex];
            K2_ASSERT(0 != (ownerMask & (1 << bitIndex)));
            exchangeVal = ownerMask & ~(1 << bitIndex);
            valResult = InterlockedCompareExchange(&mpSharedOwnerMaskArray[wordIndex], exchangeVal, ownerMask);
        } while (valResult != ownerMask);
    }
};


#endif // __SIMKERN_H