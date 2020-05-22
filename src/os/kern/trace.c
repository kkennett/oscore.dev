//   
//   BSD 3-Clause License
//   
//   Copyright (c) 2020, Kurt Kennett
//   All rights reserved.
//   
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//   
//   1. Redistributions of source code must retain the above copyright notice, this
//      list of conditions and the following disclaimer.
//   
//   2. Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//   
//   3. Neither the name of the copyright holder nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//   
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "kern.h"

#if TRACING_ON

static char const * const sgpNames[] =
{
    "<ZERO>",
    "CPUCORE_SCHED_CALL          ",
    "SCHED_EXEC_ITEM             ",
    "SCHED_ARM_TIMER             ",
    "SCHED_ENTERED               ",
    "SCHED_LEFT                  ",
    "SCHED_CALL_CLEAR_ACTIVE     ",
    "SCHED_STOP_CLEAR_ACTIVE     ",
    "CPUCORE_TIMER_FIRED         ",
    "CPUCORE_THREAD_STOP         ",
    "CPUCORE_WAKE_UP             ",
    "CPUCORE_ICI_STOP_THREAD     ",
    "CPUCORE_ICI_STOP_NOTHREAD   ",
    "CPUCORE_ICI_TLBINV          ",
    "CPUCORE_ICI_PAGEDIR         ",
    "CPUCORE_ICI_PANIC           ",
    "CPUCORE_ICI_DEBUG           ",
    "MONITOR_END_IDLE            ",
    "MONITOR_ENTER               ",
    "MONITOR_SET_THREAD_ACTIVE   ",
    "MONITOR_START_IDLE          ",
    "MONITOR_RESUME_THREAD       ",
    "X32INTR_MONITOR_ENTER       ",
    "THREAD_START_WAIT           ",
    "THREAD_END_WAIT             ",
    "THREAD_SEC_WAIT             ",
    "THREAD_ENTERED_SEC          ",
    "THREAD_LEFT_SEC             "
};

void K2OSKERN_Trace(UINT32 aTime, UINT32 aCode, UINT32 aCount, ...)
{
    UINT32  dec;
    UINT32  cur;
    UINT32  v;
    VALIST  argPtr;
    BOOL    disp;

    if (!gData.mTraceStarted)
        return;

    dec = (3 + aCount);

    disp = K2OSKERN_SetIntr(FALSE);
    do {
        K2OSKERN_SetIntr(disp);
        //
        // if intr was enabled when we started func, then 
        // interrupts are on here while we spin
        //
        cur = gData.mTrace;
        v = cur - (dec * sizeof(UINT32));
        if (v < gData.mTraceBottom)
            return;
        //
        // turn off interrupts while we try to update trace
        // pointer.  if we successfully do it then interrupts stay off
        // and we put them back after we've written our stuff.  this is
        // done so that we don't get interrupted in the middle of a trace
        // write.  This is not a seqLock - so we're not blocking other 
        // cores here. 
        disp = K2OSKERN_SetIntr(FALSE);
    } while (cur != K2ATOMIC_CompareExchange(&gData.mTrace, v, cur));

    cur -= sizeof(UINT32);
    *((UINT32 *)cur) = dec;

    cur -= sizeof(UINT32);
    *((UINT32 *)cur) = aTime;

    cur -= sizeof(UINT32);
    *((UINT32 *)cur) = aCode;

    if (aCount > 0)
    {
        K2_VASTART(argPtr, aCount);
        do {
            cur -= sizeof(UINT32);
            *((UINT32 *)cur) = K2_VAARG(argPtr, UINT32);
        } while (--aCount);

        K2_VAEND(argPtr);
    }

    K2OSKERN_SetIntr(disp);
}

void K2OSKERN_TraceDump(void)
{
    UINT32 cur;
    UINT32 count;
    UINT32 time;
    UINT32 code;

    K2OSKERN_SetIntr(FALSE);

    cur = gData.mTraceTop;
    while (cur > gData.mTrace)
    {
        cur -= sizeof(UINT32);
        count = *((UINT32 *)cur);
        count--;

        cur -= sizeof(UINT32);
        time = *((UINT32 *)cur);
        count--;

        cur -= sizeof(UINT32);
        code = *((UINT32 *)cur);
        count--;

        K2OSKERN_Debug("%8d %s", time, sgpNames[code]);

        if (count > 0)
        {
            do {
                cur -= sizeof(UINT32);
                K2OSKERN_Debug(" %08X", *((UINT32 *)cur));
            } while (--count);
        }
        K2OSKERN_Debug("\n");
    }
}

#endif
