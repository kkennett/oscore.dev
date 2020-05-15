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

#define K2_STATIC static

// {612A5D97-C492-44B3-94A1-ACE475EB930E}

K2_GUID128 const gK2OSEXEC_DriverStoreInterfaceGuid = K2OS_INTERFACE_ID_DRIVERSTORE;
char const *     gpK2OSEXEC_DriverStoreInterfaceGuidStr = "{612A5D97-C492-44B3-94A1-ACE475EB930E}";

K2OS_TOKEN gDrvStore_TokNotify = NULL;

typedef struct _SERWORK_DRV_NOTIFY SERWORK_DRV_NOTIFY;
struct _SERWORK_DRV_NOTIFY
{
    SERWORK_ITEM_HDR    Hdr;
    BOOL                mIsArrival;
    void *              mpContext;
    K2OSKERN_SVC_IFINST IfInst;
};

typedef struct _DRVSTORE DRVSTORE;
struct _DRVSTORE
{
    K2OSKERN_DRVSTORE_INFO  Info;
    UINT32                  mInterfaceId;
    K2LIST_LINK             StoreListLink;
};

static K2LIST_ANCHOR sgStoreList;

K2_STATIC 
void sDrvStore_Arrive(UINT32 aStoreInterfaceId)
{
    DRVSTORE *  pNewStore;
    K2STAT      stat;

    pNewStore = (DRVSTORE *)K2OS_HeapAlloc(sizeof(DRVSTORE));
    if (pNewStore == NULL)
    {
        K2OSKERN_Debug("!!!Out of memory allocating driver store\n");
        return;
    }

    do {
        stat = K2OSKERN_ServiceCall(
            aStoreInterfaceId,
            DRVSTORE_CALL_OPCODE_GET_INFO,
            NULL, 0,
            &pNewStore->Info, sizeof(K2OSKERN_DRVSTORE_INFO),
            NULL);
        if (K2STAT_IS_ERROR(stat))
        {
            K2OSKERN_Debug("Driver Store did not return info after arrival, error %08X\n", stat);
            break;
        }
        K2OSKERN_Debug("Driver Store \"%s\" arrived\n", pNewStore->Info.StoreName);

        pNewStore->mInterfaceId = aStoreInterfaceId;
        K2LIST_AddAtTail(&sgStoreList, &pNewStore->StoreListLink);

    } while (0);

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_HeapFree(pNewStore);
        return;
    }
}

K2_STATIC
void sDrvStore_Depart(UINT32 aStoreInterfaceId)
{
    K2LIST_LINK *   pListLink;
    DRVSTORE *      pStore;

    pListLink = sgStoreList.mpHead;
    do {
        pStore = K2_GET_CONTAINER(DRVSTORE, pListLink, StoreListLink);
        if (pStore->mInterfaceId == aStoreInterfaceId)
            break;
    } while (pListLink != NULL);

    K2OSKERN_Debug("Driver Store \"%s\" departed\n", pStore->Info.StoreName);

    K2LIST_Remove(&sgStoreList, &pStore->StoreListLink);
    pStore->mInterfaceId = 0;
}

K2_STATIC
void sDrvStore_NotifyWork(SERWORK_ITEM_HDR *apItem)
{
    SERWORK_DRV_NOTIFY *    pNotify;

    //
    // this is running on the common serialized work thread
    //

    pNotify = (SERWORK_DRV_NOTIFY *)apItem;

    K2OSKERN_Debug("%s - DRIVER STORE: %d / %d\n",
        pNotify->mIsArrival ? "ARRIVE" : "DEPART",
        pNotify->IfInst.mServiceInstanceId,
        pNotify->IfInst.mInterfaceInstanceId);

    if (pNotify->mIsArrival)
    {
        sDrvStore_Arrive(pNotify->IfInst.mInterfaceInstanceId);
    }
    else
    {
        sDrvStore_Depart(pNotify->IfInst.mInterfaceInstanceId);
    }

    K2OS_HeapFree(pNotify);
}

void DrvStore_OnNotify(void)
{
    K2_GUID128              interfaceId;
    SERWORK_DRV_NOTIFY *    pNotify;

    //
    // this is running on the k2osexec service thread
    //

    do {
        pNotify = (SERWORK_DRV_NOTIFY *)K2OS_HeapAlloc(sizeof(SERWORK_DRV_NOTIFY));
        if (pNotify != NULL)
        {
            if (K2OSKERN_NotifyRead(gDrvStore_TokNotify,
                &pNotify->mIsArrival,
                &interfaceId,
                &pNotify->mpContext,
                &pNotify->IfInst))
            {
                K2_ASSERT(0 == K2MEM_Compare(&interfaceId, &gK2OSEXEC_DriverStoreInterfaceGuid, sizeof(K2_GUID128)));
                Run_AddSerializedWork(&pNotify->Hdr, sDrvStore_NotifyWork);
            }
        }
        else
        {
            K2OS_ThreadSleep(100);
        }
    } while (1);
}

void DrvStore_Init(void)
{
    K2OS_TOKEN tokSubscrip;

    K2LIST_Init(&sgStoreList);

    gDrvStore_TokNotify = K2OSKERN_NotifyCreate();
    K2_ASSERT(gDrvStore_TokNotify != NULL);

    tokSubscrip = K2OSKERN_NotifySubscribe(gDrvStore_TokNotify, &gK2OSEXEC_DriverStoreInterfaceGuid, NULL);
    K2_ASSERT(tokSubscrip != NULL);
}