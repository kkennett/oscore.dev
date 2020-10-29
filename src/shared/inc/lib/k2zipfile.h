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
#ifndef __K2ZIPFILE_H
#define __K2ZIPFILE_H

#include <k2systype.h>

//
//------------------------------------------------------------------------
//

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _K2ZIPFILE_ENTRY K2ZIPFILE_ENTRY;
typedef struct _K2ZIPFILE_DIR   K2ZIPFILE_DIR;

struct _K2ZIPFILE_ENTRY
{
    K2ZIPFILE_DIR * mpParentDir;
    char const *    mpNameOnly;
    UINT32          mNameOnlyLen;

    UINT32          mDosDateTime;

    UINT8 *         mpCompData;
    UINT32          mCompBytes;
    UINT32          mUncompBytes;
    UINT32          mUncompCRC;

    UINT32          mUserVal;
};

struct _K2ZIPFILE_DIR
{
    K2ZIPFILE_DIR * mpParentDir;
    char const *    mpNameOnly;
    UINT32          mNameOnlyLen;

    UINT32              mNumEntries;
    K2ZIPFILE_ENTRY *   mpEntryArray;
    K2ZIPFILE_DIR *     mpSubDirList;
    K2ZIPFILE_DIR *     mpNextSib;

    UINT32              mUserVal;
};

typedef void * (*K2ZIPFILE_pf_Alloc)(UINT32 aBytes);
typedef void   (*K2ZIPFILE_pf_Free)(void *apAddr);

K2STAT 
K2ZIPFILE_CreateParse(
    UINT8 const *       apFileData,
    UINT32              aFileBytes,
    K2ZIPFILE_pf_Alloc  afAlloc,
    K2ZIPFILE_pf_Free   afFree,
    K2ZIPFILE_DIR  **   appRetDir
);


K2ZIPFILE_ENTRY const *
K2ZIPFILE_FindEntry(
    K2ZIPFILE_DIR const *   apDir,
    char const *            apFileName
);

#define K2STAT_FACILITY_ZIPFILE                         0x05500000U

#define K2STAT_ERROR_ZIPFILE_INVALID_FILEENTRY          K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 1)
#define K2STAT_ERROR_ZIPFILE_INVALID_CDS                K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 2)
#define K2STAT_ERROR_ZIPFILE_INVALID_CDSTAIL            K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 3)
#define K2STAT_ERROR_ZIPFILE_INVALID_BAD_FILESIZE       K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 4)
#define K2STAT_ERROR_ZIPFILE_INVALID_NO_FILE_ENTRIES    K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 5)
#define K2STAT_ERROR_ZIPFILE_INVALID_UNEXPECTED_EOF     K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 6)
#define K2STAT_ERROR_ZIPFILE_INVALID_NO_CDS             K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 7)
#define K2STAT_ERROR_ZIPFILE_INVALID_NO_CDS_TAIL        K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 8)
#define K2STAT_ERROR_ZIPFILE_INVALID_JUNK_AT_EOF        K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 9)
#define K2STAT_ERROR_ZIPFILE_INVALID_BAD_FILE_COUNT     K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 10)
#define K2STAT_ERROR_ZIPFILE_INTERNAL                   K2STAT_MAKE_ERROR(K2STAT_FACILITY_ZIPFILE, 11)

#ifdef __cplusplus
};  // extern "C"
#endif

//
//------------------------------------------------------------------------
//

#endif  // __K2ZIPFILE_H

