#ifndef __SIMKERN_H
#define __SIMKERN_H

#include <lib/k2win32.h>
#include <lib/k2list.h>

typedef struct  _SKSystem       SKSystem;
typedef struct  _SKCpu          SKCpu;
typedef struct  _SKThread       SKThread;
typedef struct  _SKICI          SKICI;
typedef enum    _SKThreadState  SKThreadState;
typedef struct  _SKPort         SKPort;
typedef struct  _SKProcess      SKProcess;

#define SKICI_CODE_MIGRATED_THREAD  0xFEED0001

struct _SKICI
{
    SKCpu *             mpSenderCpu;
    UINT_PTR            mCode;
    volatile SKICI *    mpNext;
};

enum _SKThreadState
{
    SKThreadState_Invalid=0,
    SKThreadState_Inception,
    SKThreadState_Suspended,
    SKThreadState_OnRunList,
    SKThreadState_Migrating,
    SKThreadState_BlockedOnPort,
    SKThreadState_Dying,
    SKThreadState_Dead
};

struct _SKPort
{
    SKProcess *     mpOwnerProcess;
    SKThread *      mpWaitingThread;
};

struct _SKProcess
{
    UINT32          mId;
    K2LIST_LINK     SystemProcessListLink;
    K2LIST_ANCHOR   ThreadList;
};

struct _SKThread
{
    HANDLE          mhWin32Thread;
    DWORD           mWin32ThreadId;

    SKProcess *     mpOwnerProcess;
    K2LIST_LINK     ProcessThreadListLink;

    SKThreadState   mThreadState;

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
    K2LIST_ANCHOR       RunningThreadList;

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

    K2LIST_ANCHOR   ProcessList;
};

extern HANDLE   theWin32ExitSignal;

void  SKSpinLock_Init(SKSpinLock *apLock);
void  SKSpinLock_Lock(SKSpinLock *apLock);
void  SKSpinLock_Unlock(SKSpinLock *apLock);

void  SKKernel_SendIci(SKCpu *apThisCpu, DWORD aTargetCpu, UINT_PTR aCode);
void  SKKernel_MsToCpuTime(SKSystem *apSystem, DWORD aMilliseconds, LARGE_INTEGER *apCpuTime);
DWORD SKKernel_CpuTimeToMs(SKSystem *apSystem, LARGE_INTEGER *apCpuTime);

void  SKCpu_OnStartup(SKCpu *apThisCpu);
void  SKCpu_OnShutdown(SKCpu *apThisCpu);
void  SKCpu_OnRecvIci(SKCpu *apThisCpu, SKCpu *apSenderCpu, UINT_PTR aCode);
void  SKCpu_OnIrqInterrupt(SKCpu *apThisCpu);
void  SKCpu_OnSchedTimerExpiry(SKCpu *apThisCpu);
void  SKCpu_OnSystemCall(SKCpu *apThisCpu);

#endif // __SIMKERN_H