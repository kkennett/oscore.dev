#include "SimKern.h"

HANDLE      theWin32ExitSignal;
static SKSystem theSystem;

void SKKernel_MsToCpuTime(SKSystem *apSystem, DWORD aMilliseconds, LARGE_INTEGER *apCpuTime)
{
    apCpuTime->QuadPart = (((LONGLONG)aMilliseconds) * apSystem->mPerfFreq.QuadPart) / 1000ll;
}

DWORD SKKernel_CpuTimeToMs(SKSystem *apSystem, LARGE_INTEGER *apCpuTime)
{
    return (DWORD)((apCpuTime->QuadPart * 1000ll) / apSystem->mPerfFreq.QuadPart);
}

void SKKernel_RecvIcis(SKCpu *apThisCpu)
{
    SKICI volatile *pIciList;
    SKICI volatile *pTemp;
    SKICI volatile *pHold;

    do
    {
        pIciList = (SKICI volatile *)InterlockedExchangePointer((volatile PVOID *)&apThisCpu->mpPendingIcis, NULL);
        if (NULL == pIciList)
            break;

        //
        // reverse the list so the Icis are processed in the order they arrived
        //
        pHold = NULL;
        do
        {
            pTemp = pIciList->mpNext;
            pIciList->mpNext = pHold;
            pHold = pIciList;
            pIciList = pTemp;
        } while (NULL != pIciList);
        pIciList = pHold;

        //
        // now do the Ici in order
        //
        do
        {
            pHold = pIciList;
            pIciList = pIciList->mpNext;

            SKCpu_OnRecvIci(apThisCpu, pHold->mpSenderCpu, pHold->mCode);

            free((void *)pHold);
            
        } while (NULL != pIciList);

    } while (true);
}

void SKKernel_SendIci(SKCpu *apThisCpu, DWORD aTargetCpu, UINT_PTR aCode)
{
    SKSystem *pSys = apThisCpu->mpSystem;
    
    SKCpu *pTarget = &pSys->mpCpus[aTargetCpu];
    
    SKICI *pIci = (SKICI *)malloc(sizeof(SKICI));
    if (NULL == pIci)
    {
        printf("Memory allocation failed generating Ici on cpu %d\n", apThisCpu->mCpuIndex);
        ExitProcess(__LINE__);
    }
    
    pIci->mCode = aCode;
    pIci->mpSenderCpu = apThisCpu;

    SKICI volatile *pLast;
    SKICI volatile *pOld;
    do
    {
        pLast = pTarget->mpPendingIcis;
        K2_CpuWriteBarrier();
        pIci->mpNext = pLast;
        K2_CpuReadBarrier();
        pOld = (SKICI volatile *)InterlockedCompareExchangePointer((volatile PVOID *)&pTarget->mpPendingIcis, pIci, (PVOID)pLast);
    } while (pOld != pLast);

    if (((DWORD)-1) == SetEvent(pTarget->mhWin32IciEvent))
    {
        printf("Could not fire action event for cpu %d from cpu %d\n", pTarget->mCpuIndex, apThisCpu->mCpuIndex);
        ExitProcess(__LINE__);
    }
}

DWORD WINAPI SKKernelThread(void *apParam)
{
    SKCpu *         pThisCpu = (SKCpu *)apParam;
    DWORD           lastWaitResult;
    HANDLE          waitHandles[3];
    DWORD           timeOutMs;
    LARGE_INTEGER   timeLast;
    LARGE_INTEGER   timeDelta;
    LARGE_INTEGER * pTimeNow;
    BOOL            schedTimerExpired;
    BOOL            ranThread;

    SKCpu_OnStartup(pThisCpu);

    if (FALSE == SetEvent(pThisCpu->mhWin32IciEvent))
    {
        printf("Could not set start event for CPU %d\n", pThisCpu->mCpuIndex);
        ExitProcess(__LINE__);
    }

    // 
    // idle thread is current thread, suspsended
    // 

    waitHandles[0] = pThisCpu->mhWin32IciEvent;
    waitHandles[1] = pThisCpu->mhWin32IrqEvent;
    waitHandles[2] = pThisCpu->mhWin32SysCallEvent;
    pTimeNow = &pThisCpu->mTimeNow;
    QueryPerformanceCounter(pTimeNow);
    timeLast = *pTimeNow;
    lastWaitResult = WAIT_TIMEOUT;
    do
    {
        lastWaitResult = WaitForMultipleObjects(3, waitHandles, FALSE, 0);

        ranThread = FALSE;
        if (lastWaitResult == WAIT_TIMEOUT)
        {
            // 
            // nothing is going on. resume the current thread
            //
            if (pThisCpu->mpCurrentThread == pThisCpu->mpIdleThread)
            {
                printf("CPU %d ENTER IDLE\n", pThisCpu->mCpuIndex);
            }
            if (((DWORD)-1) == ResumeThread(pThisCpu->mpCurrentThread->mhWin32Thread))
            {
                printf("Could not resume current thread of CPU %d\n", pThisCpu->mCpuIndex);
                ExitProcess(__LINE__);
            }

            ranThread = TRUE;
            if (0 != pThisCpu->mSchedTimeout.QuadPart)
                timeOutMs = SKKernel_CpuTimeToMs(pThisCpu->mpSystem, &pThisCpu->mSchedTimeout);
            else
                timeOutMs = INFINITE;
            lastWaitResult = WaitForMultipleObjects(3, waitHandles, FALSE, timeOutMs);

            if (((DWORD)-1) == SuspendThread(pThisCpu->mpCurrentThread->mhWin32Thread))
            {
                printf("Could not suspend current thread on CPU %d\n", pThisCpu->mCpuIndex);
                ExitProcess(__LINE__);
            }
            if (pThisCpu->mpCurrentThread == pThisCpu->mpIdleThread)
            {
                printf("CPU %d EXIT IDLE\n", pThisCpu->mCpuIndex);
            }
        }

        QueryPerformanceCounter(pTimeNow);
        timeDelta.QuadPart = pTimeNow->QuadPart - timeLast.QuadPart;
        timeLast.QuadPart = pTimeNow->QuadPart;
        if (ranThread)
        {
            pThisCpu->mpCurrentThread->mRunTime.QuadPart += timeDelta.QuadPart;
        }
        schedTimerExpired = FALSE;
        if (0 != pThisCpu->mSchedTimeout.QuadPart)
        {
            //
            // scheduling timer on this CPU is running
            //
            if (timeDelta.QuadPart > pThisCpu->mSchedTimeout.QuadPart)
            {
                pThisCpu->mSchedTimeout.QuadPart = 0;
                schedTimerExpired = TRUE;
            }
            else
            {
                pThisCpu->mSchedTimeout.QuadPart -= timeDelta.QuadPart;
            }
        }

        if (lastWaitResult == WAIT_OBJECT_0)
        {
            if (NULL != pThisCpu->mpPendingIcis)
            {
                //
                // handle ICIs first
                //
                SKKernel_RecvIcis(pThisCpu);
            }
        }
        else if (lastWaitResult == WAIT_OBJECT_0 + 1)
        {
            //
            // logical interrupt
            //
            SKCpu_OnIrqInterrupt(pThisCpu);
        }
        else if (lastWaitResult == WAIT_OBJECT_0 + 2)
        {
            // System Call from current thread
            if (pThisCpu->mpCurrentThread == pThisCpu->mpIdleThread)
            {
                printf("Idle thread did system call on CPU %d\n", pThisCpu->mCpuIndex);
                ExitProcess(__LINE__);
            }
            SKCpu_OnSystemCall(pThisCpu);
        }

        if (schedTimerExpired)
            SKCpu_OnSchedTimerExpiry(pThisCpu);

    } while (true);

    SKCpu_OnShutdown(pThisCpu);

    return 0;
}

DWORD WINAPI SKIdleThread(void *apParam)
{
    SKThread *pThisThread = (SKThread *)apParam;
    do
    {
        Sleep(1000);
    } while (true);
    return 0;
}

void 
SKInitCpu(
    SKSystem *  apSystem,
    DWORD       aCpuIndex
)
{
    SKThread *  pIdleThread;
    SKCpu *     pCpu;
    
    pCpu = &apSystem->mpCpus[aCpuIndex];
    pCpu->mpSystem = apSystem;
    pCpu->mCpuIndex = aCpuIndex;
    pCpu->mpPendingIcis = NULL;
    K2LIST_Init(&pCpu->RunningThreadList);

    pCpu->mhWin32IciEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == pCpu->mhWin32IciEvent)
    {
        printf("Could not create Ici event for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
    pCpu->mhWin32IrqEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == pCpu->mhWin32IrqEvent)
    {
        printf("Could not create Irq event for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
    pCpu->mhWin32SysCallEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == pCpu->mhWin32SysCallEvent)
    {
        printf("Could not create syscall event for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }

    pCpu->mpIdleThread = pIdleThread = (SKThread *)malloc(sizeof(SKThread));
    if (NULL == pIdleThread)
    {
        printf("Could not allocate idle thread for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
    pIdleThread->mhWin32Thread = CreateThread(
        NULL, 
        0, 
        SKIdleThread, 
        pIdleThread, 
        CREATE_SUSPENDED, 
        &pIdleThread->mWin32ThreadId);
    if (NULL == pIdleThread->mhWin32Thread)
    {
        printf("Could not create suspended idle thread for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
    if (0 == SetThreadAffinityMask(pIdleThread->mhWin32Thread, ((DWORD_PTR)1) << aCpuIndex))
    {
        printf("Could not set affinity of idle thread for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
    pIdleThread->mpCurrentCpu = pCpu;
    pIdleThread->mRunTime.QuadPart = 0;
    pIdleThread->mQuantum = 0;
    pIdleThread->mpMigratedNext = NULL;
    K2LIST_AddAtHead(&pCpu->RunningThreadList, &pIdleThread->CpuThreadListLink);

    pCpu->mhWin32KernelThread = CreateThread(
        NULL,
        0,
        SKKernelThread,
        pCpu,
        CREATE_SUSPENDED,
        &pCpu->mWin32KernelThreadId);
    if (NULL == pCpu->mhWin32KernelThread)
    {
        printf("Could not create suspended idle thread for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
    if (0 == SetThreadAffinityMask(pCpu->mhWin32KernelThread, ((DWORD_PTR)1) << aCpuIndex))
    {
        printf("Could not set affinity of kernel thread for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
    if (0 == SetThreadPriority(pCpu->mhWin32KernelThread, THREAD_PRIORITY_HIGHEST))
    {
        printf("Could not set priority of kernel thread for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }

    pCpu->mpCurrentThread = pCpu->mpIdleThread;
    pCpu->mSchedTimeout.QuadPart = 0;
    pCpu->mpPendingIcis = NULL;
    K2_CpuWriteBarrier();

    if (((DWORD)-1) == ResumeThread(pCpu->mhWin32KernelThread))
    {
        printf("Could not resume kernel thread for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
    if (WAIT_OBJECT_0 != WaitForSingleObject(pCpu->mhWin32IciEvent, INFINITE))
    {
        printf("Waiting for kernel thread to start failed for CPU %d\n", aCpuIndex);
        ExitProcess(__LINE__);
    }
}

void Startup(void)
{
    UINT_PTR ix;

    theWin32ExitSignal = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == theWin32ExitSignal)
    {
        printf("failed to create exit signal\n");
        ExitProcess(__LINE__);
    }

    theSystem.mCpuCount = GetMaximumProcessorCount(ALL_PROCESSOR_GROUPS);
    if (theSystem.mCpuCount > 64)
        theSystem.mCpuCount = 64;

    theSystem.mpCpus = (SKCpu *)malloc(theSystem.mCpuCount * sizeof(SKCpu));
    if (NULL == theSystem.mpCpus)
    {
        printf("malloc failed for cpu array\n");
        ExitProcess(__LINE__);
    }

    if (FALSE == QueryPerformanceFrequency(&theSystem.mPerfFreq))
    {
        printf("failed to get core perf frequency\n");
        ExitProcess(__LINE__);
    }

    K2LIST_Init(&theSystem.ProcessList);

    for (ix = 0; ix < theSystem.mCpuCount; ix++)
    {
        SKInitCpu(&theSystem, (DWORD)ix);
    }
}

void Run(void)
{
    WaitForSingleObject(theWin32ExitSignal, INFINITE);
    printf("Run exiting\n");
}

void Shutdown(void)
{

}

int main(int argc, char** argv)
{
    Startup();
    Run();
    Shutdown();
    return 0;
}