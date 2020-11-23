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

static K2ROFS_DIR const * sgpBuiltinRoot;
static K2ROFS_DIR const * sgpBuiltinKern;

void 
Builtin_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
)
{
    UINT32              ix;
    K2ROFS_FILE const * pFile;
    char const *        pDlxName;
    char const *        pExt;
    char                ch;
    K2OS_TOKEN          tokDlx;

    sgpBuiltinRoot = K2ROFS_ROOTDIR(apInitInfo->mpBuiltinRofs);
    K2_ASSERT(sgpBuiltinRoot != NULL);

    sgpBuiltinKern = K2ROFSHELP_SubDir(apInitInfo->mpBuiltinRofs, sgpBuiltinRoot, "kern");
    K2_ASSERT(sgpBuiltinKern != NULL);

    for (ix = 0; ix < sgpBuiltinKern->mFileCount; ix++)
    {
        pFile = K2ROFSHELP_SubFileIx(apInitInfo->mpBuiltinRofs, sgpBuiltinKern, ix);
        pDlxName = K2ROFS_NAMESTR(apInitInfo->mpBuiltinRofs, pFile->mName);
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
                    tokDlx = K2OS_DlxLoad(NULL, pDlxName, NULL);
                    if (tokDlx == NULL)
                    {
                        K2OSKERN_Debug("  *** FAILED TO LOAD BUILTIN DLX \"%s\". Error %08X\n", pDlxName, K2OS_ThreadGetStatus());
                    }
                    break;
                }
            }
        } while (ch != 0);
    }
}
