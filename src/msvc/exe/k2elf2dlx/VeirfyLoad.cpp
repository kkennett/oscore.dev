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
#include "elf2dlx.h"

static
bool
sFindGlobalSymbol(
    char const *apSymName,
    bool        aIsData,
    UINT32 *    apRetSymAddr,
    UINT32 *    apRetSymSize
    )
{
    UINT32              secIx;
    UINT32              entSize;
    UINT32              symLeft;
    Elf32_Shdr const *  pSecHdr;
    Elf32_Shdr const *  pStrHdr;
    Elf32_Sym const *   pSym;
    char const *        pStr;
    UINT32              symType;
    UINT32              symBinding;

    *apRetSymAddr = 0;
    *apRetSymSize = 0;

    for (secIx = 1; secIx < gOut.Parse.mpRawFileData->e_shnum;secIx++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&gOut.Parse, secIx);

        if (pSecHdr->sh_type != SHT_SYMTAB)
            continue;

        if ((pSecHdr->sh_link == 0) ||
            (pSecHdr->sh_link >= gOut.Parse.mpRawFileData->e_shnum))
            continue;

        pStrHdr = K2ELF32_GetSectionHeader(&gOut.Parse, pSecHdr->sh_link);

        pStr = ((char const *)gOut.Parse.mpRawFileData) + pStrHdr->sh_offset;

        pSym = (Elf32_Sym const *)(((UINT8 const *)gOut.Parse.mpRawFileData) + pSecHdr->sh_offset);
        entSize = pSecHdr->sh_entsize;
        if (entSize == 0)
            entSize = sizeof(Elf32_Sym);
        symLeft = pSecHdr->sh_size / entSize;
        do
        {
            symBinding = ELF32_ST_BIND(pSym->st_info);
            if (symBinding = STB_GLOBAL)
            {
                symType = ELF32_ST_TYPE(pSym->st_info);
                if (((aIsData) && (symType == STT_OBJECT)) ||
                    ((!aIsData) && (symType == STT_FUNC)))
                {
                    if (!K2ASC_Comp(apSymName, pStr + pSym->st_name))
                    {
                        *apRetSymAddr = pSym->st_value;
                        *apRetSymSize = pSym->st_size;
                        return true;
                    }
                }
            }
            pSym = (Elf32_Sym *)(((UINT8 const *)pSym) + entSize);
        } while (--symLeft);
    }

    return false;
}

static
void *
sLoadAddrToDataPtr(
    UINT32  aLoadAddr,
    UINT32 *apRetSecIx
    )
{
    UINT32              secIx;
    Elf32_Shdr const *  pSecHdr;

    if (apRetSecIx != NULL)
        *apRetSecIx = 0;

    for (secIx = 1; secIx < gOut.Parse.mpRawFileData->e_shnum;secIx++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&gOut.Parse, secIx);
        if ((pSecHdr->sh_flags & SHF_ALLOC) &&
            (pSecHdr->sh_type != SHT_NOBITS))
        {
            if ((aLoadAddr >= pSecHdr->sh_addr) &&
                ((aLoadAddr - pSecHdr->sh_addr) < pSecHdr->sh_size))
            {
                if (apRetSecIx != NULL)
                    *apRetSecIx = secIx;
                return (((UINT8 *)gOut.Parse.mpRawFileData) + pSecHdr->sh_offset) + (aLoadAddr - pSecHdr->sh_addr);
            }
        }
    }

    return NULL;
}

bool ValidateLoad(void)
{
    K2STAT                      status;
    UINT32                       secIx;
    UINT32                      symAddr;
    UINT32                      infoBytes;
    Elf32_Shdr const *          pSecHdr;
    DLX_EXPORTS_SECTION const * pExp;
    UINT32                      ixExpSec;
    UINT32 *                    pSrcPtr;
//    UINT32                      ixExp;
    
    status = K2ELF32_Parse((UINT8 const *)gOut.mpInput->DataPtr(), gOut.mpInput->FileBytes(), &gOut.Parse);
    if (K2STAT_IS_ERROR(status))
    {
        printf("*** Source elf file load failed with error 0x%08X\n", status);
        return false;
    }

    if (gOut.Parse.mpRawFileData->e_type != ET_EXEC)
    {
        printf("*** Source elf file is not executable\n");
        return false;
    }

    if ((gOut.Parse.mpRawFileData->e_shstrndx == 0) ||
        (gOut.Parse.mpRawFileData->e_shstrndx >= gOut.Parse.mpRawFileData->e_shnum))
    {
        printf("*** Source elf has no valid section header strings, which are required\n");
        return false;
    }

    gOut.mpSecMap = new ELFFILE_SECTION_MAPPING[gOut.Parse.mpRawFileData->e_shnum];
    if (gOut.mpSecMap == NULL)
    {
        printf("*** Memory allocation failed\n");
        return false;
    }
    K2MEM_Zero(gOut.mpSecMap, sizeof(ELFFILE_SECTION_MAPPING) * gOut.Parse.mpRawFileData->e_shnum);

    // find DLX info section and validate it
    if (!sFindGlobalSymbol("gpDlxInfo", true, &symAddr, &infoBytes))
    {
        printf("*** Source elf file did not contain DLX information\n");
        return false;
    }
    if (infoBytes != sizeof(DLX_INFO) - sizeof(UINT32))
    {
        printf("*** DLX information in source elf file is wrong size\n");
        return false;
    }
    for (secIx = 1; secIx < gOut.Parse.mpRawFileData->e_shnum;secIx++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&gOut.Parse, secIx);
        if (pSecHdr->sh_flags & SHF_ALLOC)
        {
            if (pSecHdr->sh_addr == symAddr)
                break;
        }
    }
    if (secIx == gOut.Parse.mpRawFileData->e_shnum)
    {
        printf("*** DLX information address does not match a section address\n");
        return false;
    }
    gOut.mSrcInfoSecIx = secIx;
    if (pSecHdr->sh_size != sizeof(DLX_INFO) - sizeof(UINT32))
    {
        printf("*** DLX information section is the wrong size\n");
        return false;
    }
    gOut.mpSrcInfo = (DLX_INFO *)sLoadAddrToDataPtr(symAddr, NULL);

    gOut.mTotalExportStrSize = 0;
    pSrcPtr = (UINT32 *)&gOut.mpSrcInfo->mpExpCode;
    for (ixExpSec = 0;ixExpSec < 3; ixExpSec++)
    {
        if (pSrcPtr[ixExpSec] != NULL)
        {
            pExp = gOut.mpExpSec[ixExpSec] = 
                (DLX_EXPORTS_SECTION *)sLoadAddrToDataPtr(pSrcPtr[ixExpSec], &gOut.mExpSecIx[ixExpSec]);
            if (pExp == NULL)
            {
                printf("*** Exports type %d could not be found in loaded file\n", ixExpSec);
                return false;
            }
            pSecHdr = K2ELF32_GetSectionHeader(&gOut.Parse, gOut.mExpSecIx[ixExpSec]);
//            printf("%d Exports type %d\n", pExp->mCount, ixExpSec);
//            for (ixExp = 0; ixExp < pExp->mCount;ixExp++)
//            {
//                printf("    %08X %s\n", pExp->Export[ixExp].mAddr, ((char *)pExp) + pExp->Export[ixExp].mNameOffset);
//            }
            gOut.mTotalExportCount += pExp->mCount;
            gOut.mTotalExportStrSize += pSecHdr->sh_size -
                (sizeof(DLX_EXPORTS_SECTION) + (sizeof(DLX_EXPORT_REF) * (pExp->mCount - 1)));
        }
    }

    return true;
}