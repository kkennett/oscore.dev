#ifndef __SIMKERN_H
#define __SIMKERN_H

#include <lib/k2win32.h>
#include <lib/k2list.h>

typedef struct _SKSystem    SKSystem;
typedef struct _SKCpu       SKCpu;
typedef struct _SKThread    SKThread;

#define SKICI_CODE_MIGRATED_THREAD  0xFEED0001

typedef struct _SKICI SKICI;
struct _SKICI
{
    SKCpu *             mpSenderCpu;
    UINT_PTR            mCode;
    volatile SKICI *    mpNext;
};

struct _SKThread
{
    HANDLE          mhWin32Thread;
    DWORD           mWin32ThreadId;

    LARGE_INTEGER   mRunTime;

    DWORD           mQuantum;

    // can only be modified by mpCurrentCpu
    SKCpu *         mpCurrentCpu;
    K2LIST_LINK     CpuThreadListLink;

    // Migrated threads are NOT on the Cpu's thread list (yet)
    // and will have a NULL mpCurrentCpu
    SKThread volatile * mpMigratedNext;
};

struct _SKCpu
{
    SKSystem *          mpSystem;
    DWORD               mCpuIndex;

    HANDLE              mhWin32IciEvent;
    HANDLE              mhWin32IrqEvent;
    HANDLE              mhWin32SysCallEvent;

    HANDLE              mhWin32KernelThread;
    DWORD               mWin32KernelThreadId;

    SKThread *          mpCurrentThread;

    SKThread *          mpIdleThread;

    LARGE_INTEGER       mSchedTimeout;
    LARGE_INTEGER       mTimeNow;

    volatile SKICI *    mpPendingIcis;

    // can only be modified by its own kernel thread
    K2LIST_ANCHOR       ThreadList;

    SKThread volatile * mpMigratedList;
};

typedef struct _SKSpinLock SKSpinLock;
struct _SKSpinLock
{
    CRITICAL_SECTION    mWin32Sec;
};

struct _SKSystem
{
    UINT_PTR        mCpuCount;
    SKCpu      *    mpCpus;
    LARGE_INTEGER   mPerfFreq;
};

extern HANDLE   theWin32ExitSignal;
extern SKSystem theSystem;

void SKSpinLock_Init(SKSpinLock *apLock);
void SKSpinLock_Lock(SKSpinLock *apLock);
void SKSpinLock_Unlock(SKSpinLock *apLock);

void SKKernel_SendIci(SKCpu *apThisCpu, DWORD aTargetCpu, UINT_PTR aCode);
void SKKernel_MsToCpuTime(DWORD aMilliseconds, LARGE_INTEGER *apCpuTime);
DWORD SKKernel_CpuTimeToMs(LARGE_INTEGER *apCpuTime);

void SKCpu_OnStartup(SKCpu *apThisCpu);
void SKCpu_OnShutdown(SKCpu *apThisCpu);
void SKCpu_OnRecvIci(SKCpu *apThisCpu, SKCpu *apSenderCpu, UINT_PTR aCode);
void SKCpu_OnIrqInterrupt(SKCpu *apThisCpu);
void SKCpu_OnSchedTimerExpiry(SKCpu *apThisCpu);
void SKCpu_OnSystemCall(SKCpu *apThisCpu);

#endif // __SIMKERN_H