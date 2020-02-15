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
#include "k2export.h"

struct PathTrack
{
    char const *    mpPathName;
    UINT32          mPathLen;
    PathTrack *     mpPathNext;
};

static PathTrack * sgpPathTrack = NULL;

bool 
AddFilePath(
    char const *apFileName
    )
{
    PathTrack *pPath;
    
    pPath = new PathTrack;
    if (pPath == NULL)
    {
        printf("*** Memory allocation error parsing search paths\n");
        return false;
    }

    pPath->mpPathName = apFileName;
    pPath->mPathLen = strlen(apFileName);
    
    while ((pPath->mPathLen != 0) &&
            ((apFileName[pPath->mPathLen-1] == '\\') ||
            (apFileName[pPath->mPathLen-1] == '/')))
            pPath->mPathLen--;

    if (pPath->mPathLen == 0)
    {
        delete pPath;
        printf("*** Invalid search path specified \"%s\"\n", apFileName);
        return false;
    }

    printf("Added Search Path [%.*s]\n", pPath->mPathLen, pPath->mpPathName);
    pPath->mpPathNext = sgpPathTrack;
    sgpPathTrack = pPath;

    return true;
}

char *
FindOneFile(
    char const *apFileName
    )
{
    DWORD       inPathLen;
    char *      pAllocInPath;
    DWORD       fileAttr;
    PathTrack * pPath;
    char *      pSpecPath;
   
    inPathLen = GetFullPathName(apFileName, 0, NULL, NULL);
    pAllocInPath = new char[(inPathLen + 4)&~3];
    if (pAllocInPath == NULL)
    {
        printf("*** Memory allocation failed.\n");
        return NULL;
    }

    if (!GetFullPathName(apFileName, inPathLen + 1, pAllocInPath, NULL))
    {
        // weird
        printf("*** Get full path name of \"%s\" failed\n", apFileName);
        return NULL;
    }

    fileAttr = GetFileAttributes(pAllocInPath);

    if (fileAttr != INVALID_FILE_ATTRIBUTES)
        return pAllocInPath;

    delete[] pAllocInPath;
    pAllocInPath = NULL;

    if ((K2ASC_FindCharConst(':', apFileName)) ||
        (sgpPathTrack == NULL))
        return NULL;

    pPath = sgpPathTrack;
    do {
        DWORD specLen = (inPathLen + pPath->mPathLen + 8) & ~7;
        pSpecPath = new char[specLen];
        if (pSpecPath == NULL)
        {
            printf("*** Memory Allocation Error\n");
            return NULL;
        }

        sprintf_s(pSpecPath, specLen, "%.*s\\%s", pPath->mPathLen, pPath->mpPathName, apFileName);
        inPathLen = GetFullPathName(pSpecPath, 0, NULL, NULL);
        
        pAllocInPath = new char[(inPathLen + 4)&~3];
        if (pAllocInPath == NULL)
        {
            delete[] pSpecPath;
            printf("*** Memory allocation failed.\n");
            return NULL;
        }

        if (!GetFullPathName(pSpecPath, inPathLen + 1, pAllocInPath, NULL))
        {
            // weird
            delete[] pSpecPath;
            delete[] pAllocInPath;
            printf("*** Get full path name of \"%s\" failed\n", apFileName);
            return NULL;
        }

        delete[] pSpecPath;

        fileAttr = GetFileAttributes(pAllocInPath);

        if (fileAttr != INVALID_FILE_ATTRIBUTES)
            return pAllocInPath;

        pPath = pPath->mpPathNext;
    } while (pPath != NULL);

    return NULL;
}