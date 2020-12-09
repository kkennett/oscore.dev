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

#include "k2osexec.h"

static char const * const sgpTypeIds[1] =
{
    "ACPI/HID/PNP0A03"
};

typedef struct _PCIBUS PCIBUS;
struct _PCIBUS
{
    INT32           mRefCount;
    BOOL            mActive;
    UINT32          mDevInstanceId;
    K2LIST_LINK     ListLink;
    K2OS_CRITSEC    Sec;
};

static UINT32           sgInstanceCount;
static K2OS_CRITSEC     sgSec;
static K2LIST_ANCHOR    sgList;

K2STAT
K2OS_Driver_Register(
    char const **           appRetDriverFriendlyName,
    UINT32 *                apRetNumTypeIds,
    char const * const **   appRetTypeIds
    )
{
    if (NULL != appRetDriverFriendlyName)
        *appRetDriverFriendlyName = "PCI Bus";

    if (NULL != apRetNumTypeIds)
        *apRetNumTypeIds = sizeof(sgpTypeIds) / sizeof(char const *);

    if (NULL != appRetTypeIds)
        *appRetTypeIds = sgpTypeIds;

    return K2STAT_NO_ERROR;
}

K2STAT
K2OS_Driver_PrepareInstance(
    char const *    apTypeId,
    UINT32 *        apRetHandle
)
{
    PCIBUS *pBus;

    if (0 != K2ASC_CompIns(apTypeId, sgpTypeIds[0]))
        return K2STAT_ERROR_NOT_SUPPORTED;

    pBus = (PCIBUS *)K2OS_HeapAlloc(sizeof(PCIBUS));
    if (NULL == pBus)
        return K2STAT_ERROR_OUT_OF_MEMORY;

    K2MEM_Zero(pBus, sizeof(PCIBUS));

    pBus->mRefCount = 1;
    pBus->mActive = FALSE;
    pBus->mDevInstanceId = (UINT32)-1;
    K2OS_CritSecInit(&pBus->Sec);

    K2OS_CritSecEnter(&sgSec);

    K2LIST_AddAtTail(&sgList, &pBus->ListLink);

    K2OS_CritSecLeave(&sgSec);

    *apRetHandle = (UINT32)pBus;

    return K2STAT_NO_ERROR;
}

static 
PCIBUS * 
sFindAddRef(
    UINT32 aHandle
)
{
    K2LIST_LINK *   pListLink;
    PCIBUS *        pBus;

    pBus = NULL;

    K2OS_CritSecEnter(&sgSec);

    pListLink = sgList.mpHead;
    if (NULL != pListLink)
    {
        do {
            pBus = K2_GET_CONTAINER(PCIBUS, pListLink, ListLink);
            if (pBus == (PCIBUS *)aHandle)
            {
                pBus->mRefCount++;
                break;
            }
            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);
    }

    K2OS_CritSecLeave(&sgSec);

    if (NULL != pListLink)
        return pBus;

    return NULL;
}

static
void
sRelease(
    PCIBUS *apBus
)
{
    K2LIST_LINK *   pListLink;
    PCIBUS *        pBus;

    pBus = NULL;

    K2OS_CritSecEnter(&sgSec);

    pListLink = sgList.mpHead;
    if (NULL != pListLink)
    {
        do {
            pBus = K2_GET_CONTAINER(PCIBUS, pListLink, ListLink);
            if (pBus == apBus)
            {
                --pBus->mRefCount;
                if (0 != pBus->mRefCount)
                    pBus = NULL;
                else
                {
                    K2LIST_Remove(&sgList, &pBus->ListLink);
                }
                break;
            }
            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);
    }

    K2OS_CritSecLeave(&sgSec);

    if (NULL != pBus)
    {
        K2_ASSERT(0 == pBus->mRefCount);
        K2_ASSERT(FALSE == pBus->mActive);
        K2OS_CritSecDone(&pBus->Sec);
        K2OS_HeapFree(pBus);
    }
}

K2STAT
K2OS_Driver_ActivateInstance(
    UINT32  aHandle,
    UINT32  aDevInstanceId,
    BOOL    aSetActive
    )
{
    PCIBUS * pBus;

    pBus = sFindAddRef(aHandle);
    if (NULL == pBus)
    {
        return K2STAT_ERROR_NOT_FOUND;
    }

    K2OS_CritSecEnter(&pBus->Sec);

    if (aSetActive)
    {
        pBus->mActive = TRUE;
        pBus->mDevInstanceId = aDevInstanceId;
    }
    else
    {
        pBus->mActive = FALSE;
        pBus->mDevInstanceId = (UINT32)-1;
    }

    K2OS_CritSecLeave(&pBus->Sec);

    sRelease(pBus);

    return K2STAT_NO_ERROR;
}

K2STAT
K2OS_Driver_PurgeInstance(
    UINT32  aHandle
    )
{
    PCIBUS * pBus;

    pBus = sFindAddRef(aHandle);
    if (NULL == pBus)
    {
        return K2STAT_ERROR_NOT_FOUND;
    }

    if (pBus->mActive)
    {
        sRelease(pBus);
        return K2STAT_ERROR_IN_USE;
    }

    sRelease(pBus); // for addref
    sRelease(pBus); // initial add

    return K2STAT_NO_ERROR;
}

K2STAT
K2_CALLCONV_REGS
dlx_entry(
    DLX *   apDlx,
    UINT32  aReason
)
{
    BOOL ok;

    sgInstanceCount = 0;

    ok = K2OS_CritSecInit(&sgSec);
    K2_ASSERT(ok);

    K2LIST_Init(&sgList);

    return K2STAT_NO_ERROR;
}

