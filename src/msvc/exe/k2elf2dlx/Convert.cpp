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

#define TARGET_SYMENT_BYTES (sizeof(K2TREE_NODE) + sizeof(char *))

typedef struct _STRENT STRENT;
struct _STRENT
{
    K2TREE_NODE TreeNode;
    UINT32      mSrcOffset;
    UINT32      mDstOffset;
};

#define SECTION_FLAG_PLACED     0x0001
#define SECTION_FLAG_HASOFFSET  0x0002
#define SECTION_FLAG_BSS        0x8000

static bool sTargetSections(void)
{
    UINT32          secIx;
    Elf32_Shdr *    pSrcSecHdr;
    Elf32_Shdr *    pSymSecHdr;
    Elf32_Shdr *    pStrSecHdr;
    Elf32_Rel       reloc;
    Elf32_Rel       reloc2;
    UINT8 *         pRelWork;
    UINT8 *         pSymWork;
    UINT8 *         pSymSrc;
    UINT32          otherSecIx;
    UINT32          relEntBytes;
    UINT32          relEntCount;
    UINT32          symEntBytes;
    UINT32          symEntCount;
    UINT32          strSecIx;
    UINT32          symIx;
    UINT32          symIx2;
    UINT32          relCount2;
    UINT8 *         pRelWork2;

    // collect sections that must be included in the dlx
    // and set their target segments
    for (secIx = 0;secIx < gOut.Parse.mpRawFileData->e_shnum;secIx++)
    {
        pSrcSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, secIx);
        if (pSrcSecHdr->sh_size == 0)
        {
            K2MEM_Zero(pSrcSecHdr, gOut.Parse.mSectionHeaderTableEntryBytes);
            continue;
        }
        if (!(pSrcSecHdr->sh_flags & SHF_ALLOC))
            continue;
        if ((pSrcSecHdr->sh_flags & DLX_SHF_TYPE_MASK) == DLX_SHF_TYPE_IMPORTS)
        {
            gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Reloc;
            gOut.mpSecMap[secIx].mFlags |= SECTION_FLAG_PLACED;
            
            gOut.mppWorkSecData[secIx] = (UINT8 *)K2ELF32_GetSectionData(&gOut.Parse, secIx);
            continue;
        }
        if ((pSrcSecHdr->sh_flags & DLX_SHF_TYPE_MASK) == DLX_SHF_TYPE_EXPORTS)
        {
            gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Read;
            gOut.mpSecMap[secIx].mFlags |= SECTION_FLAG_PLACED;
            gOut.mppWorkSecData[secIx] = (UINT8 *)K2ELF32_GetSectionData(&gOut.Parse, secIx);
            continue;
        }
        if (pSrcSecHdr->sh_flags & DLX_SHF_TYPE_DLXINFO)
        {
            gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Info;
            gOut.mpSecMap[secIx].mFlags |= SECTION_FLAG_PLACED;
            continue;
        }

        switch (pSrcSecHdr->sh_type)
        {
        case SHT_PROGBITS:
        case SHT_A32_EXIDX:
            if (pSrcSecHdr->sh_flags & SHF_EXECINSTR)
                gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Text;
            else if (pSrcSecHdr->sh_flags & SHF_WRITE)
                gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Data;
            else
                gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Read;
            gOut.mpSecMap[secIx].mFlags |= SECTION_FLAG_PLACED;
            gOut.mppWorkSecData[secIx] = (UINT8 *)K2ELF32_GetSectionData(&gOut.Parse, secIx);
            break;
        case SHT_NOBITS:
            gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Data;
            gOut.mpSecMap[secIx].mFlags |= SECTION_FLAG_PLACED | SECTION_FLAG_BSS;
            gOut.mOutBssCount++;
            break;
        default:
            printf("*** Unhandled Alloc section %d type %d flags 0x%08X\n", secIx, pSrcSecHdr->sh_type, pSrcSecHdr->sh_flags);
            return false;
        }
    }

    // now collect relocs and their symbols and symbol stringtables for sections we are putting into the dlx
    for (secIx = 0;secIx < gOut.Parse.mpRawFileData->e_shnum;secIx++)
    {
        pSrcSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, secIx);
        if (pSrcSecHdr->sh_size == 0)
            continue;

        if (pSrcSecHdr->sh_type != SHT_REL)
            continue;

        if (!(gOut.mpSecMap[pSrcSecHdr->sh_info].mFlags & SECTION_FLAG_PLACED))
        {
            // relocations for a section that is not going into the dlx
            K2MEM_Zero(pSrcSecHdr, gOut.Parse.mSectionHeaderTableEntryBytes);
            continue;
        }

        relEntBytes = pSrcSecHdr->sh_entsize;
        if (relEntBytes == 0)
            relEntBytes = sizeof(Elf32_Rel);
        relEntCount = pSrcSecHdr->sh_size / sizeof(Elf32_Rel);
        if (relEntCount == 0)
        {
            // empty relocations section
            K2MEM_Zero(pSrcSecHdr, gOut.Parse.mSectionHeaderTableEntryBytes);
            continue;
        }

        // we need this
        gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Reloc;
        gOut.mpSecMap[secIx].mFlags |= SECTION_FLAG_PLACED;

        // replicate table
        K2_ASSERT(gOut.mppWorkSecData[secIx] == NULL);
        gOut.mppWorkSecData[secIx] = new UINT8[pSrcSecHdr->sh_size];
        if (gOut.mppWorkSecData[secIx] == NULL)
        {
            printf("*** Memory allocation failed\n");
            return false;
        }
        K2MEM_Copy(gOut.mppWorkSecData[secIx], K2ELF32_GetSectionData(&gOut.Parse, secIx), pSrcSecHdr->sh_size);

        // we need the symbol table that this relocations table uses
        otherSecIx = pSrcSecHdr->sh_link;

        if (!(gOut.mpSecMap[otherSecIx].mFlags & SECTION_FLAG_PLACED))
        {
            gOut.mpSecMap[otherSecIx].mSegmentIndex = DlxSeg_Sym;
            gOut.mpSecMap[otherSecIx].mFlags |= SECTION_FLAG_PLACED;

            pSymSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, otherSecIx);

            symEntBytes = pSymSecHdr->sh_entsize;
            if (symEntBytes == 0)
                symEntBytes = sizeof(Elf32_Sym);
            symEntCount = pSymSecHdr->sh_size / symEntBytes;
            if (symEntCount < 2)
            {
                printf("*** Relocations use symbol table with <= null symbol present\n");
                return false;
            }

            // zero out target to accept only symbols actually used
            K2_ASSERT(gOut.mppWorkSecData[otherSecIx] == NULL);
            gOut.mppWorkSecData[otherSecIx] = new UINT8[pSymSecHdr->sh_size];
            if (gOut.mppWorkSecData[otherSecIx] == NULL)
            {
                printf("*** Memory allocation failed\n");
                return false;
            }
            K2MEM_Zero(gOut.mppWorkSecData[otherSecIx], pSymSecHdr->sh_size);
            
            // include symbol table string table and boost size by size of
            // section header string table to account for possible addition of
            // section names as symbol names
            strSecIx = pSymSecHdr->sh_link;
            if ((strSecIx != 0) &&
                (!(gOut.mpSecMap[strSecIx].mFlags & SECTION_FLAG_PLACED)))
            {
                gOut.mpSecMap[strSecIx].mSegmentIndex = DlxSeg_Sym;
                gOut.mpSecMap[strSecIx].mFlags |= SECTION_FLAG_PLACED;

                pStrSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, 
                    gOut.Parse.mpRawFileData->e_shstrndx);
                relCount2 = pStrSecHdr->sh_size;

                pStrSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, strSecIx);
                // zero out target to accept only strings actually used
                K2_ASSERT(gOut.mppWorkSecData[strSecIx] == NULL);
                relCount2 += pStrSecHdr->sh_size;

                gOut.mppWorkSecData[strSecIx] = new UINT8[(relCount2 + 4) & ~3];

                if (gOut.mppWorkSecData[strSecIx] == NULL)
                {
                    printf("*** Memory allocation failed\n");
                    return false;
                }
                K2MEM_Zero(gOut.mppWorkSecData[strSecIx], (pStrSecHdr->sh_size + 4) & ~3);
            }
        }

        // copy over only symbols that the relocations use to the target table
        pRelWork = gOut.mppWorkSecData[secIx];
        pSymWork = gOut.mppWorkSecData[otherSecIx];
        pSymSrc = (UINT8 *)K2ELF32_GetSectionData(&gOut.Parse, otherSecIx);
        do
        {
            K2MEM_Copy(&reloc, pRelWork, sizeof(Elf32_Rel));
            symIx = ELF32_R_SYM(reloc.r_info);
            if (symIx >= symEntCount)
            {
                printf("*** Reloc references symbol index %d when only %d symbols in table\n", symIx, symEntCount);
                return false;
            }
            if (symIx != 0)
            {
                K2MEM_Copy(pSymWork + (symIx * symEntBytes), pSymSrc + (symIx * symEntBytes), symEntBytes);

                // take out other relocations that use this symbol now that we know about it
                relCount2 = relEntCount - 1;
                if (relCount2 != 0)
                {
                    pRelWork2 = pRelWork + relEntBytes;
                    do
                    {
                        K2MEM_Copy(&reloc2, pRelWork2, sizeof(Elf32_Rel));
                        symIx2 = ELF32_R_SYM(reloc2.r_info);
                        if (symIx == symIx2)
                            K2MEM_Zero(pRelWork2, sizeof(Elf32_Rel));
                        pRelWork2 += relEntBytes;
                    } while (--relCount2);
                }
            }
            pRelWork += relEntBytes;
        } while (--relEntCount);

        // put back the relocations that we may have cleared out due to duplicate symbol usage
        K2MEM_Copy(gOut.mppWorkSecData[secIx], K2ELF32_GetSectionData(&gOut.Parse, secIx), pSrcSecHdr->sh_size);
    }

    // include section header strings section
    secIx = gOut.Parse.mpRawFileData->e_shstrndx;
    if (!(gOut.mpSecMap[secIx].mFlags & SECTION_FLAG_PLACED))
    {
        gOut.mpSecMap[secIx].mFlags = SECTION_FLAG_PLACED;
        gOut.mpSecMap[secIx].mSegmentIndex = DlxSeg_Info;
    }

    return true;
}

static bool sPrepNewSections(void)
{
    UINT32                      segIx;
    UINT32                      secIx;
    UINT32                      strLen;
    UINT32                      align;
    Elf32_Shdr *                pSecHdr;
    char const *                pSecHdrStr;
    char const *                pName;
    UINT32                      newSecHdrStrLen;
    UINT32                      outSecHdrOffset;
    DLX_EXPORTS_SECTION const * pExpSec;
    ELFFILE_SECTION_MAPPING *   pOldMap;
    UINT8 **                    pOldData;
    Elf32_Sym                   symEnt;
    UINT8 *                     pWork;
    UINT32                      entCount;
    UINT32                      entBytes;
    IMPENT *                    pImpEnt;
    IMPENT *                    pImpCheck;
    IMPENT *                    pImportsEnd;

    // calc new section headers section length as we go.
    newSecHdrStrLen = 1;
    pSecHdrStr = (char const *)K2ELF32_GetSectionData(&gOut.Parse, gOut.Parse.mpRawFileData->e_shstrndx);
    newSecHdrStrLen = 1;

    // dlx info is always section 1, section header strings always section 2
    gOut.mpRemap[gOut.mSrcInfoSecIx] = 1;
    gOut.mpRevRemap[1] = gOut.mSrcInfoSecIx;
    pSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, gOut.mpRevRemap[1]);
    newSecHdrStrLen += K2ASC_Len(pSecHdrStr + pSecHdr->sh_name) + 1;

    gOut.mpRemap[gOut.Parse.mpRawFileData->e_shstrndx] = 2;
    gOut.mpRevRemap[2] = gOut.Parse.mpRawFileData->e_shstrndx;
    pSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, gOut.mpRevRemap[2]);
    newSecHdrStrLen += K2ASC_Len(pSecHdrStr + pSecHdr->sh_name) + 1;

    gOut.FileAlloc.Segment[DlxSeg_Info].mMemAlign = 4;
    // set index of first target section in segment which matches largest alignment
    // we force dlx info (Section 1) to be first section in segment
    gOut.FileAlloc.Segment[DlxSeg_Info].mMemByteCount = 1;   
    gOut.mOutSecCount = 3;  // null, dlx info section, section header strings

    // count sections in segments and set maximum alignment values for each
    for (segIx = DlxSeg_Text;segIx < DlxSeg_Other;segIx++)
    {
        for (secIx = 0;secIx < gOut.Parse.mpRawFileData->e_shnum;secIx++)
        {
            if (gOut.mpSecMap[secIx].mSegmentIndex != segIx)
                continue;

            pSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, secIx);

            if (pSecHdrStr != NULL)
            {
                pName = pSecHdrStr + pSecHdr->sh_name;
                strLen = K2ASC_Len(pName);
                if (strLen > 0)
                    newSecHdrStrLen += strLen + 1;
            }

            if ((pSecHdr->sh_flags & DLX_SHF_TYPE_MASK) == DLX_SHF_TYPE_IMPORTS)
            {
                gOut.mpImportSecIx[gOut.mImportSecCount] = secIx;
                gOut.mImportSecCount++;
            }

            gOut.mpRemap[secIx] = gOut.mOutSecCount;
            gOut.mpRevRemap[gOut.mOutSecCount] = secIx;

            if (!(gOut.mpSecMap[secIx].mFlags & SECTION_FLAG_BSS))
            {
                align = pSecHdr->sh_addralign;
                if (align == 0)
                    align = 1;

                if (align > gOut.FileAlloc.Segment[segIx].mMemAlign)
                {
                    // this section has the highest alignment requirement
                    // so we store that alignment and the target section that needs it
                    gOut.FileAlloc.Segment[segIx].mMemAlign = align;
                    gOut.FileAlloc.Segment[segIx].mMemByteCount = gOut.mpRemap[secIx];
                }
            }

            gOut.mOutSecCount++;
        }
    }

    newSecHdrStrLen = K2_ROUNDUP(newSecHdrStrLen, 4);
    K2_ASSERT(gOut.mppWorkSecData[gOut.mpRevRemap[2]] == NULL);
    gOut.mppWorkSecData[gOut.mpRevRemap[2]] = new UINT8[newSecHdrStrLen];
    if (gOut.mppWorkSecData[gOut.mpRevRemap[2]] == NULL)
    {
        printf("*** Memory Allocation failed\n");
        return false;
    }
    K2MEM_Zero(gOut.mppWorkSecData[gOut.mpRevRemap[2]], newSecHdrStrLen);

    // allocate and copy over section headers
    // wipe out section names and data offsets
    gOut.mpOutSecHdr = new Elf32_Shdr[gOut.mOutSecCount];
    if (gOut.mpOutSecHdr == NULL)
    {
        printf("*** Memory allocation failed\n");
        return false;
    }
    K2MEM_Zero(gOut.mpOutSecHdr, sizeof(Elf32_Shdr));

    pOldData = gOut.mppWorkSecData;
    gOut.mppWorkSecData = new UINT8 *[gOut.mOutSecCount];
    if (gOut.mppWorkSecData == NULL)
    {
        printf("*** Memory allocation failed\n");
        return false;
    }
    K2MEM_Zero(gOut.mppWorkSecData, sizeof(UINT8 *) * gOut.mOutSecCount);

    outSecHdrOffset = 1;
    for (secIx = 1;secIx < gOut.mOutSecCount;secIx++)
    {
        pSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, gOut.mpRevRemap[secIx]);
        K2MEM_Copy(&gOut.mpOutSecHdr[secIx], pSecHdr, sizeof(Elf32_Shdr));

        pName = pSecHdrStr + pSecHdr->sh_name;
        strLen = K2ASC_Len(pName);
        if (strLen > 0)
        {
            gOut.mpOutSecHdr[secIx].sh_name = outSecHdrOffset;
            K2ASC_Copy((char *)(pOldData[gOut.mpRevRemap[2]] + outSecHdrOffset), pSecHdrStr + pSecHdr->sh_name);
            outSecHdrOffset += strLen + 1;
        }
        else
            gOut.mpOutSecHdr[secIx].sh_name = 0;

        gOut.mpOutSecHdr[secIx].sh_addr = (Elf32_Addr)-1;
        gOut.mpOutSecHdr[secIx].sh_offset = 0;

        gOut.mppWorkSecData[secIx] = pOldData[gOut.mpRevRemap[secIx]];

        if (gOut.mpOutSecHdr[secIx].sh_type == SHT_REL)
        {
            // link is symbol table
            gOut.mpOutSecHdr[secIx].sh_link = gOut.mpRemap[gOut.mpOutSecHdr[secIx].sh_link];
            // info is target section
            gOut.mpOutSecHdr[secIx].sh_info = gOut.mpRemap[gOut.mpOutSecHdr[secIx].sh_info];

            if (gOut.mpOutSecHdr[secIx].sh_entsize == 0)
                gOut.mpOutSecHdr[secIx].sh_entsize = sizeof(Elf32_Rel);
        }
        else if (gOut.mpOutSecHdr[secIx].sh_type == SHT_SYMTAB)
        {
            // link is string table
            if (gOut.mpOutSecHdr[secIx].sh_link != 0)
                gOut.mpOutSecHdr[secIx].sh_link = gOut.mpRemap[gOut.mpOutSecHdr[secIx].sh_link];
            if (gOut.mpOutSecHdr[secIx].sh_info != 0)
                gOut.mpOutSecHdr[secIx].sh_info = 0;

            // process entries and retarget symbol sections
            entBytes = gOut.mpOutSecHdr[secIx].sh_entsize;
            if (entBytes == 0)
                entBytes = gOut.mpOutSecHdr[secIx].sh_entsize = sizeof(Elf32_Sym);

            entCount = gOut.mpOutSecHdr[secIx].sh_size / entBytes;

            pWork = gOut.mppWorkSecData[secIx] + entBytes;
            entCount--;
            do
            {
                K2MEM_Copy(&symEnt, pWork, sizeof(Elf32_Sym));
                if ((symEnt.st_shndx != 0) &&
                    (symEnt.st_shndx < gOut.Parse.mpRawFileData->e_shnum))
                {
                    symEnt.st_shndx = gOut.mpRemap[symEnt.st_shndx];
                    K2MEM_Copy(pWork, &symEnt, sizeof(Elf32_Sym));
                }
                pWork += entBytes;
            } while (--entCount);
        }
        else if ((secIx >= 3) && (gOut.mpOutSecHdr[secIx].sh_type == SHT_STRTAB))
        {
            if (!K2MEM_Verify(gOut.mppWorkSecData[secIx], 0, gOut.mpOutSecHdr[secIx].sh_size))
            {
                printf("*** Internal error - new string table not empty at start\n");
                return false;
            }
            // set size to be null char only
            gOut.mpOutSecHdr[secIx].sh_size = 1;
        }
    }

    delete[] pOldData;

    outSecHdrOffset = K2_ROUNDUP(outSecHdrOffset, 4);
    K2_ASSERT(outSecHdrOffset == newSecHdrStrLen);

    gOut.mpOutSecHdr[1].sh_addralign = 4;
    gOut.mpOutSecHdr[2].sh_addralign = 4;
    gOut.mpOutSecHdr[2].sh_size = newSecHdrStrLen;

    pImportsEnd = NULL;

    gOut.mDlxInfoSize = sizeof(DLX_INFO) - sizeof(UINT32);
    gOut.mDlxInfoSize += gOut.mTargetNameLen + 1;
    gOut.mDlxInfoSize = K2_ROUNDUP(gOut.mDlxInfoSize, 4);
    for (secIx = 0; secIx < gOut.mImportSecCount; secIx++)
    {
        pSecHdr = (Elf32_Shdr *)&gOut.mpOutSecHdr[gOut.mpRemap[gOut.mpImportSecIx[secIx]]];
        pExpSec = (DLX_EXPORTS_SECTION const *)K2ELF32_GetSectionData(&gOut.Parse, gOut.mpImportSecIx[secIx]);
        pName = ((char const *)pExpSec) + sizeof(DLX_EXPORTS_SECTION) - sizeof(DLX_EXPORT_REF);
        // at export[0]
        pName += pExpSec->mCount * sizeof(DLX_EXPORT_REF);
        // at guid
        pName += sizeof(K2_GUID128);

        // get size of name portion
        align = pSecHdr->sh_size - (((UINT32)pName) - ((UINT32)pExpSec));
        align = K2_ROUNDUP(align, 4);
        align += sizeof(IMPENT) - sizeof(UINT32);

        pImpEnt = (IMPENT *)new UINT8[align];
        if (pImpEnt == NULL)
        {
            printf("*** Memory allocation error\n");
            return false;
        }
        K2MEM_Zero(pImpEnt, sizeof(IMPENT));
        pImpEnt->Import.mSizeBytes = align;
        K2MEM_Copy(&pImpEnt->Import.ID, pName - sizeof(K2_GUID128), sizeof(K2_GUID128));
        K2ASC_CopyLen((char *)&pImpEnt->Import.mFileName, pName, align - (sizeof(DLX_IMPORT) - sizeof(UINT32)));
        pImpEnt->mpNext;

        pImpCheck = gOut.mpImportFiles;
        while (pImpCheck != NULL)
        {
            if (!K2MEM_Compare(&pImpCheck->Import.ID, &pImpEnt->Import.ID, sizeof(K2_GUID128)))
            {
                if (!K2ASC_CompIns(pImpCheck->Import.mFileName, pImpEnt->Import.mFileName))
                    break;
            }
            pImpCheck = pImpCheck->mpNext;
        }
        if (pImpCheck == NULL)
        {
            pImpEnt->mIx = gOut.mImportFileCount;
            gOut.mImportFileCount++;
            gOut.mDlxInfoSize += pImpEnt->Import.mSizeBytes;

            if (pImportsEnd == NULL)
                gOut.mpImportFiles = pImpEnt;
            else
                pImportsEnd->mpNext = pImpEnt;
            pImportsEnd = pImpEnt;
            pImpCheck = pImpEnt;
        }
        else
            delete[](UINT8 *)pImpEnt;

        pSecHdr->sh_link = pImpCheck->mIx + 1;  // zero is an invalid index, we use +1
    }

    K2_ASSERT(gOut.mppWorkSecData[1] == NULL);
    gOut.mppWorkSecData[1] = new UINT8[gOut.mDlxInfoSize];
    if (gOut.mppWorkSecData[1] == NULL)
    {
        printf("*** Memory allocation failed\n");
        return false;
    }
    gOut.mpDstInfo = (DLX_INFO *)gOut.mppWorkSecData[1];
    K2MEM_Copy(gOut.mpDstInfo, gOut.mpSrcInfo, sizeof(DLX_INFO));
    gOut.mpDstInfo->mEntryStackReq = gOut.mEntryStackReq;
    K2MEM_Zero(((UINT8 *)gOut.mpDstInfo) + sizeof(DLX_INFO), gOut.mDlxInfoSize - sizeof(DLX_INFO));
    gOut.mpOutSecHdr[1].sh_size = gOut.mDlxInfoSize;
    gOut.mpDstInfo->mImportCount = gOut.mImportFileCount;
    K2ASC_CopyLen(gOut.mpDstInfo->mFileName, gOut.mpTargetName, gOut.mTargetNameLen);
    gOut.mpDstInfo->mFileName[gOut.mTargetNameLen] = 0;
    pWork = ((UINT8 *)gOut.mpDstInfo->mFileName) + gOut.mTargetNameLen + 1;
    pWork = (UINT8 *)K2_ROUNDUP(((UINT32)pWork), 4);
    pImpEnt = gOut.mpImportFiles;
    while (pImpEnt != NULL)
    {
        K2MEM_Copy(pWork, &pImpEnt->Import, pImpEnt->Import.mSizeBytes);
        pWork += pImpEnt->Import.mSizeBytes;
        pImpEnt = pImpEnt->mpNext;
    }
    
    //
    // update section map to be for new set of section headers
    //
    // make the target segment map relative to the new section table
    // and check that we have all the data we need
    pOldMap = gOut.mpSecMap;
    gOut.mpSecMap = new ELFFILE_SECTION_MAPPING[gOut.mOutSecCount];
    if (gOut.mpSecMap == NULL)
    {
        printf("*** Memory allocation failed\n");
        return false;
    }
    K2MEM_Zero(gOut.mpSecMap, sizeof(ELFFILE_SECTION_MAPPING) * gOut.mOutSecCount);
    
    for (secIx = 1; secIx < gOut.mOutSecCount; secIx++)
    {
        gOut.mpSecMap[secIx] = pOldMap[gOut.mpRevRemap[secIx]];
        gOut.FileAlloc.Segment[gOut.mpSecMap[secIx].mSegmentIndex].mSecCount++;
        gOut.mpSecMap[secIx].mOffsetInSegment = 0;
        if (gOut.mppWorkSecData[secIx] == NULL)
        {
            if (gOut.mpOutSecHdr[secIx].sh_type != SHT_NOBITS)
            {
                printf("*** Internal error: no data for target section %d src %d\n",
                    secIx, gOut.mpRevRemap[secIx]);
                return false;
            }
        }
#if 0
        printf("%2d: (Seg %d) flags %04X %08X %s\n",
            secIx,
            gOut.mpSecMap[secIx].mSegmentIndex,
            gOut.mpSecMap[secIx].mFlags,
            (UINT32)gOut.mppWorkSecData[secIx],
            (char *)gOut.mppWorkSecData[2] + gOut.mpOutSecHdr[secIx].sh_name);
#endif
    }

    delete[] pOldMap;

    for (secIx = 0; secIx < gOut.mImportSecCount;secIx++)
        gOut.mpImportSecIx[secIx] = gOut.mpRemap[gOut.mpImportSecIx[secIx]];

    return true;
}

static bool sShrinkSymbolTables(bool aKeepAllGlobalSymbols)
{
    Elf32_Shdr *    pSrcSecHdr;
    Elf32_Shdr *    pOtherSecHdr;
    UINT32          secIx;
    UINT32          srcSymIx;
    UINT32          dstSymIx;
    UINT32 *        pMapSym;
    UINT32          otherSecIx;
    UINT32          entBytes;
    UINT32          entCount;
    Elf32_Sym       symEnt;
    Elf32_Rel       relEnt;
    UINT32          relIx;
    UINT32          relSymIx;
    UINT8 *         pSymData;
    char *          pSymStr;
    bool            takeSym;

    // smush down symbol tables by removing null entries
    for (secIx = 1;secIx < gOut.mOutSecCount; secIx++)
    {
        pSrcSecHdr = &gOut.mpOutSecHdr[secIx];
        if (pSrcSecHdr->sh_size == 0)
            continue;
        if (pSrcSecHdr->sh_type != SHT_SYMTAB)
            continue;

        entBytes = pSrcSecHdr->sh_entsize;
        if (entBytes == 0)
            entBytes = sizeof(Elf32_Sym);
        entCount = pSrcSecHdr->sh_size / entBytes;

        if (aKeepAllGlobalSymbols)
        {
            pSymData = (UINT8 *)K2ELF32_GetSectionData(&gOut.Parse, gOut.mpRevRemap[secIx]);
            for (srcSymIx = 1;srcSymIx < entCount;srcSymIx++)
            {
                if (K2MEM_Verify(gOut.mppWorkSecData[secIx] + (srcSymIx * entBytes), 0, entBytes))
                {
                    // this symbol is not currently being taken
                    K2MEM_Copy(&symEnt, pSymData + (srcSymIx * entBytes), sizeof(Elf32_Sym));
                    if (ELF32_ST_BIND(symEnt.st_info) == STB_GLOBAL)
                    {
                        takeSym = true;
                        if ((symEnt.st_shndx != 0) &&
                            (symEnt.st_shndx < gOut.Parse.mpRawFileData->e_shnum))
                        {
                            symEnt.st_shndx = gOut.mpRemap[symEnt.st_shndx];

                            // if this is NOT a symbol in an import section then take it
                            pOtherSecHdr = &gOut.mpOutSecHdr[symEnt.st_shndx];
                            if ((pOtherSecHdr->sh_flags & DLX_SHF_TYPE_MASK) == DLX_SHF_TYPE_IMPORTS)
                                takeSym = false;
                        }
                        if (takeSym)
                            K2MEM_Copy(gOut.mppWorkSecData[secIx] + (srcSymIx * entBytes), &symEnt, entBytes);
                    }
                }
            }
        }

        pMapSym = new UINT32[entCount];
        if (pMapSym == NULL)
        {
            printf("*** Memory Allocation failed\n");
            return false;
        }
        K2MEM_Set32(pMapSym, (UINT32)-1, sizeof(UINT32) * entCount);

        srcSymIx = 1;
        dstSymIx = 1;

        pSymStr = (char *)K2ELF32_GetSectionData(&gOut.Parse, gOut.mpRevRemap[pSrcSecHdr->sh_link]);

        pSymData = gOut.mppWorkSecData[secIx];
        do
        {
            K2MEM_Copy(&symEnt, pSymData + (srcSymIx * entBytes), sizeof(Elf32_Sym));

            if (!K2MEM_Verify(&symEnt, 0, sizeof(symEnt)))
            {
                if (srcSymIx != dstSymIx)
                {
                    K2MEM_Copy(pSymData + (dstSymIx * entBytes),
                        pSymData + (srcSymIx * entBytes),
                        entBytes);

                    // any relocation sections that use this symbol table
                    // must retarget symbol 'srcSymIx' to 'dstSymIx'
                    pMapSym[srcSymIx] = dstSymIx;
                }
                else
                    pMapSym[srcSymIx] = srcSymIx;
                dstSymIx++;
            }
            srcSymIx++;
        } while (srcSymIx < entCount);
        gOut.mpOutSecHdr[secIx].sh_size = dstSymIx * entBytes;

        for (otherSecIx = 1; otherSecIx < gOut.mOutSecCount; otherSecIx++)
        {
            pOtherSecHdr = &gOut.mpOutSecHdr[otherSecIx];
            if (pOtherSecHdr->sh_size == 0)
                continue;
            if (pOtherSecHdr->sh_type != SHT_REL)
                continue;
            if (!(gOut.mpSecMap[otherSecIx].mFlags & SECTION_FLAG_PLACED))
                continue;
            if (pOtherSecHdr->sh_link != secIx)
                continue;

            // 
            // this relocations table uses the symbol table
            //
            entBytes = pOtherSecHdr->sh_entsize;
            if (entBytes == 0)
                entBytes = sizeof(Elf32_Rel);
            entCount = pOtherSecHdr->sh_size / entBytes;

            for (relIx = 0; relIx < entCount; relIx++)
            {
                K2MEM_Copy(&relEnt, gOut.mppWorkSecData[otherSecIx] + (relIx * entBytes), sizeof(Elf32_Rel));
                relSymIx = ELF32_R_SYM(relEnt.r_info);
                if (pMapSym[relSymIx] == (UINT32)-1)
                {
                    printf("*** Internal error : Relocation uses symbol not included in final symbol table!\n");
                    return false;
                }
                relEnt.r_info = ELF32_MAKE_RELOC_INFO(pMapSym[relSymIx], ELF32_R_TYPE(relEnt.r_info));
                K2MEM_Copy(gOut.mppWorkSecData[otherSecIx] + (relIx * entBytes), &relEnt, sizeof(Elf32_Rel));
            }
        }
        delete[] pMapSym;
        pMapSym = NULL;
    }

    return true;
}

static bool sShrinkStringTables(bool aKeepAllGlobalSymbols)
{
    Elf32_Shdr *    pSymSecHdr;
    Elf32_Shdr *    pStrSecHdr;
    Elf32_Shdr *    pSymTargetSecHdr;
    char const *    pOldStrings;
    char *          pNewStrings;
    UINT32          strSecIx;
    UINT32          symSecIx;
    UINT32          entBytes;
    UINT32          entCount;
    UINT32          newStrOffset;
    UINT8 *         pSymWork;
    Elf32_Sym       symEnt;
    char const *    pSymName;
    K2TREE_ANCHOR   strTree;
    K2TREE_NODE *   pTreeNode;
    STRENT *        pStrEnt;
    bool            keep;
    UINT32          otherSecIx;

    // section 1 is dlxinfo
    // section 2 is section header strings
    // ignore these ones
    for (strSecIx = 3;strSecIx < gOut.mOutSecCount; strSecIx++)
    {
        pStrSecHdr = &gOut.mpOutSecHdr[strSecIx];
        if (pStrSecHdr->sh_type != SHT_STRTAB)
            continue;

        pOldStrings = (char const *)K2ELF32_GetSectionData(&gOut.Parse, gOut.mpRevRemap[strSecIx]);
        pNewStrings = (char *)gOut.mppWorkSecData[strSecIx];

        K2TREE_Init(&strTree, TreeStrCompare);

        // first pass pull all used strings to tree
        for (symSecIx = 3; symSecIx < gOut.mOutSecCount; symSecIx++)
        {
            pSymSecHdr = &gOut.mpOutSecHdr[symSecIx];
            if (pSymSecHdr->sh_type != SHT_SYMTAB)
                continue;
            if (pSymSecHdr->sh_link != strSecIx)
                continue;

            // this symbol table uses the string table
            entBytes = pSymSecHdr->sh_entsize;
            if (entBytes == 0)
                entBytes = sizeof(Elf32_Sym);
            entCount = pSymSecHdr->sh_size / entBytes;

            pSymWork = gOut.mppWorkSecData[symSecIx] + entBytes;
            entCount--;
            do
            {
                K2MEM_Copy(&symEnt, pSymWork, sizeof(Elf32_Sym));

                pSymName = pOldStrings + symEnt.st_name;

                keep = true;

                if (*pSymName == 0)
                {
                    // symbol name is an empty string
                    // does the value match a section address?
                    for (otherSecIx = 1; otherSecIx < gOut.mOutSecCount; otherSecIx++)
                    {
                        pSymTargetSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, gOut.mpRevRemap[otherSecIx]);
                        if (symEnt.st_value == pSymTargetSecHdr->sh_addr)
                        {
                            // name point and offset will be out of range but we don't care here
                            // for the purposes of copying the strings over
                            pSymName = (char const *)gOut.mppWorkSecData[2] + gOut.mpOutSecHdr[otherSecIx].sh_name;
                            symEnt.st_name = (Elf32_Word)pSymName - (Elf32_Word)pOldStrings;
                            K2MEM_Copy(pSymWork, &symEnt, sizeof(Elf32_Sym));
                            break;
                        }
                    }
                    if (otherSecIx == gOut.mOutSecCount)
                        keep = false;
                }
                else if (!aKeepAllGlobalSymbols)
                {
                    // only keep symbols targeting import sections
                    if ((symEnt.st_shndx > 0) &&
                        (symEnt.st_shndx < gOut.mOutSecCount))
                    {
                        pSymTargetSecHdr = &gOut.mpOutSecHdr[symEnt.st_shndx];
                        if (!((pSymTargetSecHdr->sh_flags & DLX_SHF_TYPE_MASK) == DLX_SHF_TYPE_IMPORTS))
                        {
                            keep = false;
                        }
                    }
                }

                if (!keep)
                {
                    symEnt.st_name = 0;
                    K2MEM_Copy(pSymWork, &symEnt, sizeof(Elf32_Sym));
                }
                else
                {
                    pTreeNode = K2TREE_Find(&strTree, (UINT32)pSymName);
                    if (pTreeNode == NULL)
                    {
                        pStrEnt = new STRENT;
                        if (pStrEnt == NULL)
                        {
                            printf("*** Memory allocation failed\n");
                            return false;
                        }
                        pStrEnt->mSrcOffset = symEnt.st_name;
                        pStrEnt->mDstOffset = 0;

                        pStrEnt->TreeNode.mUserVal = (UINT32)pSymName;
                        K2TREE_Insert(&strTree, (UINT32)pSymName, &pStrEnt->TreeNode);
                    }
                }
                pSymWork += entBytes;
            } while (--entCount);
        }

        if (strTree.mNodeCount > 0)
        {
            // copy over strings we are keeping
            newStrOffset = 1;
            pTreeNode = K2TREE_FirstNode(&strTree);
            do
            {
                pStrEnt = K2_GET_CONTAINER(STRENT, pTreeNode, TreeNode);
                pStrEnt->mDstOffset = newStrOffset;
                K2ASC_Copy(pNewStrings + newStrOffset, pOldStrings + pStrEnt->mSrcOffset);
                newStrOffset += K2ASC_Len(pNewStrings + newStrOffset) + 1;
                pTreeNode = K2TREE_NextNode(&strTree, pTreeNode);
            } while (pTreeNode != NULL);

            // resize the new section header
            gOut.mpOutSecHdr[strSecIx].sh_size = newStrOffset;

            // now update symbol tables to point to new strings
            for (symSecIx = 3; symSecIx < gOut.mOutSecCount; symSecIx++)
            {
                pSymSecHdr = &gOut.mpOutSecHdr[symSecIx];
                if (pSymSecHdr->sh_type != SHT_SYMTAB)
                    continue;
                if (pSymSecHdr->sh_link != strSecIx)
                    continue;

                // this symbol table uses the string table
                entBytes = pSymSecHdr->sh_entsize;
                if (entBytes == 0)
                    entBytes = sizeof(Elf32_Sym);
                entCount = pSymSecHdr->sh_size / entBytes;

                pSymWork = gOut.mppWorkSecData[symSecIx] + entBytes;
                entCount--;
                do
                {
                    K2MEM_Copy(&symEnt, pSymWork, sizeof(Elf32_Sym));
                    if (symEnt.st_name != 0)
                    {
                        pSymName = pOldStrings + symEnt.st_name;

                        pTreeNode = K2TREE_Find(&strTree, (UINT32)pSymName);
                        if (pTreeNode == NULL)
                        {
                            printf("*** Internal error - symbol name just added not found\n");
                            return false;
                        }

                        pStrEnt = K2_GET_CONTAINER(STRENT, pTreeNode, TreeNode);
                        symEnt.st_name = pStrEnt->mDstOffset;
                        K2MEM_Copy(pSymWork, &symEnt, sizeof(Elf32_Sym));
                    }
                    pSymWork += entBytes;
                } while (--entCount);
            }

            // we are done so purge the tree
            pTreeNode = K2TREE_FirstNode(&strTree);
            do
            {
                pStrEnt = K2_GET_CONTAINER(STRENT, pTreeNode, TreeNode);
                K2TREE_Remove(&strTree, pTreeNode);
                delete pStrEnt;
                pTreeNode = K2TREE_FirstNode(&strTree);
            } while (pTreeNode != NULL);
        }
        else
        {
            K2_ASSERT(gOut.mpOutSecHdr[strSecIx].sh_size == 1);
        }
    }

    return true;
}

static bool sResizeSymTables(void)
{
    Elf32_Shdr *    pSrcSecHdr;
    UINT32          secIx;
    UINT32          entBytes;
    UINT32          entCount;
    UINT8 *         pNewSymData;
    UINT8 *         pOldSymData;
    UINT32          symIx;

    // make sure symbol table entries are large enough to 
    // permit a tree node + a char pointer to facilitate
    // a symbol lookup tree that uses the tree nodes in the
    // same place as the symbol entries after load
    for (secIx = 3;secIx < gOut.mOutSecCount; secIx++)
    {
        pSrcSecHdr = &gOut.mpOutSecHdr[secIx];
        if (pSrcSecHdr->sh_size == 0)
            continue;
        if (pSrcSecHdr->sh_type != SHT_SYMTAB)
            continue;

        entBytes = pSrcSecHdr->sh_entsize;
        if (entBytes >= TARGET_SYMENT_BYTES)
            continue;

        if (entBytes == 0)
            entBytes = sizeof(Elf32_Sym);

        entCount = pSrcSecHdr->sh_size / entBytes;

        pNewSymData = new UINT8[entCount * TARGET_SYMENT_BYTES];
        if (pNewSymData == NULL)
        {
            printf("*** Memory allocation failed\n");
            return false;
        }
        K2MEM_Zero(pNewSymData, entCount * TARGET_SYMENT_BYTES);

        pOldSymData = gOut.mppWorkSecData[secIx];

        for (symIx = 0; symIx < entCount; symIx++)
        {
            K2MEM_Copy(
                pNewSymData + (symIx * TARGET_SYMENT_BYTES),
                pOldSymData + (symIx * entBytes),
                entBytes);
        }

        pSrcSecHdr->sh_size = (entCount * TARGET_SYMENT_BYTES);
        pSrcSecHdr->sh_entsize = TARGET_SYMENT_BYTES;

        delete[] gOut.mppWorkSecData[secIx];
        gOut.mppWorkSecData[secIx] = pNewSymData;
    }

    return true;
}

static bool sPlaceSegmentSections(void)
{
    UINT32          segIx;
    UINT32          leftToCheck;
    UINT32          secIx;
    UINT32          align;
    Elf32_Shdr *    pSecHdr;
    UINT32          sectionsInSegment;
    UINT32          offsetToTarget;
    UINT32          smallestOffset;
    UINT32          secWithSmallestOffsetToTarget;
    bool            firstBss;
    UINT32          target;
    UINT32          bssCount;

    // take care of case where only section in data segment is bss
    // in this case the output segment alignment will be zero
    // because bss sections are bypassed above in alignment checks
    if (gOut.FileAlloc.Segment[DlxSeg_Data].mSecCount == 1)
    {
        if (gOut.mOutBssCount > 0)
        {
            // only sections in data segment are bss
            leftToCheck = gOut.mOutBssCount;
            for (secIx = 1; secIx < gOut.mOutSecCount; secIx++)
            {
                if (gOut.mpSecMap[secIx].mFlags & SECTION_FLAG_BSS)
                {
                    pSecHdr = &gOut.mpOutSecHdr[secIx];

                    align = pSecHdr->sh_addralign;
                    if (align == 0)
                        align = 1;

                    if (gOut.FileAlloc.Segment[DlxSeg_Data].mMemAlign < align)
                    {
                        gOut.FileAlloc.Segment[DlxSeg_Data].mMemAlign = align;
                        gOut.FileAlloc.Segment[DlxSeg_Data].mMemByteCount = secIx;
                    }

                    if (--leftToCheck == 0)
                        break;
                }
            }
        }
    }

    gOut.FileAlloc.Segment[DlxSeg_Data].mSecCount -= gOut.mOutBssCount;

    for (segIx = DlxSeg_Info; segIx < DlxSeg_Other; segIx++)
    {
        sectionsInSegment = gOut.FileAlloc.Segment[segIx].mSecCount;
        if (sectionsInSegment == 0)
            continue;

        // align must have been set when a section was added to the segment
        K2_ASSERT(gOut.FileAlloc.Segment[segIx].mMemAlign != 0);

        // if we get here there is at least one section in this segment
        // and the alloc already has the largest alignment necessary and
        // the index of that section
        secIx = gOut.FileAlloc.Segment[segIx].mMemByteCount;

        gOut.mpSecMap[secIx].mOffsetInSegment = 0;
        gOut.mpSecMap[secIx].mFlags |= SECTION_FLAG_HASOFFSET;

        pSecHdr = &gOut.mpOutSecHdr[secIx];

        gOut.FileAlloc.Segment[segIx].mMemByteCount = pSecHdr->sh_size;

        // we placed the first section
        if (--sectionsInSegment == 0)
            continue;

        //
        // there are more sections that need to have their offsets in this
        // segment set up
        //
        do {
            leftToCheck = sectionsInSegment;
            smallestOffset = (UINT32)-1;
            secWithSmallestOffsetToTarget = 0;
            for (secIx = 1; secIx < gOut.mOutSecCount; secIx++)
            {
                if (gOut.mpSecMap[secIx].mSegmentIndex != segIx)
                    continue;
                if (gOut.mpSecMap[secIx].mFlags & (SECTION_FLAG_HASOFFSET | SECTION_FLAG_BSS))
                    continue;

                pSecHdr = &gOut.mpOutSecHdr[secIx];

                align = pSecHdr->sh_addralign;
                if (align == 0)
                    align = 1;
                target = gOut.FileAlloc.Segment[segIx].mMemByteCount;
                target = K2_ROUNDUP(target, align);
                offsetToTarget = target - gOut.FileAlloc.Segment[segIx].mMemByteCount;

                if (offsetToTarget < smallestOffset)
                {
                    smallestOffset = offsetToTarget;
                    secWithSmallestOffsetToTarget = secIx;
                }
                if (--leftToCheck == 0)
                    break;
            }

            // okay 'secWithSmallestOffsetToTarget' is the section we place next
            // into this segment
            pSecHdr = &gOut.mpOutSecHdr[secWithSmallestOffsetToTarget];

            align = pSecHdr->sh_addralign;
            if (align == 0)
                align = 1;
            target = gOut.FileAlloc.Segment[segIx].mMemByteCount;
            target = K2_ROUNDUP(target, align);

            gOut.mpSecMap[secWithSmallestOffsetToTarget].mOffsetInSegment = target;
            gOut.mpSecMap[secWithSmallestOffsetToTarget].mFlags |= SECTION_FLAG_HASOFFSET;

            gOut.FileAlloc.Segment[segIx].mMemByteCount = target + pSecHdr->sh_size;

        } while (--sectionsInSegment > 0);
    }

    if (gOut.mOutBssCount == 0)
    {
        gOut.FileAlloc.mEmptyDataOffset = gOut.FileAlloc.Segment[DlxSeg_Data].mMemByteCount;
        return true;
    }

    gOut.FileAlloc.Segment[DlxSeg_Data].mSecCount += gOut.mOutBssCount;

    firstBss = TRUE;
    bssCount = gOut.mOutBssCount;
    do
    {
        leftToCheck = bssCount;
        smallestOffset = (UINT32)-1;
        secWithSmallestOffsetToTarget = 0;

        for (secIx = 1; secIx < gOut.mOutSecCount; secIx++)
        {
            if (gOut.mpSecMap[secIx].mFlags & SECTION_FLAG_HASOFFSET)
                continue;

            pSecHdr = &gOut.mpOutSecHdr[secIx];

            K2_ASSERT((gOut.mpSecMap[secIx].mFlags & SECTION_FLAG_BSS) != 0);

            align = pSecHdr->sh_addralign;
            if (align == 0)
                align = 1;
            target = gOut.FileAlloc.Segment[DlxSeg_Data].mMemByteCount;
            target = K2_ROUNDUP(target, align);
            offsetToTarget = target - gOut.FileAlloc.Segment[DlxSeg_Data].mMemByteCount;

            if (offsetToTarget < smallestOffset)
            {
                smallestOffset = offsetToTarget;
                secWithSmallestOffsetToTarget = secIx;
            }
            if (--leftToCheck == 0)
                break;
        }

        pSecHdr = &gOut.mpOutSecHdr[secWithSmallestOffsetToTarget];

        align = pSecHdr->sh_addralign;
        if (align == 0)
            align = 1;
        target = gOut.FileAlloc.Segment[DlxSeg_Data].mMemByteCount;
        target = K2_ROUNDUP(target, align);

        gOut.mpSecMap[secWithSmallestOffsetToTarget].mOffsetInSegment = target;
        gOut.mpSecMap[secWithSmallestOffsetToTarget].mFlags |= SECTION_FLAG_HASOFFSET;
        if (firstBss)
        {
            gOut.FileAlloc.mEmptyDataOffset = target;
            firstBss = false;
        }
        gOut.FileAlloc.Segment[DlxSeg_Data].mMemByteCount = target + pSecHdr->sh_size;

        gOut.FileAlloc.mEmptyDataBytes = gOut.FileAlloc.Segment[DlxSeg_Data].mMemByteCount - gOut.FileAlloc.mEmptyDataOffset;

        bssCount--;

    } while (bssCount > 0);

    return true;
}

static bool sLayoutFile(void)
{
    UINT32  fileOffset;
    UINT32  memAddr;
    UINT32  segIx;
    UINT32  secIx;

    fileOffset = 0;

    fileOffset += sizeof(Elf32_Ehdr);

    fileOffset += sizeof(Elf32_Shdr) * gOut.mOutSecCount;

    segIx = DlxSeg_Info;

    gOut.mpDstInfo->SegInfo[segIx].mLinkAddr = fileOffset;
    gOut.mpDstInfo->SegInfo[segIx].mMemActualBytes = gOut.FileAlloc.Segment[segIx].mMemByteCount;
    gOut.mpDstInfo->SegInfo[segIx].mFileOffset = fileOffset;
    gOut.mpDstInfo->SegInfo[segIx].mFileBytes = gOut.mpDstInfo->SegInfo[segIx].mMemActualBytes;

    fileOffset += gOut.mpDstInfo->SegInfo[segIx].mMemActualBytes;
    fileOffset = K2_ROUNDUP(fileOffset, DLX_SECTOR_BYTES);
    gOut.mpDstInfo->SegInfo[segIx].mFileBytes = fileOffset - gOut.mpDstInfo->SegInfo[segIx].mFileOffset;

    memAddr = 0x40000000;

    for (segIx = DlxSeg_Info + 1;segIx < DlxSeg_Other; segIx++)
    {
        gOut.mpDstInfo->SegInfo[segIx].mLinkAddr = memAddr;
        gOut.mpDstInfo->SegInfo[segIx].mMemActualBytes = gOut.FileAlloc.Segment[segIx].mMemByteCount;
        gOut.mpDstInfo->SegInfo[segIx].mFileOffset = fileOffset;
        if (segIx == DlxSeg_Data)
            gOut.mpDstInfo->SegInfo[segIx].mFileBytes = gOut.FileAlloc.mEmptyDataOffset;
        else
            gOut.mpDstInfo->SegInfo[segIx].mFileBytes = gOut.mpDstInfo->SegInfo[segIx].mMemActualBytes;

        memAddr += K2_ROUNDUP(gOut.mpDstInfo->SegInfo[segIx].mMemActualBytes, K2_VA32_MEMPAGE_BYTES);

        fileOffset += gOut.mpDstInfo->SegInfo[segIx].mFileBytes;
        fileOffset = K2_ROUNDUP(fileOffset, DLX_SECTOR_BYTES);
        gOut.mpDstInfo->SegInfo[segIx].mFileBytes = fileOffset - gOut.mpDstInfo->SegInfo[segIx].mFileOffset;
    }

    gOut.mOutFileBytes = fileOffset;

    for (secIx = 1; secIx < gOut.mOutSecCount; secIx++)
    {
        if (gOut.mpSecMap[secIx].mSegmentIndex < DlxSeg_Other)
        {
            gOut.mpOutSecHdr[secIx].sh_addr = gOut.mpDstInfo->SegInfo[gOut.mpSecMap[secIx].mSegmentIndex].mLinkAddr +
                gOut.mpSecMap[secIx].mOffsetInSegment;
            K2_ASSERT((gOut.mpOutSecHdr[secIx].sh_addr & (gOut.mpOutSecHdr[secIx].sh_addralign - 1)) == 0);
        }

        if (!(gOut.mpSecMap[secIx].mFlags & SECTION_FLAG_BSS))
        {
            gOut.mpOutSecHdr[secIx].sh_offset = gOut.mpDstInfo->SegInfo[gOut.mpSecMap[secIx].mSegmentIndex].mFileOffset +
                gOut.mpSecMap[secIx].mOffsetInSegment;
        }
    }
   
    return true;
}

int Convert(void)
{
    //
    // build target section pointer array
    //
    gOut.mppWorkSecData = new UINT8 *[gOut.Parse.mpRawFileData->e_shnum];
    if (gOut.mppWorkSecData == NULL)
    {
        printf("*** Memory allocation failed\n");
        return -400;
    }
    K2MEM_Zero(gOut.mppWorkSecData, sizeof(UINT8 *) * gOut.Parse.mpRawFileData->e_shnum);

    //
    // build remap arrays and section luts
    //
    gOut.mpRemap = new UINT32[gOut.Parse.mpRawFileData->e_shnum * 3];
    if (gOut.mpRemap == NULL)
    {
        printf("*** Memory allocation failure\n");
        return -401;
    }
    K2MEM_Zero(gOut.mpRemap, sizeof(UINT32) * gOut.Parse.mpRawFileData->e_shnum * 3);
    gOut.mpRevRemap = gOut.mpRemap + gOut.Parse.mpRawFileData->e_shnum;
    gOut.mpImportSecIx = gOut.mpRevRemap + gOut.Parse.mpRawFileData->e_shnum;

    if (!sTargetSections())
        return -402;

    if (!sPrepNewSections())
        return -403;

    if (!sShrinkSymbolTables(true))
        return -404;

    if (!sShrinkStringTables(true))
        return -405;

    if (!sResizeSymTables())
        return -406;

    if (!sPlaceSegmentSections())
        return -407;

    if (!sLayoutFile())
        return -408;

    return 0;
}
