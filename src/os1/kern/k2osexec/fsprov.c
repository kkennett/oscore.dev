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
sOpenDlx(
    K2OSKERN_OBJ_HEADER *   apPathObj,
    char const *            apRelSpec,
    K2OS_TOKEN *            apRetTokDlxFile,
    UINT32 *                apRetTotalSectors
)
{
    FSPROV_OPAQUE       opaque;
    K2STAT              stat;
    FSPROV_OBJ_FILE *   pFileObj;
    char const *        pEnd;
    UINT32              specLen;
    char *              pTempName;

    if ((apRelSpec == NULL) || (*apRelSpec == 0))
        return K2STAT_ERROR_BAD_ARGUMENT;

    if (apPathObj != NULL)
        return K2STAT_ERROR_NOT_IMPL;

    //
    // if apRelSpec does not end in an extension we add .dlx
    //
    pEnd = apRelSpec;
    while (*pEnd)
        pEnd++;
    specLen = (UINT32)(pEnd - apRelSpec);
    do {
        pEnd--;
        if ((*pEnd == '.') || (*pEnd == '/') || (*pEnd == '\\'))
            break;
    } while (pEnd != apRelSpec);
    if (*pEnd != '.')
    {
        //
        // no extension found
        //
        pTempName = K2OS_HeapAlloc(((specLen + 4) + 4) & ~3);
        if (pTempName == NULL)
            return K2STAT_ERROR_OUT_OF_MEMORY;
        K2ASC_Copy(pTempName, apRelSpec);
        K2ASC_Copy(pTempName + specLen, ".dlx");
    }
    else
        pTempName = NULL;

    stat = gFsProv_Builtin_Direct.Open((pTempName != NULL) ? pTempName : apRelSpec, &opaque, apRetTotalSectors);

    if (pTempName != NULL)
        K2OS_HeapFree(pTempName);

    if (K2STAT_IS_ERROR(stat))
        return stat;

    pFileObj = (FSPROV_OBJ_FILE *)K2OS_HeapAlloc(sizeof(FSPROV_OBJ_FILE));
    if (pFileObj == NULL)
    {
        gFsProv_Builtin_Direct.Close(opaque);
        return K2STAT_ERROR_OUT_OF_MEMORY;
    }

    K2MEM_Zero(pFileObj, sizeof(FSPROV_OBJ_FILE));
    pFileObj->Hdr.mObjType = K2OS_Obj_File;
    pFileObj->Hdr.mRefCount = 1;
    pFileObj->Hdr.Dispose = Builtin_Dispose;
    K2LIST_Init(&pFileObj->Hdr.WaitEntryPrioList);

    pFileObj->mpProvDirect = &gFsProv_Builtin_Direct;
    pFileObj->mOpaque = opaque;

    stat = K2OSKERN_AddObject(&pFileObj->Hdr);
    if (K2STAT_IS_ERROR(stat))
    {
        gFsProv_Builtin_Direct.Close(opaque);
        K2OS_HeapFree(pFileObj);
        return stat;
    }

    stat = K2OSKERN_CreateTokenNoAddRef(1, (K2OSKERN_OBJ_HEADER **)&pFileObj, apRetTokDlxFile);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OSKERN_ReleaseObject(&pFileObj->Hdr);
        *apRetTokDlxFile = NULL;
        *apRetTotalSectors = 0;
    }

    return stat;
}

static
K2STAT
sReadDlx(
    K2OS_TOKEN  aTokDlxFile,
    void *      apBuffer,
    UINT32      aStartSector,
    UINT32      aSectorCount
)
{
    K2STAT              stat;
    K2STAT              stat2;
    FSPROV_OBJ_FILE *   pFileObj;

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokDlxFile, (K2OSKERN_OBJ_HEADER **)&pFileObj);
    if (K2STAT_IS_ERROR(stat))
        return stat;

    if (pFileObj->Hdr.mObjType != K2OS_Obj_File)
    {
        stat = K2STAT_ERROR_BAD_ARGUMENT;
    }
    else
    {
        stat = pFileObj->mpProvDirect->Read(pFileObj->mOpaque, apBuffer, aStartSector, aSectorCount);
    }

    stat2 = K2OSKERN_ReleaseObject(&pFileObj->Hdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat2));

    return stat;
}

static
K2STAT
sDoneDlx(
    K2OS_TOKEN  aTokDlxFile
)
{
    K2STAT              stat;
    K2STAT              stat2;
    FSPROV_OBJ_FILE *   pFileObj;

    stat = K2OSKERN_TranslateTokensToAddRefObjs(1, &aTokDlxFile, (K2OSKERN_OBJ_HEADER **)&pFileObj);
    if (K2STAT_IS_ERROR(stat))
        return stat;

    if (pFileObj->Hdr.mObjType != K2OS_Obj_File)
    {
        stat = K2STAT_ERROR_BAD_ARGUMENT;
    }
    else
    {
        stat = pFileObj->mpProvDirect->Close(pFileObj->mOpaque);
    }

    stat2 = K2OSKERN_ReleaseObject(&pFileObj->Hdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat2));

    return stat;
}


void FsProv_Init(K2OSEXEC_INIT_INFO * apInitInfo)
{
    K2OS_TOKEN tokSubscrip;

    gFsProv_TokNotify = K2OSKERN_NotifyCreate();
    K2_ASSERT(gFsProv_TokNotify != NULL);

    tokSubscrip = K2OSKERN_NotifySubscribe(gFsProv_TokNotify, &gK2OSEXEC_FsProvInterfaceGuid, NULL);
    K2_ASSERT(tokSubscrip != NULL);

    apInitInfo->OpenDlx = sOpenDlx;
    apInitInfo->ReadDlx = sReadDlx;
    apInitInfo->DoneDlx = sDoneDlx;
}
