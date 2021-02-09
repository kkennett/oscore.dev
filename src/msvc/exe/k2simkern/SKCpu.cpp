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

void SKCpu::OnIrqInterrupt(void)
{
    //
    // irq received by this CPU
    // change mpCurrentThread if you want some other thread to run
    // when the function exits
    //
}

void SKCpu::OnSystemCall(void)
{
    SKThread *pCallingThread;

    //
    // apThisCpu->mpCurrentThread is making a system call
    // change mpCurrentThread if you want some other thread to run
    // when the function exits
    //
    pCallingThread = mpCurrentThread;

    K2_ASSERT(pCallingThread->mFlags & THREAD_FLAG_ENTER_SYSTEM_CALL);
    pCallingThread->mFlags &= ~THREAD_FLAG_ENTER_SYSTEM_CALL;

    switch (pCallingThread->mSysCall_Id)
    {
    case SYSCALL_ID_SIGNAL_NOTIFY:
        mpSystem->InsideSysCall_SignalNotify((SKNotify *)pCallingThread->mSysCall_Arg1, pCallingThread->mSysCall_Arg2);
        break;

    case SYSCALL_ID_WAIT_FOR_NOTIFY:
        mpSystem->InsideSysCall_WaitForNotify((SKNotify *)pCallingThread->mSysCall_Arg1);
        break;

    case SYSCALL_ID_SLEEP:

        break;

    default:
        pCallingThread->mSysCall_Result = (UINT_PTR)-1;
        break;
    };

    if (mReschedFlag)
    {
        //
        // run the scheduler for the threads on this cpu to set the proper mpCurrentThread
        //
    }
}

void SKCpu::OnSchedTimerExpiry(void)
{
    //
    // this current CPU's local timer expired
    // change mpCurrentThread if you want some other thread to run
    // when the function exits
    //
}

void SKCpu::OnRecvIci(SKCpu *apSenderCpu, UINT_PTR aCode)
{
    //
    // we received an ICI from the sender Cpu with the corresponding code
    // change mpCurrentThread if you want some other thread to run
    // when the function exits
    //
}

