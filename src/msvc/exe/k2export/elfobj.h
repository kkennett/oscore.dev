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
#ifndef __ELFOBJ_H
#define __ELFOBJ_H

#include <lib/k2win32.h>
#include <lib/k2mem.h>
#include <lib/k2elf32.h>

/* --------------------------------------------------------------------- */

class ELFObjectFile
{
public:
    static
    K2STAT
    Create(
        K2ReadOnlyMappedFile *  apParentFile,
        char const *            apObjName,
        UINT32                  aObjNameLen,
        UINT8 const *           apObjData,
        UINT32                  aObjDataBytes,
        ELFObjectFile **        appRetFile
    );

    ~ELFObjectFile(void);

    K2ReadOnlyMappedFile const &    ParentFile(void) const { return *mpParentFile; }
    char const *                    ObjName(void) const { return mpObjName; }
    UINT32 const &                  ObjNameLen(void) const { return mObjNameLen; }
    UINT8 const *                   FileDataPtr(void) const { return (UINT8 const *)mpRawHdr; }
    UINT32 const &                  FileDataBytes(void) const { return mFileBytes; }
    bool                            IsA32(void) const { return mpRawHdr->e_machine == EM_A32; }
    bool                            IsX32(void) const { return mpRawHdr->e_machine == EM_X32; }
    bool                            IsSameMachineAs(ELFObjectFile const *apOtherFile) const { return mpRawHdr->e_machine == apOtherFile->FileHeader()->e_machine; }

    UINT32                          EntryPoint(void) const { return mpRawHdr->e_entry; }
    UINT32                          NumSections(void) const { return mpRawHdr->e_shnum; }
    Elf32_Ehdr const *              FileHeader(void) const { return mpRawHdr; }
    Elf32_Shdr const *              SectionHeader(UINT32 aSectionIndex) const { return K2ELF32_GetSectionHeader(&Parse, aSectionIndex); }
    UINT8 const *                   SectionData(UINT32 aSectionIndex) const { return (UINT8 const *)K2ELF32_GetSectionData(&Parse, aSectionIndex); }

private:
    ELFObjectFile(
        K2ReadOnlyMappedFile *  apParentFile,
        char const *            apObjName,
        UINT32                  aObjNameLen,
        Elf32_Ehdr const *      apHdr,
        UINT32                  aFileBytes
    );

    K2STAT Init(void);

    K2ReadOnlyMappedFile *      mpParentFile;
    char const *                mpObjName;
    UINT32                      mObjNameLen;
    Elf32_Ehdr const *          mpRawHdr;
    UINT32                      mFileBytes;
    K2ELF32PARSE                Parse;
};

/* --------------------------------------------------------------------- */

#endif