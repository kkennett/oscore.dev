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

#include "k2osexec.h"

void InitPart1(void)
{
    ACPI_STATUS acpiStatus;

    acpiStatus = AcpiInitializeSubsystem();
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiInitializeTables(NULL, 16, FALSE);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));
}

void InitPart2(void)
{
    ACPI_STATUS acpiStatus;

    acpiStatus = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiLoadTables();
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));

    acpiStatus = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    K2_ASSERT(!ACPI_FAILURE(acpiStatus));
}

void
K2OSEXEC_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
)
{
    InitPart1();
    InstallHandlers1();
    InitPart2();
    InstallHandlers2();
    SetupPciConfig();

    //
    // find the system tick interrupt source and set it up
    //


    K2_ASSERT(0);
}


#if 0

void
X32Kern_StartTime(
    void
)
{
    UINT32 reg;

    if (gpX32Kern_MADT != NULL)
    {
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
    else
    {
        K2OSKERN_Debug("No MADT - must use PIT for timer!\n");
        X32Kern_PITInit();
        X32Kern_UnmaskDevIntr(0);
    }
}

#endif

#if 0

//
// now get bus clock frequency
//
UINT32  gX32Kern_BusClockRate = 0;
if (gpX32Kern_MADT != NULL)
{
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
else
gX32Kern_BusClockRate = 1000000;


#endif