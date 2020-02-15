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
#ifndef __EXPORT_H
#define __EXPORT_H

#include <stdio.h>
#include <lib/k2win32.h>
#include <lib/k2asc.h>
#include <lib/k2mem.h>
#include <lib/k2parse.h>
#include <lib/k2tree.h>
#include <lib/k2crc.h>
#include <lib/k2elf32.h>

#include <spec/dlx.h>

#include "elfobj.h"

/* --------------------------------------------------------------------- */

typedef struct _GLOBAL_SYMBOL GLOBAL_SYMBOL;
struct _GLOBAL_SYMBOL
{
    ELFObjectFile *     mpObjFile;
    Elf32_Sym const *   mpSymEnt;
    char const *        mpSymName;
    bool                mIsCode;
    bool                mIsRead;
    bool                mIsWeak;
    K2TREE_NODE         TreeNode;
};

typedef struct _EXPORT_SPEC EXPORT_SPEC;
struct _EXPORT_SPEC
{
    char const *    mpName;
    EXPORT_SPEC *   mpNext;
    UINT32          mExpNameOffset; // offset to copy of symbol name in export section
    UINT32          mSymNameOffset; // offset to copy of symbol name in string table
};

typedef struct _EXPSECT EXPSECT;
struct _EXPSECT
{
    UINT32                  mIx;
    UINT32                  mRelocIx;
    UINT32                  mCount;

    EXPORT_SPEC *           mpSpec;

    DLX_EXPORTS_SECTION *   mpExpBase;
    char *                  mpExpStrBase;
    UINT32                  mExpStrBytes;

    UINT32                  mExpSymNameOffset;

    Elf32_Rel *             mpRelocs;
};

#define SECIX_SEC_STR       1
#define SECIX_SYM_STR       2
#define SECIX_SYM           3
#define SECIX_DLXINFO       4
#define SECIX_DLXINFO_RELOC 5

#define OUTSEC_CODE         0
#define OUTSEC_READ         1
#define OUTSEC_DATA         2
#define OUTSEC_COUNT        3

typedef struct _OUTCTX OUTCTX;
struct _OUTCTX
{
    char *                  mpOutputFilePath;

    K2ReadWriteMappedFile * mpMappedDlxInf;

    DLX_INFO                DlxInfo;

    UINT32                  mElfMachine;

    K2TREE_ANCHOR           SymbolTree;

    union 
    {
        UINT_PTR            mRawBase;
        Elf32_Ehdr *        mpFileHdr;
    };

    UINT32                  mRawWork;

    EXPSECT                 mOutSec[OUTSEC_COUNT];
    UINT32                  mTotalExports;
    DLX_INFO *              mpInfo;
    Elf32_Rel *             mpSecRelocBase;
    Elf32_Rel *             mpSecRelocWork;

    UINT32                  mFileSizeBytes;

    UINT8                   mRelocType;

    UINT32                  mSectionCount;

    char *                  mpSecStrBase;
    char *                  mpSecStrWork;
    UINT32                  mSecStrTotalBytes;

    char *                  mpSymStrBase;
    UINT32                  mSymStrTotalBytes;

    Elf32_Shdr *            mpSecHdrs;

    Elf32_Sym *             mpSymBase;
    Elf32_Sym *             mpSymWork;
};

extern OUTCTX gOut;

bool    SetupDlxInfFile(char const *apArgument);
bool    SetupOutputFilePath(char const *apArgument);
bool    AddFilePath(char const *apArgument);
char *  FindOneFile(char const *apFileName);
bool    AddObjectFiles(K2ReadOnlyMappedFile & aSrcFile);
bool    Export_Load(char const *apArgument);
int     Export(void);

/* --------------------------------------------------------------------- */

#endif