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

#include <k2oshal.h>
#include "..\udooquad.h"

static UINT32 sgDebugPageAddr = 0;

void
K2_CALLCONV_REGS
K2OSHAL_DebugOut(
    UINT8 aByte
)
{
    if (sgDebugPageAddr == 0)
        return;
    if (aByte == '\n')
    {
        while ((K2MMIO_Read32(sgDebugPageAddr + IMX6_UART_OFFSET_UTS) & IMX6_UART_UTS_TXEMPTY) == 0);
        K2MMIO_Write32(sgDebugPageAddr + IMX6_UART_OFFSET_UTXD, '\r');
    }
    while ((K2MMIO_Read32(sgDebugPageAddr + IMX6_UART_OFFSET_UTS) & IMX6_UART_UTS_TXEMPTY) == 0);
    K2MMIO_Write32(sgDebugPageAddr + IMX6_UART_OFFSET_UTXD, aByte);
}

BOOL
K2_CALLCONV_REGS
K2OSHAL_DebugIn(
    UINT8 *apRetData
)
{
    if (sgDebugPageAddr == 0)
        return FALSE;
    if (0 == (K2MMIO_Read32(sgDebugPageAddr + IMX6_UART_OFFSET_USR2) & IMX6_UART_USR2_RDR))
        return FALSE;
    *apRetData = (UINT8)(K2MMIO_Read32(sgDebugPageAddr + IMX6_UART_OFFSET_URXD) & 0xFF);
    return TRUE;
}

K2STAT
K2_CALLCONV_REGS
dlx_entry(
    DLX *   apDlx,
    UINT32  aReason
)
{
    K2OSKERN_Esc(K2OSKERN_ESC_GET_DEBUGPAGE, (void *)&sgDebugPageAddr);
    K2OS_DebugPrint("k2oshal init\n");
    return 0;
}



