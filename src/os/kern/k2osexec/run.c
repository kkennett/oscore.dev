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

ACPI_STATUS DeviceWalkCallback(
    ACPI_HANDLE Object,
    UINT32      NestingLevel,
    void *      Context,
    void **     ReturnValue)
{
    ACPI_BUFFER bufDesc;
    char        charBuf[8];
    ACPI_STATUS acpiStatus;
    ACPI_DEVICE_INFO *pDevInfo;
    UINT32 ix;
    char *  pHID;

    bufDesc.Length = 8;
    bufDesc.Pointer = charBuf;

    charBuf[0] = 0;
    acpiStatus = AcpiGetName(Object, ACPI_SINGLE_NAME, &bufDesc);
    if (!ACPI_FAILURE(acpiStatus))
    {
        charBuf[4] = 0;
        acpiStatus = AcpiGetObjectInfo(Object, &pDevInfo);
        if (!ACPI_FAILURE(acpiStatus))
        {
            if (pDevInfo->Type == ACPI_TYPE_DEVICE)
            {
                for (ix = 0; ix < NestingLevel; ix++)
                    K2OSKERN_Debug(" ");
                pHID = pDevInfo->HardwareId.String;
                if (pHID != NULL)
                    K2OSKERN_Debug("%s: _HID(%s)\n", charBuf, pHID);
                else
                    K2OSKERN_Debug("%s\n", charBuf);
            }
            else
            {
                K2OSKERN_Debug("%s: Not DEVICE\n", charBuf);
            }
        }
        else
        {
            K2OSKERN_Debug("***%3d %s\n", NestingLevel, charBuf);
        }
    }

    return AE_OK;
}

void
K2OSEXEC_Run(
    void
)
{
    void *pWalkRet;

    pWalkRet = NULL;
    AcpiGetDevices(
        NULL,
        DeviceWalkCallback,
        NULL,
        &pWalkRet
    );

#if 1
    UINT64          last, newTick;
    K2OSKERN_Debug("Hang ints on\n");
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
