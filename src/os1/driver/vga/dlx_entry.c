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

#include "vga.h"

static char const * const sgpTypeIds[] =
{
    "PCICLASS/00/01",   // unclassified VGA compatible
    "PCICLASS/03/00"    // display controller, VGA compatible
};
#define TYPEID_COUNT    (sizeof(sgpTypeIds) / sizeof(char const *))

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
        *appRetDriverFriendlyName = "VGA Display";

    if (NULL != apRetNumTypeIds)
        *apRetNumTypeIds = TYPEID_COUNT;

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
    VGADISP *   pVga;
    UINT32      ix;

    for (ix = 0; ix < TYPEID_COUNT; ix++)
    {
        if (0 == K2ASC_CompIns(apTypeId, sgpTypeIds[ix]))
            break;
    }
    if (ix == TYPEID_COUNT)
        return K2STAT_ERROR_NOT_SUPPORTED;

    pVga = (VGADISP *)K2OS_HeapAlloc(sizeof(VGADISP));
    if (NULL == pVga)
        return K2STAT_ERROR_OUT_OF_MEMORY;

    K2MEM_Zero(pVga, sizeof(VGADISP));

    pVga->mRefCount = 1;
    pVga->mActive = FALSE;
    pVga->mDevInstanceId = (UINT32)-1;
    pVga->mpMatchTypeId = sgpTypeIds[ix];
    K2OS_CritSecInit(&pVga->Sec);

    K2OS_CritSecEnter(&sgSec);

    K2LIST_AddAtTail(&sgList, &pVga->ListLink);

    K2OS_CritSecLeave(&sgSec);

    *apRetHandle = (UINT32)pVga;

    return K2STAT_NO_ERROR;
}

static 
VGADISP * 
sFindAddRef(
    UINT32 aHandle
)
{
    K2LIST_LINK *   pListLink;
    VGADISP *        pVga;

    pVga = NULL;

    K2OS_CritSecEnter(&sgSec);

    pListLink = sgList.mpHead;
    if (NULL != pListLink)
    {
        do {
            pVga = K2_GET_CONTAINER(VGADISP, pListLink, ListLink);
            if (pVga == (VGADISP *)aHandle)
            {
                pVga->mRefCount++;
                break;
            }
            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);
    }

    K2OS_CritSecLeave(&sgSec);

    if (NULL != pListLink)
        return pVga;

    return NULL;
}

static
void
sRelease(
    VGADISP *apVga
)
{
    K2LIST_LINK *   pListLink;
    VGADISP *        pVga;

    pVga = NULL;

    K2OS_CritSecEnter(&sgSec);

    pListLink = sgList.mpHead;
    if (NULL != pListLink)
    {
        do {
            pVga = K2_GET_CONTAINER(VGADISP, pListLink, ListLink);
            if (pVga == apVga)
            {
                --pVga->mRefCount;
                if (0 != pVga->mRefCount)
                    pVga = NULL;
                else
                {
                    K2LIST_Remove(&sgList, &pVga->ListLink);
                }
                break;
            }
            pListLink = pListLink->mpNext;
        } while (pListLink != NULL);
    }

    K2OS_CritSecLeave(&sgSec);

    if (NULL != pVga)
    {
        K2_ASSERT(0 == pVga->mRefCount);
        K2_ASSERT(FALSE == pVga->mActive);
        K2OS_CritSecDone(&pVga->Sec);
        K2OS_HeapFree(pVga);
    }
}

K2STAT
K2OS_Driver_ActivateInstance(
    UINT32  aHandle,
    UINT32  aDevInstanceId,
    BOOL    aSetActive
    )
{
    VGADISP *   pVga;
    K2STAT      stat;

    pVga = sFindAddRef(aHandle);
    if (NULL == pVga)
    {
        return K2STAT_ERROR_NOT_FOUND;
    }

    K2OS_CritSecEnter(&pVga->Sec);

    if (aSetActive)
    {
        stat = VGA_LockedActivateInstance(pVga, aDevInstanceId);
    }
    else
    {
        stat = VGA_LockedDeactivateInstance(pVga, aDevInstanceId);
    }

    K2OS_CritSecLeave(&pVga->Sec);

    sRelease(pVga);

    return stat;
}

K2STAT
K2OS_Driver_PurgeInstance(
    UINT32  aHandle
    )
{
    VGADISP * pVga;

    pVga = sFindAddRef(aHandle);
    if (NULL == pVga)
    {
        return K2STAT_ERROR_NOT_FOUND;
    }

    if (pVga->mActive)
    {
        sRelease(pVga);
        return K2STAT_ERROR_IN_USE;
    }

    sRelease(pVga); // for addref
    sRelease(pVga); // initial add

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

