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

UINT32 K2_CALLCONV_REGS MsgThread(void *apParam)
{
    K2OSKERN_Debug("In MsgThread\n");

    K2OS_ThreadSleep(500);

    while (1)
    {
        K2OS_ThreadSleep(1000);
        K2OSKERN_Debug("Msg Tick %d\n", (UINT32)K2OS_SysUpTimeMs());
    }

    return (UINT32)0xAABBCCDD;
}

//static
void
sCreateMsgThread(
    void
)
{
    K2OS_TOKEN          tokThread;
    K2OS_THREADCREATE   cret;

    K2MEM_Zero(&cret, sizeof(cret));
    cret.mStructBytes = sizeof(cret);
    cret.mEntrypoint = MsgThread;

    tokThread = K2OS_ThreadCreate(&cret);
    if (NULL == tokThread)
    {
        K2OSKERN_Debug("ThreadCreate for MsgThread failed\n");
    }
    else
    {
        K2OSKERN_Debug("tokThread = %08X\n", tokThread);
    }
}
 
void
K2OSEXEC_Run(
    void
)
{
    sCreateMsgThread();

#if 1
    K2OSKERN_Debug("Sleep Hang ints on\n");
    while (1)
    {
        K2OS_ThreadSleep(1000);
        K2OSKERN_Debug("Main Tick %d\n", (UINT32)K2OS_SysUpTimeMs());
    }
#else
    UINT64          last, newTick;
    K2OSKERN_Debug("Stall Hang ints on\n");
    last = K2OS_SysUpTimeMs();
    while (1)
    {
        do {
            newTick = K2OS_SysUpTimeMs();
        } while (newTick - last < 1000);
        last = newTick;
        K2OSKERN_Debug("Tick %d\n", (UINT32)(newTick & 0xFFFFFFFF));
    }
#endif

}

#if 0

8086 "Intel"
27B9 "82801GBM (ICH7-M) LPC Interface Bridge"
2415 "82801AA AC'97 Audio Controller"
7113 "82371AB/EB/MB PIIX4 ACPI"
7111 "82371AB/EB/MB PIIX4 IDE"
265C "82801FB/FBM/FR/FW/FRW (ICH6 Family) USB2 EHCI Controller"

80EE    "VirtualBox"
BEEF "VirtualBox Graphics Adapter"
CAFE "VirtualBox Guest Service"

1022     "AMD"
2000 "79c970 [PCnet32 LANCE]"

106B "Apple"
003F "KeyLargo / Intrepid USB"

#endif
