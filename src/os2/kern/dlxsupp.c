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

#define MAX_DLX_NAME_LEN    32

//#define K2_STATIC
#define K2_STATIC   static

void KernDlxSupp_AtReInit(DLX *apDlx, UINT32 aModulePageLinkAddr, K2DLXSUPP_HOST_FILE *apInOutHostFile)
{
    //
    // this happens right after dlx_entry for kernel has completed
    //
    if (apDlx == gData.mpShared->LoadInfo.mpDlxCrt)
    {
        // init dlx object for crt - sInitBuiltInDlx(&gData.DlxObjCrt, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpDlxHal)
    {
        // init dlx object for hal - sInitBuiltInDlx(&gData.DlxObjHal, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else if (apDlx == gData.mpDlxKern)
    {
        // init dlx object for krn - sInitBuiltInDlx(&gpProc1->PrimaryModule, apDlx, aModulePageLinkAddr, apInOutHostFile);
    }
    else
    {
        K2_ASSERT(0);
    }
}

UINT32 KernDlx_FindClosestSymbol(K2OSKERN_OBJ_PROCESS *apCurProc, UINT32 aAddr, char *apRetSymName, UINT32 aRetSymNameBufLen)
{
    *apRetSymName = 0;
    return 0;
}

K2STAT KernDlxSupp_CritSec(BOOL aEnter)
{
    return K2STAT_NO_ERROR;
}
