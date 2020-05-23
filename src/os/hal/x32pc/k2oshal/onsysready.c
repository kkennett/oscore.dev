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

#include "..\x32pc.h"

#define K2_STATIC  static

#define HAL_SERVICE_CONTEXT     ((void *)0xDEADF00D)
#define FSPROV_IFACE_CONTEXT    ((void *)0xFEEDF00D)
#define DRVSTORE_IFACE_CONTEXT  ((void *)0xF00DDEAD)

static K2OS_FSPROVINFO const sgFsProvInfo =
{
    K2OS_FSPROV_ID_HAL,     // {138DE9B5-2549-4376-BDE7-2C3AF41B2180}
    "HAL BuiltIn",
    0x00010000
};

K2STAT 
FsProvServiceCall(
    UINT32          aCallCmd,
    void const *    apInBuf,
    UINT32          aInBufBytes,
    void *          apOutBuf,
    UINT32          aOutBufBytes,
    UINT32 *        apRetActualOut
)
{
    if (((aCallCmd & FSPROV_CALL_OPCODE_HIGH_MASK) != FSPROV_CALL_OPCODE_HIGH) ||
        (aCallCmd != FSPROV_CALL_OPCODE_GET_INFO))
        return K2STAT_ERROR_UNSUPPORTED;
    if (apInBuf != NULL)
        return K2STAT_ERROR_INBUF_NOT_NULL;
    if (apOutBuf == NULL)
        return K2STAT_ERROR_OUTBUF_NULL;
    if (aOutBufBytes < sizeof(sgFsProvInfo))
        return K2STAT_ERROR_OUTBUF_TOO_SMALL;

    K2MEM_Copy(apOutBuf, &sgFsProvInfo, sizeof(sgFsProvInfo));
    *apRetActualOut = sizeof(sgFsProvInfo);

    return K2STAT_NO_ERROR;
}

static K2OSEXEC_DRVSTORE_INFO const sgDrvStoreInfo =
{
    K2OSEXEC_DRVSTORE_ID_HAL,     // {AFA9A8C6-D494-4D7B-9423-169E416C2396}
    "HAL BuiltIn",
    0x00010000
};

static char const * const sgpRootHubId = "ACPI/HID/PNP0A03";

K2_STATIC
K2STAT 
sDrvStore_FindDriver(
    UINT32          aNumTypeIds,
    char const **   appTypeIds,
    UINT32 *        apRetSelect
)
{
    UINT32 ix;

    for (ix = 0; ix < aNumTypeIds; ix++)
    {
        K2OSKERN_Debug("  %s\n", appTypeIds[ix]);
        if (0 == K2ASC_CompIns(appTypeIds[ix], sgpRootHubId))
            break;
    }
    if (ix == aNumTypeIds)
    {
        return K2STAT_ERROR_NOT_FOUND;
    }

    //
    // request for PCI root bridge driver
    //

    *apRetSelect = ix;

    return K2STAT_NO_ERROR;
}

K2STAT sDrvStore_PrepareDriverInstance(char const *apTypeId, UINT32 *apRetStoreHandle)
{
    if (0 != K2ASC_CompIns(apTypeId, sgpRootHubId))
        return K2STAT_ERROR_NOT_FOUND;

    *apRetStoreHandle = 1;

    return K2STAT_NO_ERROR;
}

K2STAT sDrvStore_ActivateDriver(UINT32 aStoreHandle, UINT32 aDevInstanceId)
{
    if (aStoreHandle != 1)
        return K2STAT_ERROR_NOT_FOUND;

    return K2STAT_NO_ERROR;
}

K2STAT sDrvStore_PurgeDriverInstance(UINT32 aStoreHandle)
{
    if (aStoreHandle != 1)
        return K2STAT_ERROR_NOT_FOUND;

    return K2STAT_NO_ERROR;
}

static K2OSEXEC_DRVSTORE_DIRECT const sgDrvStoreDirect =
{
    0x00010000,
    sDrvStore_FindDriver,
    sDrvStore_PrepareDriverInstance,
    sDrvStore_ActivateDriver,
    sDrvStore_PurgeDriverInstance
};

K2STAT
DrvStoreServiceCall(
    UINT32          aCallCmd,
    void const *    apInBuf,
    UINT32          aInBufBytes,
    void *          apOutBuf,
    UINT32          aOutBufBytes,
    UINT32 *        apRetActualOut
)
{
    if ((aCallCmd & DRVSTORE_CALL_OPCODE_HIGH_MASK) != DRVSTORE_CALL_OPCODE_HIGH)
        return K2STAT_ERROR_UNSUPPORTED;

    if (aCallCmd == DRVSTORE_CALL_OPCODE_GET_INFO)
    {
        if (apInBuf != NULL)
            return K2STAT_ERROR_INBUF_NOT_NULL;
        if (apOutBuf == NULL)
            return K2STAT_ERROR_OUTBUF_NULL;
        if (aOutBufBytes < sizeof(sgDrvStoreInfo))
            return K2STAT_ERROR_OUTBUF_TOO_SMALL;
        K2MEM_Copy(apOutBuf, &sgDrvStoreInfo, sizeof(sgDrvStoreInfo));
        *apRetActualOut = sizeof(sgDrvStoreInfo);
    }
    else if (aCallCmd == DRVSTORE_CALL_OPCODE_GET_DIRECT)
    {
        if (apInBuf != NULL)
            return K2STAT_ERROR_INBUF_NOT_NULL;
        if (apOutBuf == NULL)
            return K2STAT_ERROR_OUTBUF_NULL;
        if (aOutBufBytes < sizeof(sgDrvStoreDirect))
            return K2STAT_ERROR_OUTBUF_TOO_SMALL;
        K2MEM_Copy(apOutBuf, &sgDrvStoreDirect, sizeof(sgDrvStoreDirect));
        *apRetActualOut = sizeof(sgDrvStoreDirect);
    }
    else
    {
        return K2STAT_ERROR_NOT_IMPL;
    }



    return K2STAT_NO_ERROR;
}

K2OS_TOKEN  tokMailbox;
K2OS_TOKEN  tokService;
UINT32      serviceId;
K2OS_TOKEN  tokPublishFsProv;
K2OS_TOKEN  tokPublishDrvStore;
UINT32      sgFsProvInterfaceId;
UINT32      sgDrvStoreInterfaceId;

UINT32
K2_CALLCONV_CALLERCLEANS
K2OSHAL_OnSystemReady(
    void
)
{
    K2OSKERN_SVC_MSGIO  msgIo;
    UINT32              waitResult;
    UINT32              requestId;
    BOOL                ok;
    UINT32              actualOut;

    tokMailbox = K2OS_MailboxCreate(NULL, FALSE);
    if (NULL == tokMailbox)
    {
        K2OSKERN_Panic("HAL failed mailbox create\n");
    }

    tokService = K2OSKERN_ServiceCreate(tokMailbox, HAL_SERVICE_CONTEXT, &serviceId);
    if (NULL == tokService)
    {
        K2OSKERN_Panic("HAL failed to create service\n");
    }

    tokPublishFsProv = K2OSKERN_ServicePublish(
        tokService,
        &gK2OSEXEC_FsProvInterfaceGuid,
        FSPROV_IFACE_CONTEXT,
        &sgFsProvInterfaceId);
    if (NULL == tokPublishFsProv)
    {
        K2OSKERN_Panic("HAL failed to publish filesystem interface\n");
    }

    tokPublishDrvStore = K2OSKERN_ServicePublish(
        tokService,
        &gK2OSEXEC_DriverStoreInterfaceGuid,
        DRVSTORE_IFACE_CONTEXT,
        &sgDrvStoreInterfaceId);
    if (NULL == tokPublishDrvStore)
    {
        K2OSKERN_Panic("HAL failed to publish driver store interface\n");
    }

    do {
        waitResult = K2OS_ThreadWait(1, &tokMailbox, FALSE, K2OS_TIMEOUT_INFINITE);
        K2_ASSERT(waitResult == K2OS_WAIT_SIGNALLED_0);
        requestId = 0;
        ok = K2OS_MailboxRecv(tokMailbox, (K2OS_MSGIO *)&msgIo, &requestId);
        K2_ASSERT(ok);
        if (requestId != 0)
        {
            if (msgIo.mSvcOpCode == SYSMSG_OPCODE_SVC_CALL)
            {
                actualOut = 0;
                if (msgIo.mInBufBytes == 0)
                    msgIo.mpInBuf = NULL;
                if (msgIo.mOutBufBytes == 0)
                    msgIo.mpOutBuf = NULL;
                if (msgIo.mpServiceContext == HAL_SERVICE_CONTEXT)
                {
                    if (msgIo.mpPublishContext == FSPROV_IFACE_CONTEXT)
                    {
                        ((K2OS_MSGIO *)&msgIo)->mStatus = FsProvServiceCall(
                            msgIo.mCallCmd,
                            msgIo.mpInBuf,
                            msgIo.mInBufBytes,
                            msgIo.mpOutBuf,
                            msgIo.mOutBufBytes,
                            &actualOut
                        );
                    }
                    else if (msgIo.mpPublishContext == DRVSTORE_IFACE_CONTEXT)
                    {
                        ((K2OS_MSGIO *)&msgIo)->mStatus = DrvStoreServiceCall(
                            msgIo.mCallCmd,
                            msgIo.mpInBuf,
                            msgIo.mInBufBytes,
                            msgIo.mpOutBuf,
                            msgIo.mOutBufBytes,
                            &actualOut
                        );
                    }
                    else
                    {
                        ((K2OS_MSGIO *)&msgIo)->mStatus = K2STAT_ERROR_NO_INTERFACE;
                    }
                }
                else
                {
                    ((K2OS_MSGIO *)&msgIo)->mStatus = K2STAT_ERROR_UNSUPPORTED;
                }
                K2_ASSERT(actualOut <= msgIo.mOutBufBytes);
                ((K2OS_MSGIO *)&msgIo)->mPayload[0] = actualOut;
            }
            else
            {
                K2OSKERN_Debug("***HAL service mailbox received bad msg\n");
                ((K2OS_MSGIO *)&msgIo)->mStatus = K2STAT_ERROR_NOT_IMPL;
                ((K2OS_MSGIO *)&msgIo)->mPayload[0] = 0;
            }
            ok = K2OS_MailboxRespond(tokMailbox, requestId, (K2OS_MSGIO *)&msgIo);
            K2_ASSERT(ok);
        }
    } while (1);

    return 0;
}

