#include "SimKern.h"

void SKCpu::OnStartup(void)
{
    //
    // main cpu kernel thread startup - 
    // idle thread is current
    //
}

void SKCpu::OnShutdown(void)
{
    //
    // main cpu loop is exiting in kernel thread
    //
}

void SKCpu::OnIrq_Timer(void)
{
    SKSystem *pSystem = mpSystem;
    SKTimerItem *pSignalList;
    SKTimerItem *pSignalListEnd;
    SKTimerItem *pTimerItem;
    UINT32 ixTick;

    //
    // called every SCHED_TICK_GRANULARITY milliseconds while the timer is running
    //
    pSignalList = pSignalListEnd = NULL;

    pSystem->TimerQueueLock.Lock();

    //
    // only process if it is not a superfluous timer interrupt that occurred in between
    // the last timer interrupt and when the timer was disabled
    //
    if (NULL != pSystem->mhWin32TimerQueueTimer)
    {
        pTimerItem = pSystem->mpTimerQueueHead;

        if (NULL != pTimerItem)
        {
            K2_ASSERT(pTimerItem->mDeltaT > 0);

            for (ixTick = 0; ixTick < SCHED_TICK_GRANULARITY; ixTick++)
            {
                if (0 == --pTimerItem->mDeltaT)
                {
                    do
                    {
                        pSystem->mpTimerQueueHead = pTimerItem->mpNext;

                        // 
                        // timer item has elapsed.
                        //
                        pTimerItem->mpNext = NULL;
                        if (NULL == pSignalList)
                        {
                            pSignalList = pSignalListEnd = pTimerItem;
                        }
                        else
                        {
                            pSignalListEnd->mpNext = pTimerItem;
                            pSignalListEnd = pTimerItem;
                        }

                        pTimerItem = pSystem->mpTimerQueueHead;

                    } while ((NULL != pTimerItem) && (pTimerItem->mDeltaT == 0));
                }
            }
        }

        if (NULL == pSystem->mpTimerQueueHead)
        {
            if (FALSE == DeleteTimerQueueTimer(NULL, pSystem->mhWin32TimerQueueTimer, INVALID_HANDLE_VALUE))
            {
                printf("DeleteTimerQueueTimer failed\n");
                ExitProcess(__LINE__);
            }

            pSystem->mhWin32TimerQueueTimer = NULL;
        }
    }

    pSystem->TimerQueueLock.Unlock();

    if (NULL == pSignalList)
        return;

    do
    {
        pTimerItem = pSignalList;
        pSignalList = pSignalList->mpNext;
        pSystem->SignalTimerItem(pTimerItem);
    } while (pSignalList);
}

void SKCpu::OnIrqInterrupt(void)
{
    //
    // irq received by this CPU
    // change mpCurrentThread if you want some other thread to run
    // when the function exits
    //
    UINT32 irqStatus;
    UINT32 irqNew;
    UINT32 irqBit;
    DWORD  irqIndex;
    UINT32 irqResult;

    do
    {
        do
        {
            irqStatus = mIrqStatus;
            if (0 == irqStatus)
                return;
            irqBit = (irqStatus ^ (irqStatus & (irqStatus - 1)));
            irqNew = irqStatus & ~irqBit;
            irqResult = InterlockedCompareExchange(&mIrqStatus, irqNew, irqStatus);
        } while (irqResult != irqStatus);

        //
        // we just stripped the lowest bit that was set from the current irq Status
        //
        _BitScanForward(&irqIndex, irqBit);

        switch (irqIndex)
        {
        case SKIRQ_INDEX_TIMER:
            //
            // timer interrupt
            //
            OnIrq_Timer();
            break;
        default:
            K2_ASSERT(0);
            break;
        }
    } while (true);
}

void SKCpu::OnSystemCall(void)
{
    SKThread *pCallingThread;

    //
    // apThisCpu->mpCurrentThread is making a system call
    //
    pCallingThread = mpCurrentThread;

    K2_ASSERT(pCallingThread->mFlags & THREAD_FLAG_ENTER_SYSTEM_CALL);
    pCallingThread->mFlags &= ~THREAD_FLAG_ENTER_SYSTEM_CALL;

    //
    // result value and status may not be known at this time. set defaults
    //
    pCallingThread->mSysCall_ResultValue = 0;
    pCallingThread->mSysCall_ResultStatus = K2STAT_ERROR_UNSUPPORTED;

    switch (pCallingThread->mSysCall_Id)
    {
    case SYSCALL_ID_THREAD_EXIT:
        mpSystem->InsideSysCall_ThreadExit(this, pCallingThread);
        break;

    case SYSCALL_ID_SIGNAL_NOTIFY:
        mpSystem->InsideSysCall_SignalNotify(this, pCallingThread);
        break;

    case SYSCALL_ID_WAIT_FOR_NOTIFY:
        mpSystem->InsideSysCall_WaitForNotify(this, pCallingThread, false);
        break;

    case SYSCALL_ID_POLL_FOR_NOTIFY:
        mpSystem->InsideSysCall_WaitForNotify(this, pCallingThread, true);
        break;

    case SYSCALL_ID_SLEEP:
        mpSystem->InsideSysCall_Sleep(this, pCallingThread);
        break;

    default:
        break;
    };
}

void SKCpu::OnSchedTimerExpiry(void)
{
    //
    // this current CPU's local timer expired
    //
    if (!mThreadChanged)
    {
        K2LIST_Remove(&RunningThreadList, &mpCurrentThread->CpuThreadListLink);
        K2LIST_AddAtTail(&RunningThreadList, &mpCurrentThread->CpuThreadListLink);
        SetNextThread();
    }
}

void SKCpu::OnRecvIci(SKCpu *apSenderCpu, UINT_PTR aCode)
{
    SKThread * pThreadList;

    //
    // we received an ICI from the sender Cpu with the corresponding code
    //
    switch (aCode)
    {
    case SKICI_CODE_MIGRATED_THREAD:
        //
        // push migrated threads onto the end of the run list.  they will be after the idle thread
        // and so the reschedule will set their quanta before they actually run
        //
        pThreadList = (SKThread *)InterlockedExchangePointer((volatile PVOID *)&mpMigratedHead, NULL);
        while (pThreadList)
        {
            K2LIST_AddAtTail(&RunningThreadList, &pThreadList->CpuThreadListLink);
            if (0 == SetThreadAffinityMask(pThreadList->mhWin32Thread, (1 << mCpuIndex)))
            {
                printf("Could not set thread affinity mask\n");
                ExitProcess(__LINE__);
            }
            pThreadList->mpCurrentCpu = this;
            pThreadList->mState = SKThreadState_OnRunList;

            pThreadList = pThreadList->mpCpuMigratedNext;
        }
        break;
    default:
        K2_ASSERT(0);
        break;
    }
}

void SKCpu::OnReschedule(void)
{
    K2LIST_LINK *   pListLink;
    SKThread *      pThread;

    //
    // should only ever be here if the idle thread is about to run
    //
    K2_ASSERT(mpIdleThread == mpCurrentThread);

    //
    // if the only thing to run is the idle thread, let it go
    //
    if (1 == RunningThreadList.mNodeCount)
    {
        mSchedTimeout.QuadPart = 0;
        return;
    }

    //
    // reschedule
    //

    K2LIST_Remove(&RunningThreadList, &mpIdleThread->CpuThreadListLink);

    pListLink = RunningThreadList.mpHead;
    K2_ASSERT(NULL != pListLink);

    if (1 == RunningThreadList.mNodeCount)
    {
        //
        // only one thread, so don't preempt it
        //
        pThread = K2_GET_CONTAINER(SKThread, pListLink, CpuThreadListLink);
        pThread->mQuantum = 0;
    }
    else
    {
        //
        // more than one thread, so we need to use preemption
        //
        do
        {
            pThread = K2_GET_CONTAINER(SKThread, pListLink, CpuThreadListLink);
            pListLink = pListLink->mpNext;
            pThread->mQuantum = 100;
        } while (NULL != pListLink);
    }

    K2LIST_AddAtTail(&RunningThreadList, &mpIdleThread->CpuThreadListLink);

    SetNextThread();
}