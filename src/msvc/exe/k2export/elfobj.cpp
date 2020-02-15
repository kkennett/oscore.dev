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
#include "elfobj.h"

ELFObjectFile::ELFObjectFile(
    K2ReadOnlyMappedFile *  apParentFile,
    char const *            apObjName,
    UINT32                  aObjNameLen,
    Elf32_Ehdr const *      apHdr,
    UINT32                  aFileBytes
    )
{
    apParentFile->AddRef();
    mpParentFile = apParentFile;

    mpObjName = apObjName;
    mObjNameLen = aObjNameLen;

    mpRawHdr = apHdr;
    mFileBytes = aFileBytes;

    K2MEM_Zero(&Parse, sizeof(Parse));
}

ELFObjectFile::~ELFObjectFile(void)
{
    mpParentFile->Release();
}

K2STAT 
ELFObjectFile::Init(void)
{
    return K2ELF32_Parse((const UINT8 *)mpRawHdr, mFileBytes, &Parse);
}

K2STAT 
ELFObjectFile::Create(
    K2ReadOnlyMappedFile *  apParentFile,
    char const *            apObjName,
    UINT32                  aObjNameLen,
    UINT8 const *           apObjData,
    UINT32                  aObjDataBytes,
    ELFObjectFile **        appRetFile
    )
{
    K2STAT          status;
    ELFObjectFile * pRet;

    pRet = new ELFObjectFile(
        apParentFile, 
        apObjName, 
        aObjNameLen, 
        (Elf32_Ehdr const *)apObjData, 
        aObjDataBytes
        );

    if (pRet == NULL)
        return K2STAT_ERROR_OUT_OF_MEMORY;

    status = pRet->Init();
    if (K2STAT_IS_ERROR(status))
    {
        delete pRet;
        return status;
    }

    *appRetFile = pRet;

    return 0;
}


