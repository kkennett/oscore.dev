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
#ifndef __ELF2DLX_H
#define __ELF2DLX_H

#include <lib/k2win32.h>
#include <lib/k2asc.h>
#include <lib/k2mem.h>
#include <lib/k2crc.h>
#include <lib/k2tree.h>
#include <lib/k2elf32.h>

#include <spec/dlx.h>

/* --------------------------------------------------------------------- */

typedef struct _ELFFILE_SEGMENT ELFFILE_SEGMENT;
struct _ELFFILE_SEGMENT
{
    UINT32  mMemByteCount;
    UINT32  mMemAlign : 24;
    UINT32  mSecCount : 8;
};

typedef struct _ELFFILE_SECTION_MAPPING ELFFILE_SECTION_MAPPING;
struct _ELFFILE_SECTION_MAPPING
{
    UINT16  mSegmentIndex;
    UINT16  mFlags;
    UINT32  mOffsetInSegment;
};

typedef struct _ELFFILE_ALLOC ELFFILE_ALLOC;
struct _ELFFILE_ALLOC
{
    ELFFILE_SEGMENT Segment[DlxSeg_Count];
    UINT32          mEmptyDataOffset;
    UINT32          mEmptyDataBytes;
};

K2STAT
CalcElfAlloc(
    Elf32_Ehdr const *  apFileHeader,
    UINT8 const *       apSectionHeaderTable,
    ELFFILE_ALLOC *     apRetAlloc
);

typedef struct _IMPENT IMPENT;
struct _IMPENT
{
    IMPENT *    mpNext;
    UINT32      mIx;
    DLX_IMPORT  Import;
};

typedef struct _OUTCTX OUTCTX;
struct _OUTCTX
{
    char *                      mpInputFilePath;
    char *                      mpOutputFilePath;
    char *                      mpImportLibFilePath;

    char *                      mpTargetName;
    UINT32                      mTargetNameLen;

    K2ReadWriteMappedFile *     mpInput;

    // requested stack size for entrypoint
    UINT32                      mEntryStackReq;

    // -k
    bool                        mIsKernelDLX;

    // VerifyLoad sets up
    K2ELF32PARSE                Parse;
    ELFFILE_SECTION_MAPPING *   mpSecMap;
    ELFFILE_ALLOC               FileAlloc;
    DLX_INFO *                  mpSrcInfo;
    UINT32                      mSrcInfoSecIx;
    UINT32                      mTotalExportCount;
    UINT32                      mTotalExportStrSize;
    DLX_EXPORTS_SECTION *       mpExpSec[3];
    UINT32                      mExpSecIx[3];
    
    // Convert
    UINT32 *                    mpRemap;
    UINT32 *                    mpRevRemap;
    UINT32 *                    mpImportSecIx;
    UINT32                      mImportSecCount;
    IMPENT *                    mpImportFiles;
    UINT32                      mImportFileCount;
    UINT32                      mOutSecCount;
    UINT32                      mDlxInfoSize;
    DLX_INFO *                  mpDstInfo;
    UINT32                      mOutBssCount;

    HANDLE                      mhOutput;
    HANDLE                      mhOutLib;
    Elf32_Shdr *                mpOutSecHdr;
    UINT8 **                    mppWorkSecData;
    UINT32                      mOutFileBytes;
};

int TreeStrCompare(UINT32 aKey, K2TREE_NODE * apNode);

extern OUTCTX  gOut;

bool SetupFilePath(char const *apArgument, char **appTarget);
bool SetupEntryStackSize(char const *apArgument, UINT32 *apRetSize);
bool ValidateLoad(void);
int  CreateImportLib(void);
int  Convert(void);
int  CreateTargetDLX(void);
void VerifyAndDump(void);

/* --------------------------------------------------------------------- */

#endif // __ELF2DLX_H