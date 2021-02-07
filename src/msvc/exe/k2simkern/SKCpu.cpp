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
    //
    // apThisCpu->mpCurrentThread is making a system call
    // change mpCurrentThread if you want some other thread to run
    // when the function exits
    //
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

