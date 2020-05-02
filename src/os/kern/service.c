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

K2OS_TOKEN
K2OSKERN_ServiceCreate(
    K2OS_TOKEN  aTokMailslot,
    void *      apContext,
    UINT32 *    apRetInstanceId
)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_MAILSLOT * pMailslot;
    K2OSKERN_OBJ_SERVICE *  pSvc;
    BOOL                    disp;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    K2OS_TOKEN              tokService;

    if ((aTokMailslot == NULL) ||
        (apRetInstanceId == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    stat = KernTok_TranslateToAddRefObjs(1, &aTokMailslot, (K2OSKERN_OBJ_HEADER **)&pMailslot);
    if (!K2STAT_IS_ERROR(stat))
    {
        do {
            if (pMailslot->Hdr.mObjType != K2OS_Obj_Mailslot)
            {
                stat = K2STAT_ERROR_BAD_TOKEN;
                break;
            }

            pSvc = (K2OSKERN_OBJ_SERVICE *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_SERVICE));
            if (pSvc == NULL)
            {
                stat = K2STAT_ERROR_OUT_OF_MEMORY;
                break;
            }

            K2MEM_Zero(pSvc, sizeof(K2OSKERN_OBJ_SERVICE));

            pSvc->Hdr.mObjType = K2OS_Obj_Service;
            pSvc->Hdr.mObjFlags = 0;
            pSvc->Hdr.mpName = NULL;
            pSvc->Hdr.mRefCount = 1;
            K2LIST_Init(&pSvc->Hdr.WaitEntryPrioList);

            pSvc->mpSlot = pMailslot;
            pSvc->mpContext = apContext;
            K2LIST_Init(&pSvc->PublishList);

            disp = K2OSKERN_SeqIntrLock(&gData.ServTreeSeqLock);

            pSvc->ServTreeNode.mUserVal = ++gData.mLastServInstId;
            K2TREE_Insert(&gData.ServTree, pSvc->ServTreeNode.mUserVal, &pSvc->ServTreeNode);

            stat = KernObj_Add(&pSvc->Hdr, NULL);
            if (K2STAT_IS_ERROR(stat))
            {
                K2TREE_Remove(&gData.ServTree, &pSvc->ServTreeNode);
            }

            K2OSKERN_SeqIntrUnlock(&gData.ServTreeSeqLock, disp);

            if (K2STAT_IS_ERROR(stat))
                K2OS_HeapFree(pSvc);

        } while (0);

        if (K2STAT_IS_ERROR(stat))
        {
            stat2 = KernObj_Release(&pMailslot->Hdr);
            K2_ASSERT(!K2STAT_IS_ERROR(stat2));
        }
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    //
    // service object added
    // now create token
    //

    tokService = NULL;
    pObjHdr = &pSvc->Hdr;
    stat = KernTok_CreateNoAddRef(1, &pObjHdr, &tokService);
    if (K2STAT_IS_ERROR(stat))
    {
        stat2 = KernObj_Release(&pSvc->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    *apRetInstanceId = pSvc->ServTreeNode.mUserVal;

    K2_ASSERT(tokService != NULL);

    return tokService;
}

UINT32
K2OSKERN_ServiceGetInstanceId(
    K2OS_TOKEN  aTokService
)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_SERVICE *  pSvc;
    UINT32                  result;

    if (aTokService == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return 0;
    }

    stat = KernTok_TranslateToAddRefObjs(1, &aTokService, (K2OSKERN_OBJ_HEADER **)&pSvc);
    if (!K2STAT_IS_ERROR(stat))
    {
        if (pSvc->Hdr.mObjType != K2OS_Obj_Service)
            stat = K2STAT_ERROR_BAD_TOKEN;
        else
            result = pSvc->ServTreeNode.mUserVal;
    }

    stat2 = KernObj_Release(&pSvc->Hdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat2));
    
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return 0;
    }

    return result;
}

K2OS_TOKEN
K2OSKERN_ServicePublish(
    K2OS_TOKEN          aTokService,
    K2_GUID128 const *  apInterfaceId
)
{
    K2STAT                  stat;
    K2STAT                  stat2;
    K2OSKERN_OBJ_SERVICE *  pSvc;
    K2OSKERN_OBJ_PUBLISH *  pPublish;
    BOOL                    disp;
    K2OSKERN_OBJ_HEADER *   pObjHdr;
    K2OS_TOKEN              tokPublish;
    K2_GUID128              guid;
    K2OSKERN_IFACE *        pIFace;
    K2OSKERN_IFACE *        pUse;
    K2TREE_NODE *           pTreeNode;
    UINT32                  svcInstanceId;

    if ((aTokService == NULL) ||
        (apInterfaceId == NULL))
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    K2MEM_Copy(&guid, apInterfaceId, sizeof(K2_GUID128));

    stat = KernTok_TranslateToAddRefObjs(1, &aTokService, (K2OSKERN_OBJ_HEADER **)&pSvc);
    if (!K2STAT_IS_ERROR(stat))
    {
        do {
            if (pSvc->Hdr.mObjType != K2OS_Obj_Service)
            {
                stat = K2STAT_ERROR_BAD_TOKEN;
                break;
            }

            svcInstanceId = pSvc->ServTreeNode.mUserVal;

            pIFace = (K2OSKERN_IFACE *)K2OS_HeapAlloc(sizeof(K2OSKERN_IFACE));
            if (pIFace == NULL)
            {
                stat = K2STAT_ERROR_OUT_OF_MEMORY;
                break;
            }

            do {
                K2MEM_Copy(&pIFace->InterfaceId, &guid, sizeof(K2_GUID128));
                K2LIST_Init(&pIFace->PublishList);
                K2MEM_Zero(&pIFace->IfaceTreeNode, sizeof(K2TREE_NODE));

                pPublish = (K2OSKERN_OBJ_PUBLISH *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_PUBLISH));
                if (pPublish == NULL)
                {
                    stat = K2STAT_ERROR_OUT_OF_MEMORY;
                    break;
                }

                K2MEM_Zero(pPublish, sizeof(K2OSKERN_OBJ_SERVICE));

                pPublish->Hdr.mObjType = K2OS_Obj_Publish;
                pPublish->Hdr.mObjFlags = 0;
                pPublish->Hdr.mpName = NULL;
                pPublish->Hdr.mRefCount = 1;
                K2LIST_Init(&pPublish->Hdr.WaitEntryPrioList);

                pPublish->mpService = pSvc;

                disp = K2OSKERN_SeqIntrLock(&gData.ServTreeSeqLock);

                pTreeNode = K2TREE_Find(&gData.IfaceTree, (UINT32)pIFace);
                if (pTreeNode != NULL)
                {
                    pUse = K2_GET_CONTAINER(K2OSKERN_IFACE, pTreeNode, IfaceTreeNode);
                }
                else
                {
                    K2TREE_Insert(&gData.IfaceTree, (UINT32)pIFace, &pIFace->IfaceTreeNode);
                    pUse = pIFace;
                    pIFace = NULL;
                }
                K2_ASSERT(0 == K2MEM_Compare(&pUse->InterfaceId, &guid, sizeof(K2_GUID128)));

                pPublish->mpIFace = pUse;
                K2LIST_AddAtTail(&pUse->PublishList, &pPublish->IfacePublishListLink);
                K2LIST_AddAtTail(&pSvc->PublishList, &pPublish->ServicePublishListLink);

                stat = KernObj_Add(&pPublish->Hdr, NULL);
                if (K2STAT_IS_ERROR(stat))
                {
                    pPublish->mpIFace = NULL;
                    K2LIST_Remove(&pSvc->PublishList, &pPublish->ServicePublishListLink);
                    K2LIST_Remove(&pUse->PublishList, &pPublish->IfacePublishListLink);
                    if (pTreeNode != &pUse->IfaceTreeNode)
                    {
                        // restore so it will get freed
                        pIFace = pUse;
                        K2_ASSERT(pIFace->PublishList.mNodeCount == 0);
                        K2TREE_Remove(&gData.IfaceTree, &pIFace->IfaceTreeNode);
                    }
                }

                K2OSKERN_SeqIntrUnlock(&gData.ServTreeSeqLock, disp);

                if (K2STAT_IS_ERROR(stat))
                    K2OS_HeapFree(pPublish);

            } while (0);

            if (pIFace != NULL)
            {
                K2OS_HeapFree(pIFace);
            }

        } while (0);

        if (K2STAT_IS_ERROR(stat))
        {
            stat2 = KernObj_Release(&pSvc->Hdr);
            K2_ASSERT(!K2STAT_IS_ERROR(stat2));
        }
    }

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    //
    // publish done
    // now create token
    //

    tokPublish = NULL;
    pObjHdr = &pPublish->Hdr;
    stat = KernTok_CreateNoAddRef(1, &pObjHdr, &tokPublish);
    if (K2STAT_IS_ERROR(stat))
    {
        stat2 = KernObj_Release(&pPublish->Hdr);
        K2_ASSERT(!K2STAT_IS_ERROR(stat2));
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    K2_ASSERT(tokPublish != NULL);

    KernNotify_IFaceChange(&guid, svcInstanceId, TRUE);

    return tokPublish;
}

void KernPublish_Dispose(K2OSKERN_OBJ_PUBLISH *apPublish)
{
    BOOL                    check;
    BOOL                    disp;
    K2STAT                  stat;
    K2OSKERN_IFACE *        pIFace;
    K2OSKERN_OBJ_SERVICE *  pSvc;

    K2_ASSERT(apPublish->Hdr.mObjType == K2OS_Obj_Publish);
    K2_ASSERT(apPublish->Hdr.mRefCount == 0);
    K2_ASSERT(!(apPublish->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apPublish->Hdr.WaitEntryPrioList.mNodeCount == 0);

    check = !(apPublish->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    KernNotify_IFaceChange(
        &apPublish->mpIFace->InterfaceId, 
        apPublish->mpService->ServTreeNode.mUserVal, 
        FALSE);

    disp = K2OSKERN_SeqIntrLock(&gData.ServTreeSeqLock);

    //
    // detach from Interface
    //
    pIFace = apPublish->mpIFace;
    K2LIST_Remove(&pIFace->PublishList, &apPublish->IfacePublishListLink);
    if (pIFace->PublishList.mNodeCount == 0)
    {
        K2TREE_Remove(&gData.IfaceTree, &pIFace->IfaceTreeNode);
    }
    else
        pIFace = NULL;

    //
    // detach from service
    //
    pSvc = apPublish->mpService;
    K2LIST_Remove(&pSvc->PublishList, &apPublish->ServicePublishListLink);

    K2OSKERN_SeqIntrUnlock(&gData.ServTreeSeqLock, disp);

    stat = KernObj_Release(&pSvc->Hdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    if (pIFace != NULL)
    {
        K2MEM_Zero(pIFace, sizeof(K2OSKERN_IFACE));
        check = K2OS_HeapFree(pIFace);
        K2_ASSERT(check);
    }

    K2MEM_Zero(apPublish, sizeof(K2OSKERN_OBJ_SERVICE));

    if (check)
    {
        check = K2OS_HeapFree(apPublish);
        K2_ASSERT(check);
    }
}

K2STAT
K2OSKERN_ServiceCall(
    UINT32          aServiceInstanceId,
    UINT32          aCallCmd,
    void const *    apInBuf,
    UINT32          aInBufBytes,
    void *          apOutBuf,
    UINT32          aOutBufBytes,
    UINT32 *        apRetActualOut
)
{
    K2OSKERN_OBJ_MSG *      pMsg;
    K2OS_MSGIO              msgIo;
    K2OSKERN_SVC_MSGIO *    pCall;
    K2OSKERN_OBJ_SERVICE *  pSvc;
    BOOL                    disp;
    K2STAT                  stat;
    K2STAT                  stat2;
    K2TREE_NODE *           pTreeNode;
    UINT32                  waitResult;

    if (apRetActualOut != NULL)
        *apRetActualOut = 0;

    pMsg = (K2OSKERN_OBJ_MSG *)K2OS_HeapAlloc(sizeof(K2OSKERN_OBJ_MSG));
    if (pMsg == NULL)
        return K2STAT_ERROR_OUT_OF_MEMORY;
    stat = KernMsg_Create(pMsg);
    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_HeapFree(pMsg);
        return stat;
    }

    pCall = (K2OSKERN_SVC_MSGIO *)&msgIo;
    pCall->mSvcOpCode = SYSMSG_OPCODE_SVC_CALL;
    pCall->mCallCmd = aCallCmd;
    pCall->mpInBuf = apInBuf;
    pCall->mInBufBytes = aInBufBytes;
    pCall->mpOutBuf = apOutBuf;
    pCall->mOutBufBytes = aOutBufBytes;
    pCall->mRetActualOut = 0;

    do {
        disp = K2OSKERN_SeqIntrLock(&gData.ServTreeSeqLock);

        pTreeNode = K2TREE_Find(&gData.ServTree, aServiceInstanceId);
        if (pTreeNode != NULL)
        {
            pSvc = K2_GET_CONTAINER(K2OSKERN_OBJ_SERVICE, pTreeNode, ServTreeNode);

            stat = KernObj_AddRef(&pSvc->Hdr);
            if (!K2STAT_IS_ERROR(stat))
            {
                pCall->mpSvcContext = pSvc->mpContext;
            }
            else
                pTreeNode = NULL;
        }

        K2OSKERN_SeqIntrUnlock(&gData.ServTreeSeqLock, disp);

        if (pTreeNode != NULL)
        {
            K2_ASSERT(pSvc != NULL);

            stat = KernMsg_Send(pSvc->mpSlot, pMsg, &msgIo, TRUE);
            if (!K2STAT_IS_ERROR(stat))
            {
                waitResult = KernThread_WaitOne(&pMsg->Hdr, K2OS_TIMEOUT_INFINITE);
                if (waitResult != K2OS_WAIT_SIGNALLED_0)
                {
                    stat2 = KernMsg_Abort(pMsg);
                    K2_ASSERT(!K2STAT_IS_ERROR(stat2));
                    stat = K2STAT_ERROR_WAIT_ABANDONED;
                }
                else
                {
                    stat = KernMsg_ReadResponse(pMsg, &msgIo, TRUE);
                    if (!K2STAT_IS_ERROR(stat))
                    {
                        stat = msgIo.mStatus;
                        if (!K2STAT_IS_ERROR(stat))
                        {
                            if (apRetActualOut != NULL)
                                *apRetActualOut = pCall->mRetActualOut;
                        }
                    }
                }
            }

            stat2 = KernObj_Release(&pSvc->Hdr);
            K2_ASSERT(!K2STAT_IS_ERROR(stat2));
        }
        else
            stat = K2STAT_ERROR_NOT_FOUND;

    } while (0);

    stat2 = KernObj_Release(&pMsg->Hdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat2));

    return stat;
}

BOOL
K2OSKERN_ServiceEnum(
    K2_GUID128 const *  apInterfaceId,
    UINT32 *            apIoCount,
    UINT32 *            apRetServiceInstanceIds
)
{
    UINT32                  ioCount;
    BOOL                    disp;
    K2OSKERN_IFACE          iFace;
    K2OSKERN_IFACE *        pIFace;
    K2TREE_NODE *           pTreeNode;
    K2LIST_LINK *           pListLink;
    K2OSKERN_OBJ_SERVICE *  pSvc;
    K2OSKERN_OBJ_PUBLISH *  pPublish;
    K2STAT                  stat;

    if (apIoCount == NULL)
    {
        K2OS_ThreadSetStatus(K2STAT_ERROR_BAD_ARGUMENT);
        return FALSE;
    }

    if (gData.ServTree.mNodeCount == 0)
    {
        *apIoCount = 0;
        return TRUE;
    }

    if (apInterfaceId != NULL)
        K2MEM_Copy(&iFace.InterfaceId, apInterfaceId, sizeof(K2_GUID128));
    
    ioCount = *apIoCount;

    stat = K2STAT_NO_ERROR;

    disp = K2OSKERN_SeqIntrLock(&gData.ServTreeSeqLock);

    if (apInterfaceId == NULL)
    {
        //
        // enum all services in the system
        //
        if ((ioCount < gData.ServTree.mNodeCount) ||
            (apRetServiceInstanceIds == NULL))
        {
            ioCount = gData.ServTree.mNodeCount;
            stat = K2STAT_ERROR_TOO_SMALL;
        }
        else
        {
            ioCount = 0;
            pTreeNode = K2TREE_FirstNode(&gData.ServTree);
            do {
                pSvc = K2_GET_CONTAINER(K2OSKERN_OBJ_SERVICE, pTreeNode, ServTreeNode);
                pTreeNode = K2TREE_NextNode(&gData.ServTree, pTreeNode);
                *apRetServiceInstanceIds = pSvc->ServTreeNode.mUserVal;
                apRetServiceInstanceIds++;
                ioCount++;
            } while (pTreeNode != NULL);
        }
    }
    else
    {
        //
        // enum all the services that publish the specified interface
        //
        pTreeNode = K2TREE_Find(&gData.IfaceTree, (UINT32)&iFace);
        if (pTreeNode != NULL)
        {
            pIFace = K2_GET_CONTAINER(K2OSKERN_IFACE, pTreeNode, IfaceTreeNode);
            K2_ASSERT(pIFace->PublishList.mNodeCount > 0);
            if ((ioCount < pIFace->PublishList.mNodeCount) ||
                (apRetServiceInstanceIds == NULL))
            {
                ioCount = pIFace->PublishList.mNodeCount;
                stat = K2STAT_ERROR_TOO_SMALL;
            }
            else
            {
                ioCount = 0;
                pListLink = pIFace->PublishList.mpHead;
                K2_ASSERT(pListLink != NULL);
                do {
                    pPublish = K2_GET_CONTAINER(K2OSKERN_OBJ_PUBLISH, pListLink, IfacePublishListLink);
                    pListLink = pListLink->mpNext;
                    *apRetServiceInstanceIds = pPublish->mpService->ServTreeNode.mUserVal;
                    apRetServiceInstanceIds++;
                    ioCount++;
                } while (pListLink != NULL);
            }
        }

        K2OSKERN_SeqIntrUnlock(&gData.ServTreeSeqLock, disp);

        if (pTreeNode == NULL)
        {
            //
            // no interface with that id found
            //
            ioCount = 0;
            stat = K2STAT_ERROR_NOT_FOUND;
        }
    }

    *apIoCount = ioCount;

    if (K2STAT_IS_ERROR(stat))
    {
        K2OS_ThreadSetStatus(stat);
        return FALSE;
    }

    return TRUE;
}

void KernService_Dispose(K2OSKERN_OBJ_SERVICE *apService)
{
    BOOL                    check;
    BOOL                    disp;
    K2STAT                  stat;
    K2OSKERN_OBJ_MAILSLOT * pSlot;

    K2_ASSERT(apService->Hdr.mObjType == K2OS_Obj_Service);
    K2_ASSERT(apService->Hdr.mRefCount == 0);
    K2_ASSERT(!(apService->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT));
    K2_ASSERT(apService->Hdr.WaitEntryPrioList.mNodeCount == 0);

    check = !(apService->Hdr.mObjFlags & K2OSKERN_OBJ_FLAG_EMBEDDED);

    K2_ASSERT(apService->PublishList.mNodeCount == 0);

    disp = K2OSKERN_SeqIntrLock(&gData.ServTreeSeqLock);

    pSlot = apService->mpSlot;

    K2TREE_Remove(&gData.ServTree, &apService->ServTreeNode);
    apService->ServTreeNode.mUserVal = 0;

    K2OSKERN_SeqIntrUnlock(&gData.ServTreeSeqLock, disp);

    stat = KernObj_Release(&pSlot->Hdr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    K2MEM_Zero(apService, sizeof(K2OSKERN_OBJ_SERVICE));

    if (check)
    {
        check = K2OS_HeapFree(apService);
        K2_ASSERT(check);
    }
}
