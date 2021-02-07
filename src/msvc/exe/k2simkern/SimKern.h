#ifndef __SIMKERN_H
#define __SIMKERN_H

#include <lib/k2win32.h>
#include <lib/k2mem.h>
#include <lib/k2list.h>

struct  SKSystem;
struct  SKCpu;
struct  SKThread;
struct  SKICI;
enum    SKThreadState;
struct  SKPort;
struct  SKProcess;

#define SKICI_CODE_MIGRATED_THREAD  0xFEED0001

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

struct SKPort
{
    SKPort(void)
    {
        mpOwnerProcess = NULL;
        mpWaitingThread = NULL;
    }
    SKProcess *     mpOwnerProcess;
    SKThread *      mpWaitingThread;
};

struct SKProcess
{
    SKProcess(UINT32 aId) : mId(aId)
    {
        K2LIST_Init(&ThreadList);
        K2MEM_Zero(&SystemProcessListLink, sizeof(SystemProcessListLink));
    }
    UINT32 const    mId;
    K2LIST_LINK     SystemProcessListLink;
    K2LIST_ANCHOR   ThreadList;
};

struct SKThread
{
    enum State
    {
        Invalid=0,
        Inception,
        Suspended,
        OnRunList,
        Migrating,
        BlockedOnPort,
        Dying,
        Dead
    };

    SKThread(void)
    {
        mhWin32Thread = NULL;
        mWin32ThreadId = 0;
        mpOwnerProcess = NULL;
        mState = SKThread::State::Inception;
        mRunTime.QuadPart = 0;
        mQuantum = 0;
        mpCurrentCpu = NULL;
        mpCpuMigratedNext = NULL;
        K2MEM_Zero(&ProcessThreadListLink, sizeof(ProcessThreadListLink));
        K2MEM_Zero(&CpuThreadListLink, sizeof(CpuThreadListLink));
    }

    HANDLE          mhWin32Thread;
    DWORD           mWin32ThreadId;

    SKProcess *     mpOwnerProcess;
    K2LIST_LINK     ProcessThreadListLink;

    State           mState;

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
        mpMigratedListHead = NULL;
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

    volatile SKICI *    mpPendingIcis;

    // can only be modified by its own kernel thread
    K2LIST_ANCHOR       RunningThreadList;

    SKThread volatile * mpMigratedListHead;

    void SendIci(DWORD aTargetCpu, UINT_PTR aCode);

    void OnStartup(void);
    void OnShutdown(void);
    void OnRecvIci(SKCpu *apSenderCpu, UINT_PTR aCode);
    void OnIrqInterrupt(void);
    void OnSchedTimerExpiry(void);
    void OnSystemCall(void);
};

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

struct SKSystem
{
    SKSystem(UINT_PTR aCpuCount) : mCpuCount(aCpuCount)
    {
        mpCpus = NULL;
        mPerfFreq.QuadPart = 0;
        K2LIST_Init(&ProcessList);
    }

    void MsToCpuTime(DWORD aMilliseconds, LARGE_INTEGER *apRetCpuTime)
    {
        apRetCpuTime->QuadPart = (((LONGLONG)aMilliseconds) * mPerfFreq.QuadPart) / 1000ll;
    }

    DWORD CpuTimeToMs(LARGE_INTEGER const *apCpuTime)
    {
        return (DWORD)((apCpuTime->QuadPart * 1000ll) / mPerfFreq.QuadPart);
    }

    UINT_PTR const  mCpuCount;
    SKCpu      *    mpCpus;

    LARGE_INTEGER   mPerfFreq;

    K2LIST_ANCHOR   ProcessList;
};

extern HANDLE   theWin32ExitSignal;

#endif // __SIMKERN_H