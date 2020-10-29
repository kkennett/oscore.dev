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

#include <lib/k2asc.h>
#include <lib/k2crc.h>
#include <lib/k2mem.h>
#include <lib/k2sort.h>
#include <lib/k2zipfile.h>

#define ZIP_MAKEID4(a,b,c,d) ((((UINT32)a)&0xFF) | ((((UINT32)b)&0xFF)<<8) | ((((UINT32)c)&0xFF)<<16) | ((((UINT32)d)&0xFF)<<24))

#define ZIP_RAW_FILEENTRY_MARKER    ZIP_MAKEID4('P','K',3,4)
#define ZIP_FILECDS_MARKER          ZIP_MAKEID4('P','K',1,2)
#define ZIP_FILECDS_TAIL_MARKER     ZIP_MAKEID4('P','K',5,6)

typedef struct _ZIPFILE_RAW_FILEENTRY ZIPFILE_RAW_FILEENTRY;
K2_PACKED_PUSH
struct _ZIPFILE_RAW_FILEENTRY
{
    UINT32  mMarker;
    UINT16  mMinVer;
    UINT16  mFlags;
    UINT16  mCompressionMethod;
    UINT32  mDosDateTime;
    UINT32  mCRC;
    UINT32  mCompSizeBytes;
    UINT32  mUncompSizeBytes;
    UINT16  mFileNameLen;
    UINT16  mExtraLen;
    /* Filename for 'mFileNameLen' */
    /* Extra for 'mExtraLen' */
} K2_PACKED_ATTRIB;
K2_PACKED_POP

typedef struct _ZIP_FILECDS ZIP_FILECDS;
K2_PACKED_PUSH
struct _ZIP_FILECDS
{
    UINT32  mMarker;
    UINT8   mMadeBy;
    UINT8   mHostOS;
    UINT8   mMinVer;
    UINT8   mTargetOS;
    UINT16  mFlags;
    UINT16  mCompressionMethod;
    UINT16  mModFileTime;
    UINT16  mModFileDate;
    UINT32  mCRC;
    UINT32  mCompSizeBytes;
    UINT32  mUncompSizeBytes;
    UINT16  mFileNameLen;
    UINT16  mExtraLen;
    UINT16  mCommentLen;
    UINT16  mDiskNum;
    UINT16  mIntFileAttr;
    UINT32  mExtFileAttr;
    UINT32  mRelHdrOffset;
    /* Filename for 'mFileNameLen' */
    /* Extra for 'mExtraLen' */
    /* Comment for 'mCommentLen' */
} K2_PACKED_ATTRIB;
K2_PACKED_POP

typedef struct _ZIP_FILECDS_TAIL ZIP_FILECDS_TAIL;
K2_PACKED_PUSH
struct _ZIP_FILECDS_TAIL
{
    UINT32  mMarker;
    UINT16  mDiskNum;
    UINT16  mDiskNumCDS;
    UINT16  mNumEntDisk;
    UINT16  mNumEntCDS;
    UINT32  mSizeCDS;
    UINT32  mDiskOffsetCDS;
    UINT16  mCommentLen;
    /* comment for 'mCommentLen' */
} K2_PACKED_ATTRIB;
K2_PACKED_POP

static UINT32 sZIP_EntryBytes(ZIPFILE_RAW_FILEENTRY *apEntry)
{
    return sizeof(ZIPFILE_RAW_FILEENTRY) + apEntry->mCompSizeBytes + apEntry->mFileNameLen + apEntry->mExtraLen;
}

static UINT32 sZIP_CDSBytes(ZIP_FILECDS *apCDS)
{
    return sizeof(ZIP_FILECDS) + apCDS->mFileNameLen + apCDS->mExtraLen + apCDS->mCommentLen;
}

static int sEat_FileEntry(UINT8 const **appSrc, UINT32 *apBytesLeftInZipFile)
{
    ZIPFILE_RAW_FILEENTRY   entry;
    UINT32                  chk;

    K2MEM_Copy(&entry, *appSrc, sizeof(ZIPFILE_RAW_FILEENTRY));
    chk = sZIP_EntryBytes(&entry);
    if (chk > * apBytesLeftInZipFile)
        return K2STAT_ERROR_ZIPFILE_INVALID_FILEENTRY;
    if (entry.mUncompSizeBytes < entry.mCompSizeBytes)
        return K2STAT_ERROR_ZIPFILE_INVALID_FILEENTRY;
    *appSrc += chk;
    *apBytesLeftInZipFile -= chk;
    return 0;
}

static int sEat_CDS(UINT8 const **appSrc, UINT32 *apBytesLeftInZipFile)
{
    ZIP_FILECDS cds;
    UINT32      chk;

    K2MEM_Copy(&cds, *appSrc, sizeof(ZIP_FILECDS));
    chk = sZIP_CDSBytes(&cds);
    if (chk > * apBytesLeftInZipFile)
        return K2STAT_ERROR_ZIPFILE_INVALID_CDS;

    *appSrc += chk;
    *apBytesLeftInZipFile -= chk;

    return 0;
}

static int sEat_CDSTail(UINT8 const **appSrc, UINT32 *apBytesLeftInZipFile)
{
    ZIP_FILECDS_TAIL    cdsTail;
    UINT32              chk;

    K2MEM_Copy(&cdsTail, *appSrc, sizeof(ZIP_FILECDS_TAIL));
    chk = sizeof(cdsTail) + cdsTail.mCommentLen;
    if (chk > * apBytesLeftInZipFile)
        return K2STAT_ERROR_ZIPFILE_INVALID_CDSTAIL;

    *appSrc += chk;
    *apBytesLeftInZipFile -= chk;

    return 0;
}

static 
K2STAT 
sZIP_ValidateFileStructure(UINT8 const *apFileData, UINT32 aFileBytes, UINT32 *apRetFileCount)
{
    int             err;
    UINT8 const *   pWork;
    UINT32          left;
    UINT32          chk;
    UINT32          fileCount;
    UINT32          cdsCount;

    if (apRetFileCount)
        *apRetFileCount = 0;
    fileCount = cdsCount = 0;

    if (aFileBytes < sizeof(UINT32))
        return K2STAT_ERROR_ZIPFILE_INVALID_BAD_FILESIZE;

    pWork = apFileData;
    left = aFileBytes;

    K2MEM_Copy(&chk, pWork, sizeof(UINT32));
    if (chk != ZIP_RAW_FILEENTRY_MARKER)
        return K2STAT_ERROR_ZIPFILE_INVALID_NO_FILE_ENTRIES;

    /* file entries */
    do {
        if (left < sizeof(ZIPFILE_RAW_FILEENTRY))
            return K2STAT_ERROR_ZIPFILE_INVALID_UNEXPECTED_EOF;
        fileCount++;
        err = sEat_FileEntry(&pWork, &left);
        if (err)
            return err;
        if (left < sizeof(ZIP_FILECDS))
            return K2STAT_ERROR_ZIPFILE_INVALID_UNEXPECTED_EOF;
        K2MEM_Copy(&chk, pWork, sizeof(UINT32));
    } while (chk == ZIP_RAW_FILEENTRY_MARKER);

    /* cds headers */
    if (chk != ZIP_FILECDS_MARKER)
        return K2STAT_ERROR_ZIPFILE_INVALID_NO_CDS;
    do {
        if ((left < sizeof(ZIP_FILECDS)) && (left < sizeof(ZIP_FILECDS_TAIL)))
            return K2STAT_ERROR_ZIPFILE_INVALID_UNEXPECTED_EOF;
        K2MEM_Copy(&chk, pWork, sizeof(UINT32));
        if (chk != ZIP_FILECDS_MARKER)
            break;
        cdsCount++;
    } while (!sEat_CDS(&pWork, &left));

    if (fileCount != cdsCount)
        return K2STAT_ERROR_ZIPFILE_INVALID_BAD_FILE_COUNT;

    if (chk != ZIP_FILECDS_TAIL_MARKER)
        return K2STAT_ERROR_ZIPFILE_INVALID_NO_CDS_TAIL;

    err = sEat_CDSTail(&pWork, &left);
    if (err)
        return err;

    if (left)
        return K2STAT_ERROR_ZIPFILE_INVALID_JUNK_AT_EOF;

    if (apRetFileCount)
        *apRetFileCount = fileCount;

    return K2STAT_NO_ERROR;
}

static int sCompStrLens(char const *pStr1, UINT32 strLen1, char const *pStr2, UINT32 strLen2)
{
    int c, v;

    c = ((int)strLen2) - ((int)strLen1);
    if (c > 0)
    {
        /* str2 longer than str1 */
        c = K2ASC_CompInsLen(pStr1, pStr2, strLen1);
        if (!c)
            return -1;
        return c;
    }
    v = K2ASC_CompInsLen(pStr1, pStr2, strLen2);
    if (c < 0)
    {
        /* str2 shorter than str1 name */
        if (!v)
            return 1;
    }
    return v;
}

static int sComparePaths(char const *pPath1, UINT32 path1Len, char const *pPath2, UINT32 path2Len)
{
    char const *pSlash1;
    char const *pSlash2;
    UINT32      left;
    int         c;

    pSlash1 = pPath1;
    left = path1Len;
    do {
        if (*pSlash1 == '/')
            break;
        pSlash1++;
    } while (--left);
    if (!left)
        pSlash1 = NULL;

    pSlash2 = pPath2;
    left = path2Len;
    do {
        if (*pSlash2 == '/')
            break;
        pSlash2++;
    } while (--left);
    if (!left)
        pSlash2 = NULL;

    if ((!pSlash1) && (!pSlash2))
        return sCompStrLens(pPath1, path1Len, pPath2, path2Len);

    if (pSlash1 && (!pSlash2))
        return 1;
    if ((!pSlash1) && (pSlash2))
        return -1;

    /* both have slashes */
    c = sCompStrLens(pPath1, (UINT32)(pSlash1 - pPath1), pPath2, (UINT32)(pSlash2 - pPath2));
    if (c)
        return c;

    /* both have slashes and are the same up to the first slash */
    left = (UINT32)(pSlash1 - pPath1);

    /* both have slashes and start with the same stuff */
    return sComparePaths(pPath1 + left + 1, path1Len - (left + 1), pPath2 + left + 1, path2Len - (left + 1));
}

static int sCompare(void const *p1, void const *p2)
{
    K2ZIPFILE_ENTRY const * pFile1;
    K2ZIPFILE_ENTRY const * pFile2;

    pFile1 = (K2ZIPFILE_ENTRY const *)p1;
    pFile2 = (K2ZIPFILE_ENTRY const *)p2;

    if ((!pFile1->mCompBytes) && (pFile2->mCompBytes))
        return -1;
    if ((pFile1->mCompBytes) && (!pFile2->mCompBytes))
        return 1;
    return sComparePaths(pFile1->mpNameOnly, pFile1->mNameOnlyLen, pFile2->mpNameOnly, pFile2->mNameOnlyLen);
}

static UINT32 sCountDirsNeeded(K2ZIPFILE_ENTRY *apEntries, UINT32 aNumEntries)
{
    char const *        pLook;
    char const *        pComp;
    UINT32              left;
    K2ZIPFILE_ENTRY *   pRest;
    UINT32              ret;
    UINT32              subLen;
    UINT32              numSub;

    /* don't need dir for Entries without a slash at beginning of sorted list */
    do {
        pLook = apEntries->mpNameOnly;
        left = apEntries->mNameOnlyLen;
        do {
            if (*pLook == '/')
                break;
            pLook++;
        } while (--left);
        if (left)
            break;
        apEntries++;
    } while (--aNumEntries);
    if (!aNumEntries)
        return 0;   /* no subdirs at this point */

    pLook++;
    subLen = (UINT32)(pLook - apEntries->mpNameOnly);
    if (aNumEntries == 1)
    {
        apEntries->mpNameOnly += subLen;
        apEntries->mNameOnlyLen -= subLen;
        return 1 + sCountDirsNeeded(apEntries, 1);
    }

    ret = 1;
    pComp = apEntries->mpNameOnly;
    apEntries->mpNameOnly += subLen;
    apEntries->mNameOnlyLen -= subLen;
    pRest = apEntries + 1;
    aNumEntries--;
    numSub = 1;
    do {
        if ((pRest->mNameOnlyLen < subLen) || (K2ASC_CompInsLen(pRest->mpNameOnly, pComp, subLen)))
            break;
        pRest->mpNameOnly += subLen;
        pRest->mNameOnlyLen -= subLen;
        numSub++;
        pRest++;
    } while (--aNumEntries);

    ret += sCountDirsNeeded(apEntries, numSub);

    if (aNumEntries)
        ret += sCountDirsNeeded(pRest, aNumEntries);

    return ret;
}

static void sDirify(K2ZIPFILE_DIR *pThisDir, K2ZIPFILE_ENTRY *apEntries, UINT32 aNumEntries, K2ZIPFILE_DIR **ppFreeDir)
{
    char const *        pLook;
    char const *        pComp;
    UINT32              left;
    K2ZIPFILE_ENTRY *   pRest;
    UINT32              subLen;
    UINT32              numSub;
    K2ZIPFILE_DIR *     pNewDir;
    K2ZIPFILE_DIR *     pPrev;

    /* don't need dir for Entries without a slash at beginning of sorted list */
    pRest = apEntries;
    subLen = 0;
    numSub = aNumEntries;
    do {
        pLook = pRest->mpNameOnly;
        left = pRest->mNameOnlyLen;
        do {
            if (*pLook == '/')
                break;
            pLook++;
        } while (--left);
        if (left)
            break;
        subLen++;
        pRest->mpParentDir = pThisDir;
        pRest++;
    } while (--numSub);

    if (subLen)
    {
        pThisDir->mpEntryArray = apEntries;
        pThisDir->mNumEntries = subLen;
        apEntries = pRest;
        aNumEntries -= subLen;
        if (!numSub)
            return;
    }
    else
    {
        pThisDir->mpEntryArray = NULL;
        pThisDir->mNumEntries = 0;
    }

    /* subdirs found */

    pNewDir = *ppFreeDir;
    (*ppFreeDir)++;

    subLen = (UINT32)(pLook - apEntries->mpNameOnly);

    pNewDir->mpParentDir = pThisDir;
    pNewDir->mpNameOnly = apEntries->mpNameOnly;
    pNewDir->mNameOnlyLen = subLen;
    pNewDir->mpNextSib = NULL;
    pNewDir->mUserVal = 0;
    if (!pThisDir->mpSubDirList)
        pThisDir->mpSubDirList = pNewDir;
    else
    {
        pPrev = pThisDir->mpSubDirList;
        while (pPrev->mpNextSib)
            pPrev = pPrev->mpNextSib;
        pPrev->mpNextSib = pNewDir;
    }

    pLook++;
    subLen++;

    if (aNumEntries == 1)
    {
        apEntries->mpNameOnly += subLen;
        apEntries->mNameOnlyLen -= subLen;
        sDirify(pNewDir, apEntries, 1, ppFreeDir);
        return;
    }

    pComp = apEntries->mpNameOnly;
    apEntries->mpNameOnly += subLen;
    apEntries->mNameOnlyLen -= subLen;
    pRest = apEntries + 1;
    aNumEntries--;
    numSub = 1;
    do {
        if ((pRest->mNameOnlyLen < subLen) || (K2ASC_CompInsLen(pRest->mpNameOnly, pComp, subLen)))
            break;
        pRest->mpNameOnly += subLen;
        pRest->mNameOnlyLen -= subLen;
        numSub++;
        pRest++;
    } while (--aNumEntries);

    sDirify(pNewDir, apEntries, numSub, ppFreeDir);

    if (aNumEntries)
        sDirify(pThisDir, pRest, aNumEntries, ppFreeDir);
}

K2STAT
K2ZIPFILE_CreateParse(
    UINT8 const *       apFileData,
    UINT32              aFileBytes,
    K2ZIPFILE_pf_Alloc  afAlloc,
    K2ZIPFILE_pf_Free   afFree,
    K2ZIPFILE_DIR **    appRetDir
)
{
    UINT8 const *           pScan;
    K2STAT                  stat;
    UINT32                  fileCount;
    UINT8 *                 pAlloc;
    K2ZIPFILE_ENTRY *       pFiles;
    K2ZIPFILE_ENTRY *       pWorkFile;
    K2ZIPFILE_DIR *         pDirs;
    ZIPFILE_RAW_FILEENTRY   entry;
    UINT32                  ix;
    UINT32                  dirCount;
    K2ZIPFILE_DIR *         pThisDir;

    if ((apFileData == NULL) || (aFileBytes==0) || (!appRetDir) || (afAlloc == NULL) || (afFree == NULL))
        return K2STAT_ERROR_BAD_ARGUMENT;
    *appRetDir = NULL;

    fileCount = 0;
    stat = sZIP_ValidateFileStructure(apFileData, aFileBytes, &fileCount);
    if (K2STAT_IS_ERROR(stat))
        return stat;
    if (!fileCount)
        return K2STAT_ERROR_EMPTY;

    ix = sizeof(K2ZIPFILE_ENTRY) * fileCount;
    pAlloc = (UINT8 *)afAlloc(ix);
    if (!pAlloc)
        return K2STAT_ERROR_OUT_OF_MEMORY;
    K2MEM_Zero(pAlloc, ix);
    pFiles = (K2ZIPFILE_ENTRY *)pAlloc;

    pWorkFile = pFiles;
    pScan = apFileData;
    for (ix = 0; ix < fileCount; ix++)
    {
        K2MEM_Copy(&entry, pScan, sizeof(ZIPFILE_RAW_FILEENTRY));
        pWorkFile->mpNameOnly = ((char const *)pScan) + sizeof(ZIPFILE_RAW_FILEENTRY);
        pWorkFile->mNameOnlyLen = entry.mFileNameLen;
        pWorkFile->mCompBytes = entry.mCompSizeBytes;
        pWorkFile->mDosDateTime = entry.mDosDateTime;
        pWorkFile->mUserVal = 0;
        pWorkFile++;
        pScan += sZIP_EntryBytes(&entry);
    }

    K2SORT_Quick(pFiles, fileCount, sizeof(K2ZIPFILE_ENTRY), sCompare);

    /* ignore empty files at the start of the sorted list */
    ix = fileCount;
    while (!pFiles->mCompBytes)
    {
        ix--;
        pFiles++;
        if (!ix)
            break;
    }
    if (!ix)
    {
        afFree(pAlloc);
        return K2STAT_ERROR_EMPTY;
    }

    dirCount = 1 + sCountDirsNeeded(pFiles, ix);

    /* reset and restart with a fresh allocation including the dirs we need */

    afFree(pAlloc);
    ix = (sizeof(K2ZIPFILE_ENTRY) * fileCount) + (sizeof(K2ZIPFILE_DIR) * dirCount);
    pAlloc = (UINT8 *)afAlloc(ix);
    if (!pAlloc)
        return K2STAT_ERROR_OUT_OF_MEMORY;
    K2MEM_Zero(pAlloc, ix);

    pDirs = (K2ZIPFILE_DIR *)pAlloc;
    pFiles = (K2ZIPFILE_ENTRY *)(pAlloc + (sizeof(K2ZIPFILE_DIR) * dirCount));
    pThisDir = pDirs;
    pDirs->mpNameOnly = "";
    pDirs->mNameOnlyLen = 0;
    pDirs++;

    pWorkFile = pFiles;
    pScan = apFileData;
    for (ix = 0; ix < fileCount; ix++)
    {
        K2MEM_Copy(&entry, pScan, sizeof(ZIPFILE_RAW_FILEENTRY));
        pWorkFile->mpNameOnly = ((char const *)pScan) + sizeof(ZIPFILE_RAW_FILEENTRY);
        pWorkFile->mNameOnlyLen = entry.mFileNameLen;
        pWorkFile->mpCompData = ((UINT8 *)pWorkFile->mpNameOnly) + entry.mFileNameLen + entry.mExtraLen;
        pWorkFile->mCompBytes = entry.mCompSizeBytes;
        pWorkFile->mUncompBytes = entry.mUncompSizeBytes;
        pWorkFile->mUncompCRC = entry.mCRC;
        pWorkFile->mDosDateTime = entry.mDosDateTime;
        pWorkFile->mUserVal = 0;
        pWorkFile++;
        pScan += sZIP_EntryBytes(&entry);
    }

    K2SORT_Quick(pFiles, fileCount, sizeof(K2ZIPFILE_ENTRY), sCompare);

    /* ignore empty files at the start of the sorted list */
    while (!pFiles->mCompBytes)
    {
        fileCount--;
        pFiles++;
        if (!fileCount)
            break;
    }

    sDirify(pThisDir, pFiles, fileCount, &pDirs);

    if (pDirs != (pThisDir + dirCount))
    {
        afFree(pAlloc);
        return K2STAT_ERROR_ZIPFILE_INTERNAL;
    }

    *appRetDir = pThisDir;

    return 0;
}

static 
K2ZIPFILE_ENTRY const *
sFindInDir(K2ZIPFILE_DIR const *apDir, char const *apPath)
{
    char const *            pSlash;
    char                    c;
    int                     partLen;
    UINT32                  ix;
    K2ZIPFILE_ENTRY const * pEntry;
    K2ZIPFILE_DIR const *   pSubDir;

    if (!(*apPath))
        return NULL;

    pSlash = apPath;
    do {
        c = *pSlash;
        if ((!c) || (c == '/'))
            break;
        pSlash++;
    } while (1);
    partLen = (int)(pSlash - apPath);
    if (!c)
    {
        pEntry = apDir->mpEntryArray;
        for (ix = 0; ix < apDir->mNumEntries; ix++)
        {
            if (pEntry->mNameOnlyLen == partLen)
            {
                if (!K2ASC_CompInsLen(apPath, apDir->mpEntryArray[ix].mpNameOnly, partLen))
                    break;
            }
            pEntry++;
        }
        if (ix != apDir->mNumEntries)
        {
            return pEntry;
        }
        return NULL;
    }

    /* there is a slash, so this part is a subdirectory */
    pSubDir = apDir->mpSubDirList;
    if (pSubDir)
    {
        do {
            pEntry = sFindInDir(pSubDir, apPath + partLen + 1);
            if (pEntry != NULL)
                return pEntry;
            pSubDir = pSubDir->mpNextSib;
        } while (pSubDir);
    }

    return NULL;
}

K2ZIPFILE_ENTRY const *
K2ZIPFILE_FindEntry(
    K2ZIPFILE_DIR const *   apDir,
    char const *            apFileName
)
{
    if ((!apDir) || (!apFileName))
        return NULL;
    while (*apFileName == '/')
        apFileName++;
    return sFindInDir(apDir, apFileName);
}
