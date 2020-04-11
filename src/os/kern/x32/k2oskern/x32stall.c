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

    //
    // use HPET if it is availble, otherwise the ACPI timer
    //
    if (gpX32Kern_HPET != 0)
    {
        v = ((UINT32)gpX32Kern_HPET->Address.Address) & K2_VA32_MEMPAGE_OFFSET_MASK;
        sgHPET = K2OS_KVA_X32_HPET | v;

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

