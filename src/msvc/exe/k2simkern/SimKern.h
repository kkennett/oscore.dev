#ifndef __SIMKERN_H
#define __SIMKERN_H

#include <lib/k2win32.h>
#include <lib/k2mem.h>
#include <lib/k2list.h>

struct  SKSystem;
struct  SKCpu;
struct  SKThread;
struct  SKICI;
struct  SKProcess;
struct  SKNotify;
struct  SKIpcEndpoint;
typedef enum _SKThreadState SKThreadState;
typedef enum _SKNotifyState SKNotifyState;

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
    volatile SKICI *    mpNext;
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

typedef UINT32 SKIpcMessage;



struct SKIpcEndpoint
{
    SKIpcEndpoint(SKSystem *apSystem) : mpSystem(apSystem)
    {
        K2LIST_Init(&SendingThreadList);
        K2LIST_Init(&RecvingThreadList);
    }
    SKSystem * const    mpSystem;
    K2LIST_ANCHOR       SendingThreadList;
    K2LIST_ANCHOR       RecvingThreadList;
};

typedef enum _SKThreadState SKThreadState;
enum _SKThreadState
{
    SKThreadState_Invalid=0,
    SKThreadState_Inception,
    SKThreadState_Suspended,
    SKThreadState_OnRunList,
    SKThreadState_Migrating,
    SKThreadState_SendBlocked,  // waiting to send on IPC Endpoint
    SKThreadState_RecvBlocked,  // waiting to recv IPC Endpoint or to recv a Notify
    SKThreadState_Dying,
    SKThreadState_Dead
};

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
        mSysCallResult = 0;
        K2MEM_Zero(&ProcessThreadListLink, sizeof(ProcessThreadListLink));
        K2MEM_Zero(&CpuThreadListLink, sizeof(CpuThreadListLink));
        K2MEM_Zero(&BlockedOnListLink, sizeof(BlockedOnListLink));
    }

    SKSystem * const mpSystem;

    HANDLE          mhWin32Thread;
    DWORD           mWin32ThreadId;

    SKProcess *     mpOwnerProcess;
    K2LIST_LINK     ProcessThreadListLink;

    SKThreadState   mState;
    K2LIST_LINK     BlockedOnListLink;

    UINT_PTR        mSysCallResult;

    LARGE_INTEGER   mRunTime;

    DWORD           mQuantum;

    // can only be modified by mpCurrentCpu
    SKCpu *         mpCurrentCpu;
    K2LIST_LINK     CpuThreadListLink;

    // Migrated threads are NOT on the Cpu's thread list (yet)
    // and will have a NULL mpCurrentCpu
    SKThread volatile * mpCpuMigratedNext;
};

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
        mReschedFlag = FALSE;
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
    BOOL                mReschedFlag;

    volatile SKICI *    mpPendingIcis;

    // can only be modified by its own kernel thread
    K2LIST_ANCHOR       RunningThreadList;

    SKThread volatile * mpMigratedHead;

    void SendIci(DWORD aTargetCpu, UINT_PTR aCode);

    void OnStartup(void);
    void OnShutdown(void);
    void OnRecvIci(SKCpu *apSenderCpu, UINT_PTR aCode);
    void OnIrqInterrupt(void);
    void OnSchedTimerExpiry(void);
    void OnSystemCall(void);
};

#define SYSCALL_ID_SIGNAL_NOTIFY    1
#define SYSCALL_ID_WAIT_FOR_NOTIFY  2

struct SKSystem
{
    SKSystem(UINT_PTR aCpuCount) : mCpuCount(aCpuCount)
    {
        mpCpus = NULL;
        mPerfFreq.QuadPart = 0;
        K2LIST_Init(&ProcessList);
    }

    UINT_PTR const  mCpuCount;
    SKCpu      *    mpCpus;

    LARGE_INTEGER   mPerfFreq;

    K2LIST_ANCHOR   ProcessList;

    SKCpu * GetCurrentCpu(void)
    {
        return &mpCpus[GetCurrentProcessorNumber()];
    }

    void SendThreadToCpu(DWORD aTargetCpu, SKThread *apThread);

    void MsToCpuTime(DWORD aMilliseconds, LARGE_INTEGER *apRetCpuTime)
    {
        apRetCpuTime->QuadPart = (((LONGLONG)aMilliseconds) * mPerfFreq.QuadPart) / 1000ll;
    }

    DWORD CpuTimeToMs(LARGE_INTEGER const *apCpuTime)
    {
        return (DWORD)((apCpuTime->QuadPart * 1000ll) / mPerfFreq.QuadPart);
    }

    void SysCall(UINT_PTR aCallId, UINT_PTR aArg1, UINT_PTR aArg2)
    {

    }

    void ThreadFree_SignalNotify(SKNotify *apNotify, UINT_PTR aDataWord)
    {
        SKThread *  pReleasedThread;
        UINT_PTR    result;
        SKCpu *     pCurrentCpu;
        SKCpu *     pCpuReceivingThread;

        //
        // this function is threadless, and so can never block
        // and can be called from interrupt handlers.  if this is issued
        // by a thread, the capability (rights) to notify this object
        // will already have been checked
        //

        pCurrentCpu = apNotify->mpSystem->GetCurrentCpu();

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
        }
        else
        {
            K2_ASSERT(0 != apNotify->WaitingThreadList.mNodeCount);

            pReleasedThread = K2_GET_CONTAINER(SKThread, apNotify->WaitingThreadList.mpHead, BlockedOnListLink);
            
            K2LIST_Remove(&apNotify->WaitingThreadList, &pReleasedThread->BlockedOnListLink);
            
            K2_ASSERT(SKThreadState_RecvBlocked == pReleasedThread->mState);
            
            result = aDataWord;
            
            if (0 == apNotify->WaitingThreadList.mNodeCount)
            {
                apNotify->mState = SKNotifyState_Idle;
            }
        }

        apNotify->Lock.Unlock();

        if (NULL != pReleasedThread)
        {
            pReleasedThread->mSysCallResult = result;

            pCpuReceivingThread = (SKCpu *)0xFEEDF00D;

            if (pCurrentCpu != pCpuReceivingThread)
            {
                apNotify->mpSystem->SendThreadToCpu(pCpuReceivingThread->mCpuIndex, pReleasedThread);
            }
            else
            {
                pReleasedThread->mState = SKThreadState_OnRunList;
                K2LIST_AddAtTail(&pCpuReceivingThread->RunningThreadList, &pReleasedThread->CpuThreadListLink);
                pCurrentCpu->mReschedFlag = TRUE;
            }
        }
    }

    void InsideSysCall_SignalNotify(SKNotify *apNotify, UINT_PTR aDataWord)
    {
        ThreadFree_SignalNotify(apNotify, aDataWord);
    }

    void InsideSysCall_WaitForNotify(SKNotify *apNotify)
    {
        SKCpu *     pCurrentCpu;
        SKThread *  pCurrentThread;

        //
        // this can only be called by a thread.  the check to see if the thread
        // has the capability (rights) to wait on this object has already been checked
        // by the time you get here.
        //
        pCurrentCpu = apNotify->mpSystem->GetCurrentCpu();
        pCurrentThread = pCurrentCpu->mpCurrentThread;

        apNotify->Lock.Lock();

        if (apNotify->mState == SKNotifyState_Active)
        {
            //
            // we are not going to block
            //
            pCurrentThread->mSysCallResult = apNotify->mDataWord;
            apNotify->mDataWord = 0;
            apNotify->mState = SKNotifyState_Idle;
        }
        else
        {
            if (apNotify->mState == SKNotifyState_Idle)
            {
                apNotify->mState = SKNotifyState_Waiting;
            }
            else
            {
                K2_ASSERT(SKNotifyState_Waiting == apNotify->mState);
            }

            K2LIST_Remove(&pCurrentCpu->RunningThreadList, &pCurrentThread->CpuThreadListLink);
            pCurrentCpu->mpCurrentThread = pCurrentCpu->mpIdleThread;
            pCurrentCpu->mReschedFlag = TRUE;

            pCurrentThread->mState = SKThreadState_RecvBlocked;
            K2LIST_AddAtTail(&apNotify->WaitingThreadList, &pCurrentThread->BlockedOnListLink);
        }

        apNotify->Lock.Unlock();
    }

    SKIpcMessage WaitForIpc(SKIpcEndpoint *apEndpoint)
    {
        //
        // data will either already be in the receive buffer (kernel page) or will come in after we block
        // kernel page should always be read-only to user mode code, but r/w to kernel
        //
        // buffer head/tail indexes can live in one UINT32 in high/low
        //
        // use zeroth cache line (64 bytes) for counters (head/tail)
        return 0;
    }

    SKIpcMessage PollForIpc(SKIpcEndpoint *apEndpoint)
    {
        return 0;
    }

    void SendIpc(SKIpcEndpoint *apEndpoint, SKIpcMessage aMessage)
    {
        //
        // use current thread's (pinned) send buffer if message uses one.
        // copy the data into the receive buffer space (kernel page) at the point of the send
        // and if the endpoint has an associated notify, then signal it
        //

        //
        // if there is a send buffer, then check to make sure it is pinned
        // if it is not, then transform the system call into a 'pin buffer' IPC message send
        // to the pager, that will be chained to the real send system call when it completes
        //
    }

    bool TryIpc(SKIpcEndpoint *apEndpoint, SKIpcMessage aMessage)
    {
        return false;
    }

};

#define IPC_CHANNEL_CONSUMER_ACTIVE_FLAG    0x80000000
#define IPC_CHANNEL_COUNT_MASK              0x0000003F  // 0-63

struct SKIpcChannel
{
    SKIpcChannel(SKSystem *apSystem) : mpSystem(apSystem)
    {
        mEntryBytes = 0;
        mpSharedOwnerMask = NULL;
        mpConsumerCountAndActiveFlag = NULL;  // init value to IPC_CHANNEL_CONSUMER_ACTIVE_FLAG;
        mpProducerCount = NULL;
        mpConsumerNotify = NULL;

        mpConsumerRoViewOfBuffer = NULL;
        mpProducerRwViewOfBuffer = NULL;
    };

    SKSystem * const        mpSystem;
    UINT32                  mEntryBytes;

    UINT64 volatile *       mpSharedOwnerMask;
    UINT32 volatile *       mpConsumerCountAndActiveFlag;
    UINT32 volatile *       mpProducerCount;
    SKNotify *              mpConsumerNotify;

    UINT8 volatile *        mpConsumerRoViewOfBuffer;
    UINT8 volatile *        mpProducerRwViewOfBuffer;

    bool Produce(UINT8 const *apMsg, UINT32 aMsgBytes)
    {
        UINT32 slotIndex;
        UINT32 exchangeVal;
        UINT32 valResult;

        UINT64 ownerMask64;
        UINT64 exchangeVal64;
        UINT64 valResult64;

        //
        // atomically increment producer count if there is room
        //
        do
        {
            slotIndex = *mpProducerCount;
            if (0 != ((*mpSharedOwnerMask) & (1ull << slotIndex)))
            {
                //
                // no space to produce into
                //
                return false;
            }
            exchangeVal = ((slotIndex + 1) & IPC_CHANNEL_COUNT_MASK);
            valResult = InterlockedCompareExchange(mpProducerCount, exchangeVal, slotIndex);
        } while (valResult != slotIndex);

        //
        // we have the target slot number.  do the copy now
        //
        K2MEM_Copy((void *)(mpProducerRwViewOfBuffer + (slotIndex * mEntryBytes)), apMsg, aMsgBytes);

        //
        // set the slot to 'full' status now
        //
        do
        {
            ownerMask64 = *mpSharedOwnerMask;
            exchangeVal64 = ownerMask64 | (1ull << slotIndex);
            valResult64 = InterlockedCompareExchange64((volatile LONG64 *)mpSharedOwnerMask, exchangeVal64, ownerMask64);
        } while (valResult64 != ownerMask64);

        //
        // if the produce and consume counts were the same and now they are not, and the consumer active flag is clear
        // then we need to wake up the consumer by signalling the notify
        //
        if ((*mpConsumerCountAndActiveFlag) == slotIndex)
        {
            //
            // active flag not set so wake the receiver
            //
            mpSystem->SysCall(SYSCALL_ID_SIGNAL_NOTIFY, (UINT_PTR)mpConsumerNotify, 1);
        }

        return true;
    }

    UINT8 volatile * ConsumeAcquire(UINT32 *apRetKey)
    {
        UINT32 slotIndex;
        UINT32 exchangeVal;
        UINT32 valResult;
        bool   doWait;

        K2_ASSERT(apRetKey != NULL);

        //
        // check value at current consume count to see if it is 'full'
        //
        do
        {
            slotIndex = *mpConsumerCountAndActiveFlag;

            if (0 != ((*mpSharedOwnerMask) & (1ull << (slotIndex & IPC_CHANNEL_COUNT_MASK))))
            {
                exchangeVal = ((slotIndex + 1) & IPC_CHANNEL_COUNT_MASK) | IPC_CHANNEL_CONSUMER_ACTIVE_FLAG;
                valResult = InterlockedCompareExchange(mpConsumerCountAndActiveFlag, exchangeVal, slotIndex);
                if (valResult != slotIndex)
                    break;
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
                    if (valResult != slotIndex)
                    {
                        //
                        // we successfully cleared the active flag
                        //
                        if (0 != ((*mpSharedOwnerMask) & (1ull << exchangeVal)))
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
                            } while (valResult == slotIndex);

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
                    mpSystem->SysCall(SYSCALL_ID_WAIT_FOR_NOTIFY, (UINT_PTR)mpConsumerNotify, 0);
            }

        } while (true);

        slotIndex &= ~IPC_CHANNEL_CONSUMER_ACTIVE_FLAG;

        *apRetKey = slotIndex;

        return mpConsumerRoViewOfBuffer + (mEntryBytes * slotIndex);
    }

    void ConsumeRelease(UINT32 aKey)
    {
        UINT64 ownerMask64;
        UINT64 exchangeVal64;
        UINT64 valResult64;

        K2_ASSERT(aKey < 64);

        //
        // set the slot to 'empty' status so producer can produce into it again
        //
        do
        {
            ownerMask64 = *mpSharedOwnerMask;
            K2_ASSERT(0 != (ownerMask64 & (1ull << aKey)));
            exchangeVal64 = ownerMask64 & ~(1ull << aKey);
            valResult64 = InterlockedCompareExchange64((volatile LONG64 *)mpSharedOwnerMask, exchangeVal64, ownerMask64);
        } while (valResult64 != ownerMask64);
    }
};


extern HANDLE   theWin32ExitSignal;

#endif // __SIMKERN_H