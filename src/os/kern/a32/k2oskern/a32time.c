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

#include "a32kern.h"

void 
A32Kern_InitStall(
    void
)
{
    UINT32 pfr1;

    if (gpA32Kern_MADT_GICD != NULL)
    {
        K2OSKERN_Debug("Multicore, Global timer should be at 0x%08X\n", A32KERN_MP_GLOBAL_TIMER_VIRT);





    }
    else
    {
        pfr1 = A32_ReadIDPFR1();
        if (!(pfr1 & A32_IDPFR1_TIMER_MASK))
        {
            K2OSKERN_Debug("IDPFR1 = 0x%08X\n", pfr1);
            K2OSKERN_Panic("No MPCore and Generic timer is not implemented\n");
        }

        //
        // Generic timer support not implemented yet
        //
        K2_ASSERT(0);
    }
}

void
K2_CALLCONV_REGS
K2OSKERN_MicroStall(
    UINT32      aMicroseconds
)
{

}

UINT64
K2_CALLCONV_REGS
K2OS_GetAbsTimeMs(
    void
)
{
    return 0;
}

void A32Kern_StartTime(void)
{
    //
    // called on core 0 right before core enters monitor for the first time 
    //


}