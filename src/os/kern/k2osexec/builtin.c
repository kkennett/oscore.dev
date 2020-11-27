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

static K2ROFS const *     sgpRofs;
static K2ROFS_DIR const * sgpBuiltinRoot;
static K2ROFS_DIR const * sgpBuiltinKern;

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

void
Builtin_Run(
    void
)
{
    UINT32              ix;
    K2ROFS_FILE const * pFile;
    char const *        pDlxName;
    char const *        pExt;
    char                ch;
    K2OS_TOKEN          tokDlx;

    K2OSKERN_Debug("\n\nLOADING BUILTIN DRIVERS\n");

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
                    tokDlx = K2OS_DlxLoad(NULL, pDlxName, NULL);
                    if (tokDlx == NULL)
                    {
                        K2OSKERN_Debug("  *** FAILED TO LOAD BUILTIN DLX \"%s\". Error %08X\n", pDlxName, K2OS_ThreadGetStatus());
                    }
                    else
                    {
                        K2OSKERN_Debug("Loaded %s token %08X\n", pDlxName, tokDlx);
                    }
                    break;
                }
            }
        } while (ch != 0);
    }
    K2OSKERN_Debug("FINISHED BUILTIN DRIVERS\n");
}

void Builtin_Dispose(K2OSKERN_OBJ_HEADER *apObjHdr)
{
    K2OSKERN_Debug("Builtin_Dispose(%08X)\n", apObjHdr);
    K2_ASSERT(0);
}
