#include "SimKern.h"

HANDLE  theWin32ExitSignal = NULL;
DWORD   theThreadSelfSlot = TLS_OUT_OF_INDEXES;

K2STAT SK_SystemCall(UINT_PTR aCallId, UINT_PTR aArg1, UINT_PTR aArg2, UINT_PTR *apResultValue)
{
    SKThread *pThisThread = (SKThread *)TlsGetValue(theThreadSelfSlot);
    return pThisThread->mpSystem->SysCall(aCallId, aArg1, aArg2, apResultValue);
}

void SKSystem::SendThreadToCpu(DWORD aTargetCpu, SKThread *apThread)
{
    SKCpu *pTarget = &mpCpus[aTargetCpu];
    SKCpu *pThisCpu = GetCurrentCpu();

    K2_ASSERT(NULL == apThread->mpCurrentCpu);
    K2_ASSERT(apThread->mState == SKThreadState_Migrating);

    if (pThisCpu == pTarget)
    {
        // adding this at the end will place it after the idle thread, which guarantees a reschedule calc
        // before it executes
        apThread->mState = SKThreadState_OnRunList;
        apThread->mpCurrentCpu = pThisCpu;
        K2LIST_AddAtTail(&pThisCpu->RunningThreadList, &apThread->CpuThreadListLink);
    }
    else
    {
        SKThread * pLast;
        SKThread * pOld;
        do
        {
            pLast = pTarget->mpMigratedHead;
            K2_CpuWriteBarrier();
            apThread->mpCpuMigratedNext = pLast;
            K2_CpuReadBarrier();
            pOld = (SKThread *)InterlockedCompareExchangePointer((PVOID volatile *)&pTarget->mpMigratedHead, apThread, (PVOID)pLast);
        } while (pOld != pLast);

        pThisCpu->SendIci(pTarget->mCpuIndex, SKICI_CODE_MIGRATED_THREAD);
    }
}

static void SKKernel_RecvIcis(SKCpu *apThisCpu)
{
    SKICI * pIciList;
    SKICI * pTemp;
    SKICI * pHold;

    do
    {
        pIciList = (SKICI *)InterlockedExchangePointer((PVOID volatile *)&apThisCpu->mpPendingIcis, NULL);
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

            apThisCpu->OnRecvIci(pHold->mpSenderCpu, pHold->mCode);

            delete pHold;
            
        } while (NULL != pIciList);

    } while (true);
}

void SKCpu::SendIci(DWORD aTargetCpu, UINT_PTR aCode)
{
    SKCpu *pTarget = &mpSystem->mpCpus[aTargetCpu];
    
    SKICI *pIci = new SKICI;
    if (NULL == pIci)
    {
        printf("Memory allocation failed generating Ici on cpu %d\n", mCpuIndex);
        ExitProcess(__LINE__);
    }
    
    pIci->mCode = aCode;
    pIci->mpSenderCpu = this;

    SKICI * pLast;
    SKICI * pOld;
    do
    {
        pLast = pTarget->mpPendingIcis;
        K2_CpuWriteBarrier();
        pIci->mpNext = pLast;
        K2_CpuReadBarrier();
        pOld = (SKICI *)InterlockedCompareExchangePointer((PVOID volatile *)&pTarget->mpPendingIcis, pIci, (PVOID)pLast);
    } while (pOld != pLast);

    if (((DWORD)-1) == SetEvent(pTarget->mhWin32IciEvent))
    {
        printf("Could not fire action event for cpu %d from cpu %d\n", pTarget->mCpuIndex, mCpuIndex);
        ExitProcess(__LINE__);
    }
}

void SKCpu::SetNextThread(void)
{
    SKThread *pThread = K2_GET_CONTAINER(SKThread, RunningThreadList.mpHead, CpuThreadListLink);
    K2_ASSERT(pThread != mpCurrentThread);
    K2_ASSERT(pThread->mState == SKThreadState_OnRunList);
    K2_ASSERT(pThread->mpCurrentCpu == this);
    if (pThread == mpIdleThread)
        mSchedTimeout.QuadPart = 0;
    else
        mpSystem->MsToCpuTime(pThread->mQuantum, &mSchedTimeout);
    mpCurrentThread = pThread;
    mThreadChanged = TRUE;
}

static DWORD WINAPI SKKernelThread(void *apParam)
{
    SKCpu *         pThisCpu = (SKCpu *)apParam;
    DWORD           lastWaitResult;
    HANDLE          waitHandles[3];
    DWORD           timeOutMs;
    LARGE_INTEGER   timeLast;
    LARGE_INTEGER   timeDelta;
    LARGE_INTEGER * pTimeNow;
    BOOL            ranThread;

    pThisCpu->OnStartup();

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
                //
                // in order to run the idle thread nothing else must be able to run on this cpu
                //
                K2_ASSERT(pThisCpu->RunningThreadList.mpHead == &pThisCpu->mpIdleThread->CpuThreadListLink);
                K2_ASSERT(pThisCpu->RunningThreadList.mpTail == &pThisCpu->mpIdleThread->CpuThreadListLink);
//                printf("CPU %d ENTER IDLE\n", pThisCpu->mCpuIndex);
            }
            if (((DWORD)-1) == ResumeThread(pThisCpu->mpCurrentThread->mhWin32Thread))
            {
                printf("Could not resume current thread of CPU %d\n", pThisCpu->mCpuIndex);
                ExitProcess(__LINE__);
            }

            ranThread = TRUE;
            if (0 != pThisCpu->mSchedTimeout.QuadPart)
                timeOutMs = pThisCpu->mpSystem->CpuTimeToMs(&pThisCpu->mSchedTimeout);
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
//                printf("CPU %d EXIT IDLE\n", pThisCpu->mCpuIndex);
            }
        }

        QueryPerformanceCounter(pTimeNow);
        timeDelta.QuadPart = pTimeNow->QuadPart - timeLast.QuadPart;
        timeLast.QuadPart = pTimeNow->QuadPart;
        if (ranThread)
        {
            pThisCpu->mpCurrentThread->mRunTime.QuadPart += timeDelta.QuadPart;
        }
        pThisCpu->mSchedTimerExpired = FALSE;
        if (0 != pThisCpu->mSchedTimeout.QuadPart)
        {
            //
            // scheduling timer on this CPU is running
            //
            if (timeDelta.QuadPart > pThisCpu->mSchedTimeout.QuadPart)
            {
                pThisCpu->mSchedTimeout.QuadPart = 0;
                pThisCpu->mSchedTimerExpired = TRUE;
            }
            else
            {
                pThisCpu->mSchedTimeout.QuadPart -= timeDelta.QuadPart;
            }
        }

        pThisCpu->mThreadChanged = FALSE;

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
            pThisCpu->OnIrqInterrupt();
        }
        else if (lastWaitResult == WAIT_OBJECT_0 + 2)
        {
            // System Call from current thread
            if (pThisCpu->mpCurrentThread == pThisCpu->mpIdleThread)
            {
                printf("Idle thread did system call on CPU %d\n", pThisCpu->mCpuIndex);
                ExitProcess(__LINE__);
            }

            pThisCpu->OnSystemCall();
        }

        if (pThisCpu->mSchedTimerExpired)
        {
            pThisCpu->mSchedTimerExpired = FALSE;
            pThisCpu->OnSchedTimerExpiry();
        }

        if (pThisCpu->mpCurrentThread == pThisCpu->mpIdleThread)
        {
            pThisCpu->OnReschedule();
        }

    } while (true);

    pThisCpu->OnShutdown();

    return 0;
}

void SetupThread(SKThread *apNewThread)
{
    //
    // win32 thread must have completed startup and entered its thread routine
    // before SuspendThread() will work reliably.  So these shenanigans are 
    // necessary when starting a new thread
    //
    apNewThread->mhWin32StartupMutex = CreateMutex(NULL, TRUE, NULL);
    if (NULL == apNewThread->mhWin32StartupMutex)
    {
        printf("failed to create thread 1 startup event\n");
        ExitProcess(__LINE__);
    }
    apNewThread->mhWin32StartupEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == apNewThread->mhWin32StartupEvent)
    {
        printf("failed to create thread 1 startup event\n");
        ExitProcess(__LINE__);
    }
    if (((DWORD)-1) == ResumeThread(apNewThread->mhWin32Thread))
    {
        printf("failed to resume thread 1\n");
        ExitProcess(__LINE__);
    }
    if (WAIT_OBJECT_0 != WaitForSingleObject(apNewThread->mhWin32StartupEvent, 10000))
    {
        printf("failed to wait for thread 1 to start\n");
        ExitProcess(__LINE__);
    }
    CloseHandle(apNewThread->mhWin32StartupEvent);
    apNewThread->mhWin32StartupEvent = NULL;
    if (((DWORD)-1) == SuspendThread(apNewThread->mhWin32Thread))
    {
        printf("failed to suspend thread 1\n");
        ExitProcess(__LINE__);
    }
    ReleaseMutex(apNewThread->mhWin32StartupMutex);
}

static DWORD WINAPI sIdleThread(void *apParam)
{
    SKThread *pThisThread = (SKThread *)apParam;
    TlsSetValue(theThreadSelfSlot, apParam);

    SetEvent(pThisThread->mhWin32StartupEvent);
    WaitForSingleObject(pThisThread->mhWin32StartupMutex, INFINITE);
    ReleaseMutex(pThisThread->mhWin32StartupMutex);
    CloseHandle(pThisThread->mhWin32StartupMutex);
    pThisThread->mhWin32StartupMutex = NULL;

    do
    {
        Sleep(1000);
    } while (true);

    return 0;
}

static void SKInitCpu(SKSystem *apSystem, DWORD aCpuIndex)
{
    SKCpu *     pCpu;
    SKThread *  pIdleThread;
    
    pCpu = &apSystem->mpCpus[aCpuIndex];
    pCpu->mpSystem = apSystem;
    pCpu->mCpuIndex = aCpuIndex;

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

    pCpu->mpIdleThread = pIdleThread = new SKThread(apSystem);
    pIdleThread->mhWin32Thread = CreateThread(
        NULL, 
        0, 
        sIdleThread, 
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

    K2LIST_AddAtTail(&apSystem->mpProcess0->ThreadList, &pIdleThread->ProcessThreadListLink);
    pIdleThread->mpOwnerProcess = apSystem->mpProcess0;
    pCpu->mpIdleThread->mpCurrentCpu = pCpu;
    pIdleThread->mState = SKThreadState_OnRunList;
    K2LIST_AddAtHead(&pCpu->RunningThreadList, &pIdleThread->CpuThreadListLink);
    SetupThread(pIdleThread);

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

static SKSystem *   sgpSystem;

static void Startup(void)
{
    UINT_PTR ix;
    UINT_PTR cpuCount;

    theWin32ExitSignal = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (NULL == theWin32ExitSignal)
    {
        printf("failed to create exit signal\n");
        ExitProcess(__LINE__);
    }

    cpuCount = GetMaximumProcessorCount(ALL_PROCESSOR_GROUPS);
    if (cpuCount > 64)
        cpuCount = 64;

    cpuCount = 1;

    sgpSystem = new SKSystem(cpuCount);
    if (NULL == sgpSystem)
    {
        printf("failed to allocate system\n");
        ExitProcess(__LINE__);
    }

    sgpSystem->mpCpus = new SKCpu[cpuCount];
    if (NULL == sgpSystem->mpCpus)
    {
        printf("failed to create cpu array\n");
        ExitProcess(__LINE__);
    }

    if (FALSE == QueryPerformanceFrequency(&sgpSystem->mPerfFreq))
    {
        printf("failed to get core perf frequency\n");
        ExitProcess(__LINE__);
    }

    theThreadSelfSlot = TlsAlloc();
    if (TLS_OUT_OF_INDEXES == theThreadSelfSlot)
    {
        printf("failed to thread self tls slot\n");
        ExitProcess(__LINE__);
    }

    sgpSystem->mpProcess0 = new SKProcess(sgpSystem, 0);
    if (NULL == sgpSystem->mpProcess0)
    {
        printf("failed to create process 0\n");
        ExitProcess(__LINE__);
    }
    K2LIST_AddAtTail(&sgpSystem->ProcessList, &sgpSystem->mpProcess0->SystemProcessListLink);

    for (ix = 0; ix < sgpSystem->mCpuCount; ix++)
    {
        SKInitCpu(sgpSystem, (DWORD)ix);
    }
}

static DWORD WINAPI sInitThread(void *apParam)
{
    SKThread *pThisThread = (SKThread *)apParam;

    TlsSetValue(theThreadSelfSlot, pThisThread);

    SetEvent(pThisThread->mhWin32StartupEvent);
    WaitForSingleObject(pThisThread->mhWin32StartupMutex, INFINITE);
    ReleaseMutex(pThisThread->mhWin32StartupMutex);
    CloseHandle(pThisThread->mhWin32StartupMutex);
    pThisThread->mhWin32StartupMutex = NULL;

    // thread has really started at this point

    SK_SystemCall(SYSCALL_ID_SLEEP, 3000, 0, NULL);

    SK_SystemCall(SYSCALL_ID_THREAD_EXIT, 0, 0, NULL);


    return 0;
}

static void Run(void)
{
    //
    // push the first thread into the system
    //
    SKThread *pNewThread;
    pNewThread = new SKThread(sgpSystem);
    if (pNewThread == NULL)
    {
        printf("failed to create thread 1\n");
        ExitProcess(__LINE__);
    }
    pNewThread->mhWin32Thread = CreateThread(NULL, 0, sInitThread, pNewThread, CREATE_SUSPENDED, &pNewThread->mWin32ThreadId);
    pNewThread->mState = SKThreadState_Migrating;
    pNewThread->mpOwnerProcess = sgpSystem->mpProcess0;
    K2LIST_AddAtTail(&sgpSystem->mpProcess0->ThreadList, &pNewThread->ProcessThreadListLink);

    //
    // we are sending the thread from "outside the system" :)  so we need to
    // add it via the foreign CPU mechanism and not via SendThreadToCpu
    //
    SKCpu *pTarget = &sgpSystem->mpCpus[0];
    pTarget->mpMigratedHead = pNewThread;
    K2_CpuWriteBarrier();
    pTarget->SendIci(0, SKICI_CODE_MIGRATED_THREAD);

    WaitForSingleObject(theWin32ExitSignal, INFINITE);
    printf("Run exiting\n");
}

static void Shutdown(void)
{

}

int main(int argc, char** argv)
{
    Startup();
    Run();
    Shutdown();
    return 0;
}