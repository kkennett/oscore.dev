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

#define BUILTIN_SERVICE_CONTEXT ((void *)0xDEADFEED)
#define DRVSTORE_IFACE_CONTEXT  ((void *)0xFEEDDEAD)

typedef struct _BUILTIN_DLX BUILTIN_DLX;
struct _BUILTIN_DLX
{
    K2OS_TOKEN                      mToken;
    K2ROFS_FILE const *             mpFile;
    char const *                    mpDlxName;
    K2OS_pf_Driver_Register         mfRegister;
    K2OS_pf_Driver_PrepareInstance  mfPrepareInstance;
    K2OS_pf_Driver_ActivateInstance mfActivateInstance;
    K2OS_pf_Driver_PurgeInstance    mfPurgeInstance;

    char const *                    mpFriendlyName;
    UINT32                          mNumTypeIds;
    char const * const *            mppTypeIds;
};

typedef struct _BUILTIN_DRV_INST BUILTIN_DRV_INST;
struct _BUILTIN_DRV_INST
{
    INT32           mRefCount;
    BUILTIN_DLX *   mpDriver;
    UINT32          mDriverHandle;
    K2TREE_NODE     TreeNode;       // userval is pointer to instance
};

static K2ROFS const *       sgpRofs;
static K2ROFS_DIR const *   sgpBuiltinRoot;
static K2ROFS_DIR const *   sgpBuiltinKern;

static K2OS_TOKEN           sgTokMailbox;
static K2OS_TOKEN           sgTokService;
static UINT32               sgServiceId;
static K2OS_TOKEN           sgTokPublish;
static UINT32               sgDrvStoreInterfaceId;

static BUILTIN_DLX *        sgBuiltinDlx;
static UINT32               sgBuiltinDlxCount;

static K2TREE_ANCHOR        sgDrvInstanceTree;
static K2OSKERN_SEQLOCK     sgDrvInstanceTreeLock;


static 
K2STAT 
sBuiltin_Open(
    char const *    apRelSpec, 
    FSPROV_OPAQUE * apRetFile, 
    UINT32 *        apRetTotalSectors
)
{
    K2ROFS_FILE const *pFile;

    pFile = K2ROFSHELP_SubFile(sgpRofs, sgpBuiltinKern, apRelSpec);
    if (pFile == NULL)
        return K2STAT_ERROR_NOT_FOUND;

    *apRetFile = (FSPROV_OPAQUE)pFile;
    *apRetTotalSectors = K2_ROUNDUP(pFile->mSizeBytes, K2ROFS_SECTOR_BYTES) / K2ROFS_SECTOR_BYTES;

    return K2STAT_NO_ERROR;
}

static 
K2STAT 
sBuiltin_Read(
    FSPROV_OPAQUE   aFile, 
    void *          apBuffer, 
    UINT32          aSectorOffset,
    UINT32          aSectorCount
)
{
    K2ROFS_FILE const * pFile;
    UINT32              fileSectors;

    pFile = (K2ROFS_FILE const *)aFile;

    fileSectors = K2_ROUNDUP(pFile->mSizeBytes, K2ROFS_SECTOR_BYTES) / K2ROFS_SECTOR_BYTES;

    if ((aSectorOffset >= fileSectors) ||
        ((fileSectors - aSectorOffset) < aSectorCount))
        return K2STAT_ERROR_BAD_ARGUMENT;

    K2MEM_Copy(
        apBuffer, 
        K2ROFS_FILEDATA(sgpRofs, pFile) + 
            (aSectorOffset * K2ROFS_SECTOR_BYTES), 
        aSectorCount * K2ROFS_SECTOR_BYTES
    );

    return K2STAT_NO_ERROR;
}

static 
K2STAT 
sBuiltin_Close(
    FSPROV_OPAQUE aFile
)
{
    return K2STAT_NO_ERROR;
}

K2OSEXEC_FSPROV_DIRECT 
gFsProv_Builtin_Direct =
{
    K2OSEXEC_FSPROV_CURRENT_VERSION,
    sBuiltin_Open,
    sBuiltin_Read,
    sBuiltin_Close
};

void 
Builtin_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
)
{
    sgpRofs = apInitInfo->mpBuiltinRofs;

    sgpBuiltinRoot = K2ROFS_ROOTDIR(sgpRofs);
    K2_ASSERT(sgpBuiltinRoot != NULL);

    sgpBuiltinKern = K2ROFSHELP_SubDir(sgpRofs, sgpBuiltinRoot, "kern");
    K2_ASSERT(sgpBuiltinKern != NULL);
}

static
UINT32
K2_CALLCONV_REGS
sBuiltinThread(
    void *apParam
);

void
Builtin_Run(
    void
)
{
    UINT32              ix;
    UINT32              ix2;
    K2STAT              stat;
    K2ROFS_FILE const * pFile;
    char const *        pDlxName;
    char const *        pExt;
    char                ch;
    K2_EXCEPTION_TRAP   trap;
    K2OS_TOKEN          tokThread;
    K2OS_THREADCREATE   cret;

    K2OSKERN_Debug("\n\nLOADING BUILTIN DLX\n");

    sgBuiltinDlx = (BUILTIN_DLX *)K2OS_HeapAlloc(sizeof(BUILTIN_DLX) * sgpBuiltinKern->mFileCount);
    if (sgBuiltinDlx == NULL)
    {
        K2OSKERN_Debug("*** Could not allocate builtin DLX tokens\n");
        return;
    }
    K2MEM_Zero(sgBuiltinDlx, sizeof(BUILTIN_DLX) * sgpBuiltinKern->mFileCount);

    sgBuiltinDlxCount = 0;
    for (ix = 0; ix < sgpBuiltinKern->mFileCount; ix++)
    {
        pFile = K2ROFSHELP_SubFileIx(sgpRofs, sgpBuiltinKern, ix);
        pDlxName = K2ROFS_NAMESTR(sgpRofs, pFile->mName);
        pExt = pDlxName;
        do {
            ch = *pExt;
            pExt++;
            if (ch == '.')
            {
                if (0 == K2ASC_CompIns(pExt, "dlx"))
                {
                    //
                    // try to load this DLX 
                    //
                    sgBuiltinDlx[sgBuiltinDlxCount].mToken = K2OS_DlxLoad(NULL, pDlxName, NULL);
                    if (sgBuiltinDlx[sgBuiltinDlxCount].mToken == NULL)
                    {
                        K2OSKERN_Debug("  *** FAILED TO LOAD BUILTIN DLX \"%s\". Error %08X\n", pDlxName, K2OS_ThreadGetStatus());
                    }
                    else
                    {
                        sgBuiltinDlx[sgBuiltinDlxCount].mpFile = pFile;
                        sgBuiltinDlx[sgBuiltinDlxCount].mpDlxName = pDlxName;
                        sgBuiltinDlx[sgBuiltinDlxCount].mfRegister = 
                            (K2OS_pf_Driver_Register)K2OS_DlxFindExport(sgBuiltinDlx[sgBuiltinDlxCount].mToken, DlxSeg_Text, K2OS_DRIVER_REGISTER_FN_NAME);
                        sgBuiltinDlxCount++;
                    }
                    break;
                }
            }
        } while (ch != 0);
    }

    K2OSKERN_Debug("FINISHED BUILTIN DLX\n");

    K2OSKERN_Debug("Registering Drivers...\n");
    for (ix = 0; ix < sgBuiltinDlxCount; ix++)
    {
        if (sgBuiltinDlx[ix].mfRegister != NULL)
        {
            K2OSKERN_Debug("  Register %s...\n", sgBuiltinDlx[ix].mpDlxName);
            stat = K2_EXTRAP(&trap, sgBuiltinDlx[ix].mfRegister(&sgBuiltinDlx[ix].mpFriendlyName, &sgBuiltinDlx[ix].mNumTypeIds, &sgBuiltinDlx[ix].mppTypeIds));
            if (K2STAT_IS_ERROR(stat))
            {
                K2OSKERN_Debug("    *** Register failed %08X\n", stat);
                sgBuiltinDlx[ix].mfRegister = NULL;
                sgBuiltinDlx[ix].mpFriendlyName = NULL;
                sgBuiltinDlx[ix].mNumTypeIds = 0;
                sgBuiltinDlx[ix].mppTypeIds = NULL;
            }
            else
            {
                K2OSKERN_Debug("    Driver \"%s\" registered %d type Ids\n",
                    sgBuiltinDlx[ix].mpFriendlyName,
                    sgBuiltinDlx[ix].mNumTypeIds);
                for (ix2 = 0; ix2 < sgBuiltinDlx[ix].mNumTypeIds; ix2++)
                {
                    K2OSKERN_Debug("      %s\n", sgBuiltinDlx[ix].mppTypeIds[ix2]);
                }

                if (sgBuiltinDlx[ix].mNumTypeIds > 0)
                {
                    ch = 0;
                    sgBuiltinDlx[ix].mfPrepareInstance =
                        (K2OS_pf_Driver_PrepareInstance)K2OS_DlxFindExport(sgBuiltinDlx[ix].mToken, DlxSeg_Text, K2OS_DRIVER_PREPARE_INST_FN_NAME);
                    if (NULL != sgBuiltinDlx[ix].mfPrepareInstance)
                    {
                        sgBuiltinDlx[ix].mfActivateInstance =
                            (K2OS_pf_Driver_ActivateInstance)K2OS_DlxFindExport(sgBuiltinDlx[ix].mToken, DlxSeg_Text, K2OS_DRIVER_ACTIVATE_FN_NAME);
                        if (NULL != sgBuiltinDlx[ix].mfActivateInstance)
                        {
                            sgBuiltinDlx[ix].mfPurgeInstance =
                                (K2OS_pf_Driver_PurgeInstance)K2OS_DlxFindExport(sgBuiltinDlx[ix].mToken, DlxSeg_Text, K2OS_DRIVER_PURGE_FN_NAME);
                            if (NULL != sgBuiltinDlx[ix].mfPurgeInstance)
                            {
                                ch = 1;
                            }
                            else
                            {
                                K2OSKERN_Debug("Driver \"%s\" did not export %s\n", sgBuiltinDlx[ix].mpFriendlyName, K2OS_DRIVER_PURGE_FN_NAME);
                            }
                        }
                        else
                        {
                            K2OSKERN_Debug("Driver \"%s\" did not export %s\n", sgBuiltinDlx[ix].mpFriendlyName, K2OS_DRIVER_ACTIVATE_FN_NAME);
                        }
                    }
                    else
                    {
                        K2OSKERN_Debug("Driver \"%s\" did not export %s\n", sgBuiltinDlx[ix].mpFriendlyName, K2OS_DRIVER_PREPARE_INST_FN_NAME);
                    }
                    if (0 == ch)
                    {
                        K2OSKERN_Debug("Driver \"%s\" not usable - did not provide at least one requred export\n", sgBuiltinDlx[ix].mpFriendlyName);
                        sgBuiltinDlx[ix].mfPrepareInstance = NULL;
                        sgBuiltinDlx[ix].mfActivateInstance = NULL;
                        sgBuiltinDlx[ix].mNumTypeIds = 0;
                    }
                }
            }
        }
    }
    K2OSKERN_Debug("Finished Registering Drivers.\n");

    sgTokMailbox = K2OS_MailboxCreate(NULL, FALSE);
    K2_ASSERT(NULL != sgTokMailbox);

    sgTokService = K2OSKERN_ServiceCreate(
        sgTokMailbox,
        BUILTIN_SERVICE_CONTEXT,
        &sgServiceId);
    K2_ASSERT(NULL != sgTokService);
    K2_ASSERT(0 != sgServiceId);

    sgTokPublish = K2OSKERN_ServicePublish(
        sgTokService,
        &gK2OSEXEC_DriverStoreInterfaceGuid,
        DRVSTORE_IFACE_CONTEXT,
        &sgDrvStoreInterfaceId);
    K2_ASSERT(NULL != sgTokPublish);
    K2_ASSERT(0 != sgDrvStoreInterfaceId);

    K2TREE_Init(&sgDrvInstanceTree, NULL);
    K2OSKERN_SeqIntrInit(&sgDrvInstanceTreeLock);

    K2MEM_Zero(&cret, sizeof(cret));
    cret.mStructBytes = sizeof(cret);
    cret.mEntrypoint = sBuiltinThread;
    cret.mpArg = NULL;

    tokThread = K2OS_ThreadCreate(&cret);
    K2_ASSERT(NULL != tokThread);
    K2OS_TokenDestroy(tokThread);
}

static K2OSEXEC_DRVSTORE_INFO const sgDrvStoreInfo =
{
    K2OSEXEC_DRVSTORE_ID_BUILTIN,     // {AFA9A8C6-D494-4D7B-9423-169E416C2396}
    "Built-In",
    0x00010000
};

static K2STAT sDrvStore_FindDriver(UINT32 aNumTypeIds, char const ** appTypeIds, UINT32 * apRetSelect, void **appRetContext)
{
    UINT32 ix;
    UINT32 ix2;
    UINT32 ix3;
    UINT32 numIds;

//    K2OSKERN_Debug("Builtin:Direct:FindDriver(%d)\n", aNumTypeIds);
//    for (ix3 = 0; ix3 < aNumTypeIds; ix3++)
//    {
//        K2OSKERN_Debug("  %3d: %s\n", ix3, appTypeIds[ix3]);
//    }

    for (ix = 0; ix < sgBuiltinDlxCount; ix++)
    {
        numIds = sgBuiltinDlx[ix].mNumTypeIds;
        if (0 == numIds)
            continue;
        for (ix2 = 0; ix2 < numIds; ix2++)
        {
            for (ix3 = 0; ix3 < aNumTypeIds; ix3++)
            {
                if (0 == K2ASC_CompIns(sgBuiltinDlx[ix].mppTypeIds[ix2], appTypeIds[ix3]))
                    break;
            }
            if (ix3 < aNumTypeIds)
                break;
        }
        if (ix2 < numIds)
            break;
    }

    if (ix == sgBuiltinDlxCount)
        return K2STAT_ERROR_NOT_FOUND;

    K2OSKERN_Debug("  Matched supported driver index %d in \"%s\"\n", ix2, sgBuiltinDlx[ix].mpFriendlyName);

    *apRetSelect = ix3;
    *appRetContext = (void *)((ix2 << 16) | ix);

    return K2STAT_NO_ERROR;
}


static K2STAT sDrvStore_PrepareDriverInstance(void *apFindResultContext, char const *apTypeId, UINT32 *apRetStoreHandle)
{
    UINT32              ix;
    UINT32              ix2;
    K2STAT              stat;
    K2STAT              stat2;
    K2_EXCEPTION_TRAP   trap;
    UINT32              driverHandle;
    BUILTIN_DRV_INST *  pInst;
    BOOL                disp;

    ix = (UINT32)apFindResultContext;
    ix2 = ix >> 16;
    ix &= 0xFFFF;
    K2_ASSERT(ix < sgBuiltinDlxCount);
    K2_ASSERT(ix2 < sgBuiltinDlx[ix].mNumTypeIds);
//    K2OSKERN_Debug("  Prepare instance for index %d in \"%s\"\n", ix2, sgBuiltinDlx[ix].mpFriendlyName);

    stat = K2_EXTRAP(&trap, sgBuiltinDlx[ix].mfPrepareInstance(apTypeId, &driverHandle));
    if (!K2STAT_IS_ERROR(stat))
    {
        //
        // build translation entry for driverHandle->storeHandle
        //
        pInst = (BUILTIN_DRV_INST *)K2OS_HeapAlloc(sizeof(BUILTIN_DRV_INST));
        if (pInst == NULL)
        {
            stat2 = K2_EXTRAP(&trap, sgBuiltinDlx[ix].mfPurgeInstance(driverHandle));
            if (K2STAT_IS_ERROR(stat2))
            {
                K2OSKERN_Debug("Failed to purge driver instance %08X\n", stat2);
            }
            return stat;
        }

        pInst->mRefCount = 1;
        pInst->mpDriver = &sgBuiltinDlx[ix];
        pInst->mDriverHandle = driverHandle;
        pInst->TreeNode.mUserVal = (UINT32)pInst;

        disp = K2OSKERN_SeqIntrLock(&sgDrvInstanceTreeLock);
        K2TREE_Insert(&sgDrvInstanceTree, (UINT32)pInst, &pInst->TreeNode);
        K2OSKERN_SeqIntrUnlock(&sgDrvInstanceTreeLock, disp);

        *apRetStoreHandle = (UINT32)pInst;
    }
    else
    {
        *apRetStoreHandle = 0;
    }

    return stat;
}

static
BUILTIN_DRV_INST * 
sFindAddRef(
    UINT32 aStoreHandle
)
{
    BUILTIN_DRV_INST *  pInst;
    BOOL                disp;
    K2TREE_NODE *       pTreeNode;

    disp = K2OSKERN_SeqIntrLock(&sgDrvInstanceTreeLock);

    pTreeNode = K2TREE_Find(&sgDrvInstanceTree, aStoreHandle);
    if (NULL != pTreeNode)
    {
        pInst = K2_GET_CONTAINER(BUILTIN_DRV_INST, pTreeNode, TreeNode);
        pInst->mRefCount++;
    }
    else
        pInst = NULL;

    K2OSKERN_SeqIntrUnlock(&sgDrvInstanceTreeLock, disp);

    return pInst;
}

static
void
sRelease(
    BUILTIN_DRV_INST *  apInst
)
{
    BUILTIN_DRV_INST *  pInst;
    BOOL                disp;
    K2TREE_NODE *       pTreeNode;
    K2STAT              stat;
    K2_EXCEPTION_TRAP   trap;

    disp = K2OSKERN_SeqIntrLock(&sgDrvInstanceTreeLock);

    pTreeNode = K2TREE_Find(&sgDrvInstanceTree, (UINT32)apInst);
    K2_ASSERT(NULL != pTreeNode);

    pInst = K2_GET_CONTAINER(BUILTIN_DRV_INST, pTreeNode, TreeNode);
    --pInst->mRefCount;
    if (0 == pInst->mRefCount)
    {
        K2TREE_Remove(&sgDrvInstanceTree, pTreeNode);
    }
    else
        pInst = NULL;

    K2OSKERN_SeqIntrUnlock(&sgDrvInstanceTreeLock, disp);

    if (NULL != pInst)
    {
        stat = K2_EXTRAP(&trap, pInst->mpDriver->mfPurgeInstance(pInst->mDriverHandle));
        K2_ASSERT(!K2STAT_IS_ERROR(stat));
        K2OS_HeapFree(pInst);
    }
}

static K2STAT sDrvStore_ActivateDriver(UINT32 aStoreHandle, UINT32 aDevInstanceId, BOOL aSetActive)
{
    BUILTIN_DRV_INST *  pInst;
    K2_EXCEPTION_TRAP   trap;
    K2STAT              stat;

    pInst = sFindAddRef(aStoreHandle);
    if (pInst == NULL)
        return K2STAT_ERROR_NOT_FOUND;

    stat = K2_EXTRAP(&trap, pInst->mpDriver->mfActivateInstance(pInst->mDriverHandle, aDevInstanceId, aSetActive));

    sRelease(pInst);

    return stat;
}

static K2STAT sDrvStore_PurgeDriverInstance(UINT32 aStoreHandle)
{
    BUILTIN_DRV_INST *  pInst;

    pInst = sFindAddRef(aStoreHandle);
    if (pInst == NULL)
        return K2STAT_ERROR_NOT_FOUND;

    sRelease(pInst);    // addRef
    sRelease(pInst);    // original ref

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

static
K2STAT
sDrvStoreServiceCall(
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
        K2OSKERN_Debug("Builtin:DrvStore:Unknown call %08X\n", aCallCmd);
        return K2STAT_ERROR_NOT_IMPL;
    }

    return K2STAT_NO_ERROR;
}

static
UINT32
K2_CALLCONV_REGS
sBuiltinThread(
    void *apParam
)
{
    K2OSKERN_SVC_MSGIO  msgIo;
    UINT32              waitResult;
    UINT32              requestId;
    BOOL                ok;
    UINT32              actualOut;

    do {
        waitResult = K2OS_ThreadWait(1, &sgTokMailbox, FALSE, K2OS_TIMEOUT_INFINITE);
        K2_ASSERT(waitResult == K2OS_WAIT_SIGNALLED_0);
        requestId = 0;
        ok = K2OS_MailboxRecv(sgTokMailbox, (K2OS_MSGIO *)&msgIo, &requestId);
        K2_ASSERT(ok);
        if (requestId != 0)
        {
            //
            // pending request
            //
            if (msgIo.mSvcOpCode == SYSMSG_OPCODE_SVC_CALL)
            {
                actualOut = 0;
                if (msgIo.mInBufBytes == 0)
                    msgIo.mpInBuf = NULL;
                if (msgIo.mOutBufBytes == 0)
                    msgIo.mpOutBuf = NULL;
                if ((msgIo.mpServiceContext == BUILTIN_SERVICE_CONTEXT) &&
                    (msgIo.mpPublishContext == DRVSTORE_IFACE_CONTEXT))
                {
                    ((K2OS_MSGIO *)&msgIo)->mStatus = sDrvStoreServiceCall(
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
                K2_ASSERT(actualOut <= msgIo.mOutBufBytes);
                ((K2OS_MSGIO *)&msgIo)->mPayload[0] = actualOut;
            }
            else
            {
                K2OSKERN_Debug("***Builtin service mailbox received bad msg\n");
                ((K2OS_MSGIO *)&msgIo)->mStatus = K2STAT_ERROR_NOT_IMPL;
                ((K2OS_MSGIO *)&msgIo)->mPayload[0] = 0;
            }
            ok = K2OS_MailboxRespond(sgTokMailbox, requestId, (K2OS_MSGIO *)&msgIo);
            K2_ASSERT(ok);
        }
        else
        {
            //
            // notify message
            //
        }
    } while (1);

    return 0;
}

void Builtin_Dispose(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    K2OSKERN_Debug("Builtin_Dispose(%08X)\n", apObjHdr);
    K2_ASSERT(0);
}
