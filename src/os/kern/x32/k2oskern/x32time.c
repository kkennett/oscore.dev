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

#include "x32kern.h"

static UINT32  sgPmTimerPort = 0;
static BOOL    sgPmTimerPort32 = FALSE;

static UINT64 volatile sgTickCounter = 0;
static UINT32 volatile sgTicksLeft;

static UINT32 sgHPET = 0;
static UINT64 sgHPET_Period = 0;
static UINT64 sgHPET_Scaled_Period = 0;

UINT32  gX32Kern_BusClockRate = 0;

typedef UINT32 (*pfTimerRead)(void);

static pfTimerRead  sgfTimerRead = NULL;
static UINT64       sgTimerDiv = 0;

static UINT32 sReadHPET(void)
{
    return MMREG_READ32(sgHPET, HPET_OFFSET_COUNT);
}

static UINT32 sReadPmTimer(void)
{
    UINT32 v;
    v = X32_IoRead32(sgPmTimerPort);
    if (!sgPmTimerPort32)
        v |= 0xFF000000;
    return v;
}

void
X32Kern_InitStall(
    void
)
{
    UINT32 reg;
    UINT32 v;
    UINT32 ix;
    UINT32 avg[4];

    //
    // use HPET if it is availble, otherwise the ACPI timer
    //
    if (gpX32Kern_HPET != 0)
    {
        v = ((UINT32)gpX32Kern_HPET->Address.Address) & K2_VA32_MEMPAGE_OFFSET_MASK;
        sgHPET = K2OSKERN_X32_HPET_KVA | v;

        //
        // get # of timers
        //
        reg = MMREG_READ32(sgHPET, HPET_OFFSET_GCID);
        v = (reg & HPET_GCID_LO_NUM_TIM_CAP) >> HPET_GCID_LO_NUM_TIM_CAP_SHL;

        //
        // store period of tick in femptoseconds
        //
        sgHPET_Period = MMREG_READ32(sgHPET, HPET_OFFSET_GCID_PERIOD32);
        sgHPET_Scaled_Period = (sgHPET_Period * 1024 * 1024 * 1024) / 1000000000;

        //
        // disable interupts from all HPET timers
        //
        ix = HPET_OFFSET_TIMER0_CONFIG;
        do {
            reg = MMREG_READ32(sgHPET, ix + HPET_TIMER_OFFSET_CONFIG);
            reg &= ~HPET_TIMER_CONFIG_LO_INT_ENB;
            reg |= HPET_TIMER_CONFIG_LO_32MODE;
            MMREG_WRITE32(sgHPET, ix + HPET_TIMER_OFFSET_CONFIG, reg);

            ix += HPET_TIMER_SPACE;
        } while (--v);

        //
        // enable the main counter in case it is not already ticking
        //
        reg = MMREG_READ32(sgHPET, HPET_OFFSET_CONFIG);
        reg |= HPET_CONFIG_ENABLE_CNF;
        MMREG_WRITE32(sgHPET, HPET_OFFSET_CONFIG, reg);

        sgfTimerRead = sReadHPET;
        sgTimerDiv = sgHPET_Scaled_Period;
    }
    else 
    {
        K2_ASSERT(0 != gpX32Kern_FADT->PmTmrLen);
        if (gpX32Kern_FADT->XPmTmrBlk.Address != 0)
        {
            K2_ASSERT(gpX32Kern_FADT->XPmTmrBlk.AddressSpaceId == ACPI_ASID_SYSTEM_IO);
            K2_ASSERT(gpX32Kern_FADT->XPmTmrBlk.AccessSize == ACPI_ASIZE_DWORD);
            K2_ASSERT(gpX32Kern_FADT->XPmTmrBlk.RegisterBitWidth == 32);
            K2_ASSERT(gpX32Kern_FADT->XPmTmrBlk.RegisterBitOffset == 0);
            sgPmTimerPort = (UINT32)gpX32Kern_FADT->XPmTmrBlk.Address;
        }
        else
        {
            sgPmTimerPort = gpX32Kern_FADT->PmTmrBlk;
        }

        if ((gpX32Kern_FADT->Flags >> 8) & 1)
            sgPmTimerPort32 = TRUE;

        sgfTimerRead = sReadPmTimer;
        sgTimerDiv = PMTMR_FEMPTO_1G_SCALED;
    }

    //
    // now get bus clock frequency
    //

    // set timer divisor to 4 to get bus clock rate (FSB rate is gBusClockRate * 4)
    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_DIV);
    reg &= ~X32_LOCAPIC_TIMER_DIV_MASK;
    reg |= X32_LOCAPIC_TIMER_DIV_4;
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_DIV, reg);

    // start the timer
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_INIT, 0xFFFFFFFF);

    // timer counts DOWN.  use 1/10 second timings, averaging 4 of them
    for (ix = 0; ix < 4; ix++)
    {
        reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_CURR);
        K2OSKERN_MicroStall(100000);
        v = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_CURR);
        avg[ix] = reg - v;
    }

    // stop the timer
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_INIT, 0);

    // average the timings
    for (ix = 1; ix < 4; ix++)
    {
        avg[0] += avg[ix];
    }
    avg[0] >>= 2;
    gX32Kern_BusClockRate = (((avg[0] * 10) + 500000) / 1000000) * 1000000;
}

void
K2_CALLCONV_REGS
K2OSKERN_MicroStall(
    UINT32      aMicroseconds
)
{
    UINT32 pauseTicks;
    UINT32 v;
    UINT32 last;
    UINT64 fempto;

    while (aMicroseconds > 1000000)
    {
        K2OSKERN_MicroStall(1000000);
        aMicroseconds -= 1000000;
    }

    if (aMicroseconds == 0)
        return;

    fempto = ((UINT64)aMicroseconds) << 30;

    last = sgfTimerRead();

    pauseTicks = (UINT32)(fempto / sgTimerDiv);
    do {
        v = sgfTimerRead();
        last = v - last;
        if (pauseTicks <= last)
            break;
        pauseTicks -= last;
        last = v;
    } while (1);
}

UINT64
K2_CALLCONV_REGS
K2OS_GetAbsTimeMs(
    void
)
{
    return sgTickCounter;
}

void
KernArch_ArmSchedTimer(
    UINT32 aMsFromNow
)
{
    BOOL disp;

    //
    // interrupts should be on
    //
    disp = K2OSKERN_SetIntr(FALSE);
    
    K2ATOMIC_Exchange(&sgTicksLeft, aMsFromNow);
    
    K2OSKERN_SetIntr(disp);
}

BOOL 
X32Kern_IntrTimerTick(
    void
)
{
    UINT32 v;
    BOOL   result;

    //
    // interrupts are off
    //
    sgTickCounter++;

    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_EOI, X32KERN_INTR_LVT_TIMER);

    //
    // if scheduling timer expires, return TRUE and will enter scheduler, else return FALSE
    //
    result = FALSE;

    do {
        v = sgTicksLeft;
        K2_CpuReadBarrier();

        if (v == 0)
            break;

        if (v == K2ATOMIC_CompareExchange(&sgTicksLeft, v - 1, v))
        {
            //
            // if we just decremented the last ms then return TRUE
            //
            if (v == 1)
                result = TRUE;
            break;
        }

    } while (1);

    return result;
}

void
X32Kern_StartTime(
    void
)
{
    UINT32 reg;

    //
    // called on core 0 right before core enters monitor for the first time 
    //
    sgTickCounter = 0;

    // make sure the timer is stopped
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_INIT, 0);

    // set timer mode to masked and periodic
    reg = MMREG_READ32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_TIMER);
    reg &= ~X32_LOCAPIC_LVT_TIMER_MODE_MASK;
    reg |= X32_LOCAPIC_LVT_TIMER_PERIODIC;
    reg |= X32_LOCAPIC_LVT_MASK;
    reg &= ~X32_LOCAPIC_LVT_STATUS;
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_TIMER, reg);

    // interrupt once per millisecond
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_TIMER_INIT, gX32Kern_BusClockRate / 1000);

    // unmask interrupt
    reg &= ~(X32_LOCAPIC_LVT_MASK | X32_LOCAPIC_LVT_STATUS);
    MMREG_WRITE32(K2OSKERN_X32_LOCAPIC_KVA, X32_LOCAPIC_OFFSET_LVT_TIMER, reg);
}

