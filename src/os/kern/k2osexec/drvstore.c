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

// {612A5D97-C492-44B3-94A1-ACE475EB930E}

K2_GUID128 const gK2OSEXEC_DriverStoreInterfaceGuid = K2OS_INTERFACE_ID_DRIVERSTORE;
char const *     gpK2OSEXEC_DriverStoreInterfaceGuidStr = "{612A5D97-C492-44B3-94A1-ACE475EB930E}";

K2OS_TOKEN gDrvStore_TokNotify = NULL;

static SERWORK_ITEM_HDR sgWork;

void sDoWork(SERWORK_ITEM_HDR *apItem)
{
    int a, b;
    K2_ASSERT(apItem == &sgWork);
    a = 1;
    b = 0;
    K2OSKERN_Debug("Do work\n");
    a = a / b;
}

void DrvStore_OnNotify(void)
{
    BOOL                    isArrival;
    K2_GUID128              interfaceId;
    void *                  pContext;
    K2OSKERN_SVC_IFINST     ifInst;
    K2STAT                  stat;
    K2OSKERN_DRVSTORE_INFO  storeInfo;

    do {
        if (!K2OSKERN_NotifyRead(gDrvStore_TokNotify,
            &isArrival,
            &interfaceId,
            &pContext,
            &ifInst))
            break;

        K2_ASSERT(0 == K2MEM_Compare(&interfaceId, &gK2OSEXEC_DriverStoreInterfaceGuid, sizeof(K2_GUID128)));

        if (isArrival)
        {
            //
            // file system provider interface was published
            //
            K2OSKERN_Debug("ARRIVE - DRIVER STORE: %d / %d\n",
                ifInst.mServiceInstanceId,
                ifInst.mInterfaceInstanceId);

            stat = K2OSKERN_ServiceCall(
                ifInst.mInterfaceInstanceId,
                DRVSTORE_CALL_OPCODE_GET_INFO,
                NULL, 0,
                &storeInfo, sizeof(storeInfo),
                NULL);
            if (K2STAT_IS_ERROR(stat))
            {
                K2OSKERN_Debug("Driver Store did not return info, error %08X\n", stat);
            }
            else
            {
                K2OSKERN_Debug("Driver Store \"%s\" arrived\n", storeInfo.StoreName);

                Run_AddSerializedWork(&sgWork, sDoWork);




            }
        }
        else
        {
            //
            // file system provider interface left
            //
            K2OSKERN_Debug("DEPART - DRIVER STORE: %d / %d\n",
                ifInst.mServiceInstanceId,
                ifInst.mInterfaceInstanceId);
        }
    } while (1);
}

void DrvStore_Init(void)
{
    K2OS_TOKEN tokSubscrip;

    gDrvStore_TokNotify = K2OSKERN_NotifyCreate();
    K2_ASSERT(gDrvStore_TokNotify != NULL);

    tokSubscrip = K2OSKERN_NotifySubscribe(gDrvStore_TokNotify, &gK2OSEXEC_DriverStoreInterfaceGuid, NULL);
    K2_ASSERT(tokSubscrip != NULL);
}