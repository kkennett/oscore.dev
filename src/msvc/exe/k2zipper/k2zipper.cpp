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
#include <lib/k2win32.h>
#include <lib/k2mem.h>
#include <lib/k2zipfile.h>
#include <lib/zlib.h>
#include <lib/k2crc.h>

static
void
K2_CALLCONV_REGS
myAssert(
    char const *    apFile,
    int             aLineNum,
    char const *    apCondition
)
{
    DebugBreak();
}

extern "C" K2_pf_ASSERT K2_Assert = myAssert;

void * intZalloc(void *pOpaque, unsigned int nItems, unsigned int nSize)
{
    return malloc(nItems * nSize);
}

void intZfree(void *pOpaque, void *ptr)
{
    free(ptr);
}

bool sCheckDiff(char const *pPath, char *pPathEnd, K2ZIPFILE_DIR * pWorkDir, bool *apFatal)
{
    UINT                    ix;
    DWORD                   fAttr;
    bool                    retVal;
    FILETIME                localFileTime;
    WORD                    fatDate;
    WORD                    fatTime;
    DWORD                   dosDateTime;
    K2ReadOnlyMappedFile *  pExistingFile;
    bool                    thisFileChanged;
    UINT32                  unCompCrc;
    K2ZIPFILE_DIR *         pSub;
    z_stream                stream;
    int                     zErr;
    K2ZIPFILE_ENTRY         entry;

    retVal = false;
    *apFatal = false;
    dosDateTime = 0;
    unCompCrc = 0;

    for (ix = 0; ix < pWorkDir->mNumEntries; ix++)
    {
        strncpy_s(pPathEnd, _MAX_PATH - 1 - ((UINT32)(pPathEnd - pPath)),
            pWorkDir->mpEntryArray[ix].mpNameOnly, pWorkDir->mpEntryArray[ix].mNameOnlyLen);
        pPathEnd[pWorkDir->mpEntryArray[ix].mNameOnlyLen] = 0;

        fAttr = GetFileAttributes(pPath);
        if ((fAttr == INVALID_FILE_ATTRIBUTES) ||
            (fAttr & FILE_ATTRIBUTE_DIRECTORY))
        {
            printf("k2zipper: could not get attributes of \"%s\". Assuming it's gone.\n", pPath);
            thisFileChanged = true;
            pWorkDir->mpEntryArray[ix].mUserVal = 0xFFFFFFFF;
            continue;
        }

        pExistingFile = K2ReadOnlyMappedFile::Create(pPath);
        if (pExistingFile == NULL)
        {
            printf("k2zipper: could not map in \"%s\"\n", pPath);
            *apFatal = true;
            return true;
        }

        do {
            thisFileChanged = false;

            if (pWorkDir->mpEntryArray[ix].mUncompBytes != pExistingFile->FileBytes())
            {
                printf("k2zipper: different file size \"%s\"\n", pPath);
                thisFileChanged = true;
                break;
            }

            if ((!FileTimeToLocalFileTime(&pExistingFile->FileTime(), &localFileTime)) ||
                (!FileTimeToDosDateTime(&localFileTime, &fatDate, &fatTime)))
            {
                printf("\nk2zipper: could not convert file time of \"%s\"\n\n", pPath);
                thisFileChanged = true;
                *apFatal = true;
                break;
            }

            dosDateTime = (((UINT32)fatDate) << 16) | (((UINT32)fatTime) & 0xFFFF);
            if (pWorkDir->mpEntryArray[ix].mDosDateTime != dosDateTime)
            {
                printf("k2zipper: different file time \"%s\"\n", pPath);
                thisFileChanged = true;
                break;
            }

            unCompCrc = K2CRC_Calc32(0, pExistingFile->DataPtr(), pExistingFile->FileBytes());
            if (pWorkDir->mpEntryArray[ix].mUncompCRC != unCompCrc)
            {
                printf("k2zipper: different content \"%s\"\n", pPath);
                thisFileChanged = true;
                break;
            }

        } while (false);

        if (!(*apFatal))
        {
            if (thisFileChanged)
            {
                // 
                // compress the new file, update the entry, and point to it instead
                //
                retVal = true;

                K2MEM_Copy(&entry, &pWorkDir->mpEntryArray[ix], sizeof(K2ZIPFILE_ENTRY));

                entry.mDosDateTime = dosDateTime;
                entry.mUncompBytes = pExistingFile->FileBytes();
                entry.mUncompCRC = unCompCrc;
                entry.mCompBytes = 0;
                entry.mpCompData = (UINT8 *)malloc(entry.mUncompBytes * 2);

                K2MEM_Zero(&stream, sizeof(stream));

                stream.zalloc = intZalloc;
                stream.zfree = intZfree;
                stream.opaque = NULL;

                zErr = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
                if (zErr != Z_OK)
                {
                    printf("k2zipper: could not initialize compressor for \"%s\"\n", pPath);
                    *apFatal = true;
                }
                else
                {
                    stream.next_out = entry.mpCompData;
                    stream.avail_out = entry.mUncompBytes * 2;
                    stream.next_in = (z_const Bytef *)pExistingFile->DataPtr();
                    stream.avail_in = entry.mUncompBytes;

                    zErr = deflate(&stream, Z_FINISH);
                    if (zErr != Z_STREAM_END)
                    {
                        printf("k2zipper: compressor failed for \"%s\"\n", pPath);
                        *apFatal = true;
                    }
                    else
                    {
                        entry.mCompBytes = stream.total_out;
                        entry.mUserVal = 1;
                        K2MEM_Copy(&pWorkDir->mpEntryArray[ix], &entry, sizeof(K2ZIPFILE_ENTRY));
                    }

                    deflateEnd(&stream);
                }
            }
            else
            {
                //
                // compressed file is the same as the one on disk
                //
                printf(" k2zipper: same \"%s\"\n", pPath);
                pWorkDir->mpEntryArray[ix].mUserVal = 0;
            }
        }

        pExistingFile->Release();

        if (*apFatal)
            return true;
    }

    pSub = pWorkDir->mpSubDirList;
    if (pSub != NULL)
    {
        do {
            strncpy_s(pPathEnd, _MAX_PATH - 1 - ((UINT32)(pPathEnd - pPath)),
                pSub->mpNameOnly, pSub->mNameOnlyLen);
            pPathEnd[pSub->mNameOnlyLen] = 0;

            fAttr = GetFileAttributes(pPath);
            if ((fAttr == INVALID_FILE_ATTRIBUTES) ||
                (!(fAttr & FILE_ATTRIBUTE_DIRECTORY)))
            {
                printf("k2zipper: could not get attributes of dir \"%s\". Assuming it's gone.\n", pPath);
                pSub->mUserVal = 0xFFFFFFFF;
            }
            else
            {
                pPathEnd[pSub->mNameOnlyLen] = '\\';
                pPathEnd[pSub->mNameOnlyLen + 1] = 0;

                thisFileChanged = sCheckDiff(pPath, pPathEnd + pSub->mNameOnlyLen + 1, pSub, apFatal);
                if (*apFatal)
                    return true;

                if (thisFileChanged)
                {
                    pSub->mUserVal = 1;
                    retVal = true;
                }
                else
                    pSub->mUserVal = 0;
            }

            pSub = pSub->mpNextSib;
        } while (pSub != NULL);
    }

    return retVal;
}

bool sCheckForNew(char const *pPath, char *pPathEnd, K2ZIPFILE_DIR * pWorkDir, bool *apFatal)
{
    HANDLE          hFind;
    WIN32_FIND_DATA findData;
    UINT32          nameLen;
    bool            subNew;
    bool            retVal;

    retVal = false;

    strcpy_s(pPathEnd, _MAX_PATH - 1 - ((UINT32)(pPathEnd - pPath)), "*.*");
    pPathEnd[3] = 0;

    hFind = FindFirstFile(pPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return false;
    do {
        nameLen = strlen(findData.cFileName);

        strncpy_s(pPathEnd, _MAX_PATH - 1 - ((UINT32)(pPathEnd - pPath)),
            findData.cFileName, nameLen);
        pPathEnd[nameLen] = 0;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if ((0 != strcmp(findData.cFileName, ".")) &&
                (0 != strcmp(findData.cFileName, "..")))
            {
                pPathEnd[nameLen] = '\\';
                pPathEnd[nameLen + 1] = 0;
                subNew = sCheckForNew(pPath, pPathEnd + nameLen + 1, pWorkDir, apFatal);
                if (subNew)
                    retVal = true;
            }
        }

    } while (FindNextFile(hFind, &findData));

    return retVal;
}

int main(int argc, char **argv)
{
    K2STAT                  stat;
    DWORD                   fAttr;
    char                    dirPath[_MAX_PATH];
    char *                  pEnd;
    bool                    someDiff;
    K2ReadOnlyMappedFile *  pExistingFile;
    K2ZIPFILE_DIR *         pExistingRootDir;
    bool                    fatalError;

    if (argc < 3)
    {
        printf("\nk2zipper: need dir target and existing/target filename\n\n");
        return -1;
    }

    fAttr = GetFileAttributes(argv[1]);
    if ((fAttr == INVALID_FILE_ATTRIBUTES) || ((fAttr & FILE_ATTRIBUTE_DIRECTORY)==0))
    {
        printf("\nk2zipper: first argument is not a directory\n\n");
        return -2;
    }
    strcpy_s(dirPath, _MAX_PATH, argv[1]);
    dirPath[_MAX_PATH - 1] = 0;
    dirPath[_MAX_PATH - 2] = 0;
    pEnd = dirPath;
    while (*pEnd)
        pEnd++;
    pEnd--;
    if ((*pEnd != '/') && (*pEnd != '\\'))
    {
        pEnd++;
        *pEnd = '\\';
        pEnd++;
        *pEnd = 0;
    }
    else
        pEnd++;

    fatalError = false;

    pExistingFile = K2ReadOnlyMappedFile::Create(argv[2]);
    pExistingRootDir = NULL;
    if (pExistingFile != NULL)
    {
        stat = K2ZIPFILE_CreateParse((UINT8 const *)pExistingFile->DataPtr(), pExistingFile->FileBytes(), malloc, free, &pExistingRootDir);
        if (K2STAT_IS_ERROR(stat))
        {
            printf("\nk2zipper: existing target file is not parseable as a ZIP file\n\n");
            return -3;
        }

        someDiff = sCheckDiff(dirPath, pEnd, pExistingRootDir, &fatalError);
    }
    else
        someDiff = true;

    if (fatalError)
    {
        printf("\nk2zipper: fatal error\n");
        if (pExistingFile != NULL)
        {
            if (pExistingRootDir)
                free(pExistingRootDir);
            pExistingFile->Release();
        }
        return -4;
    }

    //
    // now we scan the source dir for new files that weren't there before
    //
    someDiff = sCheckForNew(dirPath, pEnd, pExistingRootDir, &fatalError);

    if (!someDiff)
    {
        printf("\nk2zipper: no diff\n");
        return 0;
    }

//    ghOutFile = CreateFile(argv[2], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    return 0;
}