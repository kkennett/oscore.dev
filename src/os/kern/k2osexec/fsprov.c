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

// {0440C404-CAD6-4333-A23D-24FDBF335DA8}
K2_GUID128 const    gK2OSEXEC_FsProvInterfaceGuid       = K2OS_INTERFACE_ID_FSPROV;
char const *        gpK2OSEXEC_FsProvInterfaceGuidStr   = "{0440C404-CAD6-4333-A23D-24FDBF335DA8}";

// {71E000D8-2A94-476A-B6C7-A8575C6618CE}
K2_GUID128 const    gK2OSEXEC_FileSysInterfaceGuid      = K2OS_INTERFACE_ID_FILESYS;
char const *        gpK2OSEXEC_FileSysInterfaceGuidStr  = "{71E000D8-2A94-476A-B6C7-A8575C6618CE}";

K2OS_TOKEN gFsProv_TokNotify = NULL;

void FsProv_OnNotify(void)
{
    BOOL                isArrival;
    K2_GUID128          interfaceId;
    void *              pContext;
    K2OSKERN_SVC_IFINST ifInst;
    K2STAT              stat;
    K2OS_FSPROVINFO     provInfo;

    do {
        if (!K2OSKERN_NotifyRead(gFsProv_TokNotify,
            &isArrival,
            &interfaceId,
            &pContext,
            &ifInst))
            break;

        K2_ASSERT(0 == K2MEM_Compare(&interfaceId, &gK2OSEXEC_FsProvInterfaceGuid, sizeof(K2_GUID128)));

        if (isArrival)
        {
            //
            // file system provider interface was published
            //
            K2OSKERN_Debug("ARRIVE - FILESYS PROVIDER: %d / %d\n",
                ifInst.mServiceInstanceId,
                ifInst.mInterfaceInstanceId);

            stat = K2OSKERN_ServiceCall(
                ifInst.mInterfaceInstanceId,
                FSPROV_CALL_OPCODE_GET_INFO,
                NULL, 0,
                &provInfo, sizeof(provInfo),
                NULL);
            if (K2STAT_IS_ERROR(stat))
            {
                K2OSKERN_Debug("Provider did not return info, error %08X\n", stat);
            }
            else
            {
                K2OSKERN_Debug("FsProvider \"%s\" arrived\n", provInfo.ProvName);
            }
        }
        else
        {
            //
            // file system provider interface left
            //
            K2OSKERN_Debug("DEPART - FILESYS PROVIDER: %d / %d\n",
                ifInst.mServiceInstanceId,
                ifInst.mInterfaceInstanceId);
        }
    } while (1);
}

static
K2STAT
sResolveDlxSpec(
    K2OS_PATH_TOKEN aTokPath,
    char const *    apRelSpec,
    char **         appRetFullSpec
)
{
    // resolve spec to a full name, allocating the spec buffer from the heap
    K2OSKERN_Debug("Exec:ResolveDlxSpec(%08X, %s)\n", aTokPath, apRelSpec);
    return K2STAT_ERROR_NOT_IMPL;
}

void FsProv_Init(K2OSEXEC_INIT_INFO * apInitInfo)
{
    K2OS_TOKEN tokSubscrip;

    gFsProv_TokNotify = K2OSKERN_NotifyCreate();
    K2_ASSERT(gFsProv_TokNotify != NULL);

    tokSubscrip = K2OSKERN_NotifySubscribe(gFsProv_TokNotify, &gK2OSEXEC_FsProvInterfaceGuid, NULL);
    K2_ASSERT(tokSubscrip != NULL);

    apInitInfo->ResolveDlxSpec = sResolveDlxSpec;
}
