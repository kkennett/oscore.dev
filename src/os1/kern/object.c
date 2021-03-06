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

static char const * sgpK2OSEXEC_MailboxName = "{51646997-5DE4-4175-AC0B-94BF2B57C0F4}";

void sObjectDispose(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    //    K2OSKERN_Debug("ObjectDispose(%d)\n", apObjHdr->mObjType);
    K2_ASSERT(apObjHdr->Dispose != NULL);
    apObjHdr->Dispose(apObjHdr);
}

K2STAT KernObj_AddName(K2OSKERN_OBJ_NAME *apNewName, K2OSKERN_OBJ_NAME **appRetActual)
{
    K2STAT          stat;
    UINT32          disp;
    K2TREE_NODE *   pObjTreeNode;
    K2TREE_NODE *   pNameTreeNode;

    K2_ASSERT(apNewName != NULL);
    K2_ASSERT(apNewName->Hdr.mRefCount > 0);
    K2_ASSERT(apNewName->Hdr.mObjType == K2OS_Obj_Name);
    K2_ASSERT(apNewName->Hdr.mpName == NULL);

    K2_ASSERT(appRetActual != NULL);
    *appRetActual = NULL;

    K2LIST_Init(&apNewName->Hdr.WaitEntryPrioList);

    disp = K2OSKERN_SeqIntrLock(&gData.ObjTreeSeqLock);

    pObjTreeNode = K2TREE_Find(&gData.ObjTree, (UINT32)apNewName);
    if (pObjTreeNode == NULL)
    {
        //
        // object itself is not already known, which means we find it or add it
        //

        pNameTreeNode = K2TREE_Find(&gData.NameTree, (UINT32)(apNewName->NameBuffer));
        if (pNameTreeNode != NULL)
        {
            //
            // same name already exists.  get a pointer to it and addref it
            //
            *appRetActual = K2_GET_CONTAINER(K2OSKERN_OBJ_NAME, pNameTreeNode, NameTreeNode);
            (*appRetActual)->Hdr.mRefCount++;

            stat = K2STAT_ALREADY_EXISTS;   // this is not an error, it is a status
        }
        else
        {
            //
            // name did not exist, so add it and the new object to the respective trees.
            //
            apNewName->NameTreeNode.mUserVal = (UINT32)apNewName->NameBuffer;
            K2TREE_Insert(&gData.NameTree, apNewName->NameTreeNode.mUserVal, &apNewName->NameTreeNode);

            apNewName->Hdr.ObjTreeNode.mUserVal = (UINT32)&apNewName->Hdr;
            K2TREE_Insert(&gData.ObjTree, (UINT32)&apNewName->Hdr, &apNewName->Hdr.ObjTreeNode);

            *appRetActual = apNewName;

            stat = K2STAT_NO_ERROR;
        }
    }
    else
    {
        stat = K2STAT_ERROR_ALREADY_EXISTS;
    }

    K2OSKERN_SeqIntrUnlock(&gData.ObjTreeSeqLock, disp);

    return stat;
}

K2STAT K2OSKERN_AddObject(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    return KernObj_Add(apObjHdr, NULL);
}
K2STAT KernObj_Add(K2OSKERN_OBJ_HEADER *apObjHdr, K2OSKERN_OBJ_NAME *apObjName)
{
    K2STAT          stat;
    UINT32          disp;
    K2TREE_NODE *   pObjTreeNode;

    K2_ASSERT(apObjHdr != NULL);
    K2_ASSERT(apObjHdr->mRefCount > 0);
    K2_ASSERT(apObjHdr->mObjType != K2OS_Obj_None);
    K2_ASSERT(apObjHdr->mObjType != K2OS_Obj_Name);
    K2_ASSERT(apObjHdr->mpName == NULL);

    if (apObjHdr->mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT)
    {
        K2_ASSERT(apObjHdr->mRefCount = 0x7FFFFFFF);
    }

    if (apObjName != NULL)
    {
        K2_ASSERT(apObjName->Hdr.mObjType == K2OS_Obj_Name);
        K2_ASSERT(apObjName->Hdr.mpName == NULL);

        K2OS_CritSecEnter(&apObjName->OwnerSec);

        if (apObjName->mpObject != NULL)
        {
            stat = K2STAT_ERROR_ALREADY_EXISTS;
        }
        else
        {
            stat = K2OSKERN_AddRefObject(&apObjName->Hdr);
        }

        if (K2STAT_IS_ERROR(stat))
        {
            K2OS_CritSecLeave(&apObjName->OwnerSec);
            return stat;
        }
    }

    //
    // if object is being named, then the name has been addref'd and
    // the name owner sec is locked here
    //

    K2LIST_Init(&apObjHdr->WaitEntryPrioList);

    disp = K2OSKERN_SeqIntrLock(&gData.ObjTreeSeqLock);

    pObjTreeNode = K2TREE_Find(&gData.ObjTree, (UINT32)apObjHdr);

    if (pObjTreeNode == NULL)
    {
        stat = K2STAT_NO_ERROR;

        if (apObjName != NULL)
        {
            apObjHdr->mpName = apObjName;
            apObjName->mpObject = apObjHdr;
        }

        apObjHdr->ObjTreeNode.mUserVal = (UINT32)apObjHdr;
        K2TREE_Insert(&gData.ObjTree, (UINT32)apObjHdr, &apObjHdr->ObjTreeNode);
    }
    else
    {
        stat = K2STAT_ERROR_ALREADY_EXISTS;
    }

    K2OSKERN_SeqIntrUnlock(&gData.ObjTreeSeqLock, disp);

    if (K2STAT_IS_ERROR(stat))
    {
        if (apObjName != NULL)
        {
            K2OSKERN_ReleaseObject(&apObjName->Hdr);
        }
    }

    if (apObjName != NULL)
    {
        if (NULL == gData.mpMsgBox_K2OSEXEC)
        {
            //
            // K2EXEC has not started its mail slot yet.  is this it?
            //
            if (0 == K2ASC_CompIns(apObjName->NameBuffer, sgpK2OSEXEC_MailboxName))
            {
                gData.mpMsgBox_K2OSEXEC = K2_GET_CONTAINER(K2OSKERN_OBJ_MAILBOX, apObjName->mpObject, Hdr);
                K2_ASSERT(gData.mpMsgBox_K2OSEXEC->Hdr.mObjType == K2OS_Obj_Mailbox);
            }
        }

        KernEvent_Change(&apObjName->Event_IsOwned, TRUE);

        K2OS_CritSecLeave(&apObjName->OwnerSec);
    }

    return stat;
}

K2STAT K2OSKERN_AddRefObject(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    BOOL            disp;
    K2TREE_NODE *   pTreeNode;

    K2_ASSERT(apObjHdr != NULL);

    disp = K2OSKERN_SeqIntrLock(&gData.ObjTreeSeqLock);

    pTreeNode = K2TREE_Find(&gData.ObjTree, (UINT32)apObjHdr);

    if ((pTreeNode != NULL) && (!(apObjHdr->mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT)))
    {
        K2ATOMIC_Inc(&apObjHdr->mRefCount);
    }

    K2OSKERN_SeqIntrUnlock(&gData.ObjTreeSeqLock, disp);

    if (pTreeNode == NULL)
        return K2STAT_ERROR_NOT_FOUND;

    return K2STAT_NO_ERROR;
}

K2STAT K2OSKERN_ReleaseObject(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    UINT32              disp;
    K2STAT              stat;
    K2TREE_NODE *       pTreeNode;
    BOOL                refDecToZero;
    BOOL                nameRelease;
    K2OSKERN_OBJ_DLX *  pDecDlxToOne;
    K2OSKERN_OBJ_NAME * pName;

    K2_ASSERT(apObjHdr != NULL);

    nameRelease = FALSE;
    refDecToZero = FALSE;
    pDecDlxToOne = NULL;

    disp = K2OSKERN_SeqIntrLock(&gData.ObjTreeSeqLock);

    pTreeNode = K2TREE_Find(&gData.ObjTree, (UINT32)apObjHdr);

    if (pTreeNode != NULL)
    {
        if (!(apObjHdr->mObjFlags & K2OSKERN_OBJ_FLAG_PERMANENT))
        {
            if (apObjHdr->mRefCount == 1)
            {
                if (apObjHdr->mpName != NULL)
                {
                    //
                    // need to disconnect from name first
                    //
                    nameRelease = TRUE;
                    pName = apObjHdr->mpName;
                    apObjHdr->mpName = NULL;
                }
                else
                {
                    apObjHdr->mRefCount = 0;
                    K2_CpuWriteBarrier();
                    K2TREE_Remove(&gData.ObjTree, &apObjHdr->ObjTreeNode);
                    if (apObjHdr->mObjType == K2OS_Obj_Name)
                    {
                        K2TREE_Remove(&gData.NameTree, &((K2OSKERN_OBJ_NAME *)apObjHdr)->NameTreeNode);
                    }
                    refDecToZero = TRUE;
                }
            }
            else
            {
                --apObjHdr->mRefCount;
                if ((apObjHdr->mObjType == K2OS_Obj_DLX) && 
                    (apObjHdr->mRefCount == 1))
                {
                    pDecDlxToOne = (K2OSKERN_OBJ_DLX *)apObjHdr;
                }
                K2_CpuWriteBarrier();
                apObjHdr = NULL;
            }
        }
        else
            apObjHdr = NULL;
    }
    else
    {
        K2OSKERN_Debug("!!!K2OSKERN_ReleaseObject - object %08X not found!\n");
    }

    K2OSKERN_SeqIntrUnlock(&gData.ObjTreeSeqLock, disp);

    //
    // apObjHdr object may be GONE here if we were not the last release
    //

    if (pTreeNode == NULL)
    {
        return K2STAT_ERROR_NOT_FOUND;
    }

    if (apObjHdr == NULL)
    {
        //
        // object was found but this WAS NOT the last release, or object is permanent
        //
        if (pDecDlxToOne != NULL)
        {
            //
            // release our external reference to the dlx, so that 
            // only the dlx internal reference(s) exist now.  This may call
            // back through a purge to remove the last reference and dispose
            // of the object.  we are outside of object tree locks here
            // and should be able to do that.
            //
            DLX_Release(pDecDlxToOne->mpDlx);
        }
        return K2STAT_NO_ERROR;
    }

    //
    // object was found and this *was* the last release at the point of the call.
    // see if we need to disconnect from the name.  if we do then we need to 
    // reassess object disposition after that operation since somebody else may
    // have been able to addref to this object through the name before we had
    // a chance to disconnect from the name.
    //
    if (nameRelease)
    {
        K2_ASSERT(!refDecToZero);

        K2_ASSERT(pName->mpObject == apObjHdr);

        K2OS_CritSecEnter(&pName->OwnerSec);

        pName->mpObject = NULL;

        KernEvent_Change(&pName->Event_IsOwned, FALSE);

        K2OS_CritSecLeave(&pName->OwnerSec);

        stat = K2OSKERN_ReleaseObject(&pName->Hdr);

        K2_ASSERT(!K2STAT_IS_ERROR(stat));

        disp = K2OSKERN_SeqIntrLock(&gData.ObjTreeSeqLock);

        if (apObjHdr->mRefCount == 1)
        {
            apObjHdr->mRefCount = 0;
            K2_CpuWriteBarrier();
            K2TREE_Remove(&gData.ObjTree, &apObjHdr->ObjTreeNode);
            refDecToZero = TRUE;
        }

        K2OSKERN_SeqIntrUnlock(&gData.ObjTreeSeqLock, disp);
    }

    if (refDecToZero)
    {
        sObjectDispose(apObjHdr);
    }

    return K2STAT_NO_ERROR;
}

