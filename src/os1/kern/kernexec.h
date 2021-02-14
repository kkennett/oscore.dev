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

#ifndef __KERNEXEC_H
#define __KERNEXEC_H

#include <k2oshal.h>
#include <spec/k2rofs.h>

/* --------------------------------------------------------------------------------- */

typedef struct _K2OSKERN_OBJ_NAME           K2OSKERN_OBJ_NAME;
typedef struct _K2OSKERN_OBJ_HEADER         K2OSKERN_OBJ_HEADER;

#define K2OSKERN_OBJ_FLAG_PERMANENT     0x80000000
#define K2OSKERN_OBJ_FLAG_EMBEDDED      0x40000000

typedef void (*K2OSKERN_pf_ObjDispose)(K2OSKERN_OBJ_HEADER *apObj);

struct _K2OSKERN_OBJ_HEADER
{
    K2OS_ObjectType         mObjType;
    UINT32                  mObjFlags;
    INT32 volatile          mRefCount;
    K2OSKERN_pf_ObjDispose  Dispose;

    K2TREE_NODE             ObjTreeNode;
    K2OSKERN_OBJ_NAME *     mpName;
    K2LIST_ANCHOR           WaitEntryPrioList;
};

/* --------------------------------------------------------------------------------- */

typedef
K2STAT
(*K2OSEXEC_pf_OpenDlx)(
    K2OSKERN_OBJ_HEADER *   apPathObj,
    char const *            apRelSpec,
    K2OS_TOKEN *            apRetTokDlxFile,
    UINT32 *                apRetTotalSectors
    );

typedef
K2STAT
(*K2OSEXEC_pf_ReadDlx)(
    K2OS_TOKEN  aTokDlxFile,
    void *      apBuffer,
    UINT32      aStartSector,
    UINT32      aSectorCount
    );

typedef
K2STAT
(*K2OSEXEC_pf_DoneDlx)(
    K2OS_TOKEN  aTokDlxFile
    );

typedef struct _K2OSEXEC_INIT_INFO K2OSEXEC_INIT_INFO;
struct _K2OSEXEC_INIT_INFO
{
    //
    // INPUT
    //
    UINT32                  mEfiMapSize;
    UINT32                  mEfiMemDescSize;
    UINT32                  mEfiMemDescVer;
    K2ROFS const *          mpBuiltinRofs;

    //
    // OUTPUT
    //
    K2OSKERN_IRQ_CONFIG     SysTickDevIrqConfig;
    K2OSEXEC_pf_OpenDlx     OpenDlx;
    K2OSEXEC_pf_ReadDlx     ReadDlx;
    K2OSEXEC_pf_DoneDlx     DoneDlx;
};

typedef
void
(*K2OSEXEC_pf_Init)(
    K2OSEXEC_INIT_INFO * apInitInfo
    );

void
K2OSEXEC_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
    );

typedef
void
(*K2OSEXEC_pf_Run)(
    K2OSHAL_pf_OnSystemReady afReady
    );

void
K2OSEXEC_Run(
    K2OSHAL_pf_OnSystemReady afReady
    );


/* --------------------------------------------------------------------------------- */

#endif // __KERNEXEC_H