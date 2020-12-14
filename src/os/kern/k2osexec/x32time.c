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

#include "ik2osexec.h"

UINT32 gTime_BusClockRate;

void
Time_Start(
    K2OSEXEC_INIT_INFO * apInitInfo
)
{
    UINT32 v;
    UINT32 reg;
    UINT32 ix;
    UINT32 avg[4];

    K2MEM_Zero(&apInitInfo->SysTickDevIrqConfig, sizeof(K2OSKERN_IRQ_CONFIG));

    if (gpMADT != NULL)
    {
        // set timer divisor to 4 to get bus clock rate (FSB rate is gTime_BusClockRate * 4)
        reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TIMER_DIV);
        reg &= ~X32_LOCAPIC_TIMER_DIV_MASK;
        reg |= X32_LOCAPIC_TIMER_DIV_4;
        MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TIMER_DIV, reg);

        // start the timer
        MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TIMER_INIT, 0xFFFFFFFF);

        // timer counts DOWN.  use 1/10 second timings, averaging 4 of them
        for (ix = 0; ix < 4; ix++)
        {
            reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TIMER_CURR);
            K2OSKERN_MicroStall(100000);
            v = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TIMER_CURR);
            avg[ix] = reg - v;
        }

        // stop the timer
        MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TIMER_INIT, 0);

        // average the timings
        for (ix = 1; ix < 4; ix++)
        {
            avg[0] += avg[ix];
        }
        avg[0] >>= 2;
        gTime_BusClockRate = (((avg[0] * 10) + 500000) / 1000000) * 1000000;

        // make sure the timer is stopped
        MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TIMER_INIT, 0);

        // set timer mode to masked and periodic
        reg = MMREG_READ32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_TIMER);
        reg &= ~X32_LOCAPIC_LVT_TIMER_MODE_MASK;
        reg |= X32_LOCAPIC_LVT_TIMER_PERIODIC;
        reg |= X32_LOCAPIC_LVT_MASK;
        reg &= ~X32_LOCAPIC_LVT_STATUS;
        MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_LVT_TIMER, reg);

        // interrupt once per millisecond
        MMREG_WRITE32(K2OS_KVA_X32_LOCAPIC, X32_LOCAPIC_OFFSET_TIMER_INIT, gTime_BusClockRate / 1000);

        apInitInfo->SysTickDevIrqConfig.mSourceIrq = X32_DEVIRQ_LVT_TIMER;
    }
    else
    {
        gTime_BusClockRate = 1000000;
        X32PIT_InitTo1Khz();
        // leave mSourceIrq as zero, which is the IRQ of the PIT
    }
}
