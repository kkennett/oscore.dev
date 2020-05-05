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

#define HAL_SERVICE_CONTEXT     ((void *)0xDEADF00D)
#define FSPROV_IFACE_CONTEXT    ((void *)0xFEEDF00D)

static K2OS_FSPROVINFO const sgInfo =
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
    if (aOutBufBytes < sizeof(sgInfo))
        return K2STAT_ERROR_OUTBUF_TOO_SMALL;

    K2MEM_Copy(apOutBuf, &sgInfo, sizeof(sgInfo));
    *apRetActualOut = sizeof(sgInfo);

    return K2STAT_NO_ERROR;
}

K2OS_TOKEN  tokMailbox;
K2OS_TOKEN  tokService;
UINT32      serviceId;
K2OS_TOKEN  tokPublish;
UINT32      interfaceId;

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

    K2OS_TokenDestroy(tokMailbox);

    tokPublish = K2OSKERN_ServicePublish(
        tokService,
        &gK2OSEXEC_FsProvInterfaceGuid,
        FSPROV_IFACE_CONTEXT,
        &interfaceId);
    if (NULL == tokPublish)
    {
        K2OSKERN_Panic("HAL failed to publish filesystem interface\n");
    }

    do {
        waitResult = K2OS_ThreadWaitOne(tokMailbox, K2OS_TIMEOUT_INFINITE);
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

