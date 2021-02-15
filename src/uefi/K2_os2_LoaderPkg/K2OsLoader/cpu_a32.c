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
#include "K2OsLoader.h"
#include <Pi/PiBootMode.h>
#include <Pi/PiHob.h>
#include <Library/HobLib.h>
#include <Guid/ArmMpCoreInfo.h>

EFI_STATUS Loader_FillCpuInfo(void)
{
    EFI_HOB_GUID_TYPE * pGuidHob;
    UINTN               coreCount;
    ARM_CORE_INFO *     pInfo;
    UINTN               ix;

    //
    // fill gData.LoadInfo.CpuInfo for gData.LoadInfo.mCpuCount
    //

    //
    // find the arm multicore info table in the hob list
    //
    pGuidHob = (EFI_HOB_GUID_TYPE *)GetFirstGuidHob(&gArmMpCoreInfoGuid);
    if (pGuidHob == NULL)
    {
        K2Printf(L"Arm MpCore Info Hob not found. Default to 1 cpu.\n");
        gData.LoadInfo.mCpuCoreCount = 1;
        return EFI_SUCCESS;
    }

    gData.LoadInfo.mCpuCoreCount = 0;

    coreCount = (pGuidHob->Header.HobLength - sizeof(EFI_HOB_GUID_TYPE)) / sizeof(ARM_CORE_INFO);

    pInfo = (ARM_CORE_INFO *)(((UINT8 *)pGuidHob) + sizeof(EFI_HOB_GUID_TYPE));

    for (ix = 0;ix < coreCount;ix++)
    {
        gData.LoadInfo.CpuInfo[ix].mCpuId =
            ((pInfo[ix].ClusterId & 0xFFFF) << 16) |
            (pInfo[ix].CoreId & 0xFFFF);

        gData.LoadInfo.CpuInfo[ix].mAddrSet = (UINT32)pInfo[ix].MailboxSetAddress;
        gData.LoadInfo.CpuInfo[ix].mAddrGet = (UINT32)pInfo[ix].MailboxGetAddress;
        gData.LoadInfo.CpuInfo[ix].mAddrClear = (UINT32)pInfo[ix].MailboxClearAddress;
        gData.LoadInfo.CpuInfo[ix].mClearValue = (UINT32)pInfo[ix].MailboxClearValue;

        gData.LoadInfo.mCpuCoreCount++;
    }

    return EFI_SUCCESS;
}
