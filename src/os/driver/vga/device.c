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

#include "vga.h"

K2STAT VGA_LockedActivateInstance(VGADISP * apDisp, UINT32 aDevInstanceId)
{
    UINT32      ix;
    UINT32      barsDone;
    UINT32      barCount;
    UINT32 *    pBars;

    K2OSKERN_Debug("Activate VGA device instance %d\n", aDevInstanceId);

    apDisp->mpPciDevInfo = K2OSEXEC_DevPci_GetInfo(aDevInstanceId);
    if (apDisp->mpPciDevInfo == NULL)
        return K2STAT_ERROR_NOT_FOUND;

    apDisp->mDevInstanceId = aDevInstanceId;
    apDisp->mActive = TRUE;

    K2OSKERN_Debug(
        "    VGA %04X/%04X\n",
        apDisp->mpPciDevInfo->Cfg.AsType0.mVendorId,
        apDisp->mpPciDevInfo->Cfg.AsType0.mDeviceId
    );

    barCount = apDisp->mpPciDevInfo->mBarsFoundCount;
    K2OSKERN_Debug(
        "    %d BARS\n",
        barCount
    );

    if (0 != barCount)
    {
        barsDone = 0;
        pBars = &apDisp->mpPciDevInfo->Cfg.AsType0.mBar0;
        for (ix = 0; ix < 6; ix++)
        {
            if (pBars[ix] != 0)
            {
                K2OSKERN_Debug("[%d] %08X for %08X\n", ix, pBars[ix], apDisp->mpPciDevInfo->mBarSize[ix]);
                if (++barsDone == barCount)
                    break;
            }
        }
    }

    return K2STAT_NO_ERROR;
}

K2STAT VGA_LockedDeactivateInstance(VGADISP * apDisp, UINT32 aDevInstanceId)
{
    K2_ASSERT(aDevInstanceId == apDisp->mDevInstanceId);
    K2_ASSERT(TRUE == apDisp->mActive);

    K2OSKERN_Debug("Deactivate VGA device instance %d\n", apDisp->mDevInstanceId);

    apDisp->mpPciDevInfo = NULL;
    apDisp->mActive = FALSE;
    apDisp->mDevInstanceId = (UINT32)-1;

    return K2STAT_NO_ERROR;
}
