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

#include "kern.h"

K2OS_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_AlarmCreate(K2OS_TOKEN aNameToken, UINT32 aIntervalMs, BOOL aPeriodic)
{
    K2STAT                  stat;
    K2OSKERN_OBJ_NAME *     pNameObj;
    K2OSKERN_OBJ_ALARM *    pAlarmObj;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    K2OS_TOKEN              tokAlarm;

    tokAlarm = NULL;

    if ((aIntervalMs == 0) ||
        (aIntervalMs == K2OS_TIMEOUT_INFINITE))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return NULL;
    }

    pNameObj = NULL;
    if (aNameToken != NULL)
    {
        stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aNameToken, (K2OSKERN_OBJ_HEADER **)&pNameObj);
        if (!K2STAT_IS_ERROR(stat))
        {
            if (pNameObj->Hdr.mObjType != K2OS_Obj_Name)
            {
                K2OSKERN_ReleaseObject(&pNameObj->Hdr);
                stat = K2STAT_ERROR_BAD_TOKEN;
            }
        }
        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_ThreadSetStatus(stat);
            return FALSE;
        }
    }

    do {
        pAlarmObj = (K2OSKERN_OBJ_ALARM *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_ALARM));
        if (pAlarmObj == NULL)
        {
            stat = K2OS_ThreadGetStatus();
            break;
        }

        stat = KernAlarm_Create(pAlarmObj, pNameObj, aIntervalMs, aPeriodic);
        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_HeapFree(pAlarmObj);
            break;
        }

        pObjHdr = &pAlarmObj->Hdr;
        stat = K2OSKERN_CreateTokenNoAddRef(1, &pObjHdr, &tokAlarm);
        if (K2STAT_IS_ERROR(stat))
        {
            K2OSKERN_ReleaseObject(&pAlarmObj->Hdr);
        }

    } while (0);

    if (aNameToken != NULL)
    {
        K2_ASSERT(pNameObj != NULL);
        K2OSKERN_ReleaseObject(&pNameObj->Hdr);
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return NULL;
    }

    return tokAlarm;
}

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_AlarmAcquireByName(K2OS_TOKEN aNameToken)
{
    return K2OSKERN_CreateTokenFromAddRefOfNamedObject(aNameToken, K2OS_Obj_Alarm);
}
