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
#include "k2export.h"

static char const * const sgpSecStr_SecStr = ".sechdrstr";
static char const * const sgpSecStr_SymStr = ".symstr";
static char const * const sgpSecStr_Sym = ".symtable";
static char const * const sgpSecStr_DLXInfo = ".dlxinfo";
static char const * const sgpSecStr_DLXInfoReloc = ".dlxinfo.rel";
static char const * const sgpSecStr_Exp = ".?export";
static char const * const sgpSecStr_Rel = ".?export.rel";

static char const * const sgpSymExp = "dlx_?export";
static char const * const sgpSymInfo = "gpDlxInfo";

static
void
sPrepSection(
    UINT32 aIx
    )
{
    UINT32          work;
    UINT32          startOffset;
    UINT32          len;
    EXPORT_SPEC *   pSpec;

    if (gOut.mOutSec[aIx].mCount == 0)
        return;

    gOut.mFileSizeBytes += gOut.mOutSec[aIx].mCount * sizeof(Elf32_Sym);

    gOut.mSecStrTotalBytes += K2ASC_Len(sgpSecStr_Exp) + 1;
    gOut.mSecStrTotalBytes += K2ASC_Len(sgpSecStr_Rel) + 1;

    gOut.mOutSec[aIx].mIx = gOut.mSectionCount;
    gOut.mFileSizeBytes += sizeof(Elf32_Shdr);
    gOut.mSectionCount++;

    // data in export section
    startOffset = gOut.mFileSizeBytes;
    gOut.mFileSizeBytes += sizeof(DLX_EXPORTS_SECTION) - sizeof(DLX_EXPORT_REF);
    gOut.mFileSizeBytes += gOut.mOutSec[aIx].mCount * sizeof(DLX_EXPORT_REF);
    startOffset = gOut.mFileSizeBytes - startOffset;
    work = 0;
    pSpec = gOut.mOutSec[aIx].mpSpec;
    while (pSpec != NULL)
    {
        pSpec->mExpNameOffset = work + startOffset;
        pSpec->mSymNameOffset = gOut.mSymStrTotalBytes;

        len = K2ASC_Len(pSpec->mpName) + 1;
        work += len;
        gOut.mSymStrTotalBytes += len;
        pSpec = pSpec->mpNext;
    }
    gOut.mSymStrTotalBytes += work;

    // section data symbol name
    gOut.mOutSec[aIx].mExpSymNameOffset = gOut.mSymStrTotalBytes;
    gOut.mSymStrTotalBytes += K2ASC_Len(sgpSymExp) + 1;

    // align
    gOut.mOutSec[aIx].mExpStrBytes = K2_ROUNDUP(work, 4);
    gOut.mFileSizeBytes += gOut.mOutSec[aIx].mExpStrBytes;
        
    // reloc section
    gOut.mOutSec[aIx].mRelocIx = gOut.mSectionCount;
    gOut.mFileSizeBytes += sizeof(Elf32_Shdr);
    gOut.mSectionCount++;

    // data in reloc section
    gOut.mFileSizeBytes += sizeof(Elf32_Rel) * gOut.mOutSec[aIx].mCount;

    // symbol for export section
    gOut.mFileSizeBytes += sizeof(Elf32_Sym);

    // reloc for export section (for pointer in info section)
    gOut.mFileSizeBytes += sizeof(Elf32_Rel);
}

static
void
sPlaceSection(
    UINT32 aIx
    )
{
    UINT32 secBase;

    if (gOut.mOutSec[aIx].mCount == 0)
        return;

    // exports
    secBase = gOut.mRawWork;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_offset = gOut.mRawWork - gOut.mRawBase;

    gOut.mOutSec[aIx].mpExpBase = (DLX_EXPORTS_SECTION *)gOut.mRawWork;
    gOut.mRawWork += sizeof(DLX_EXPORTS_SECTION) - sizeof(DLX_EXPORT_REF);
    gOut.mRawWork += gOut.mOutSec[aIx].mCount * sizeof(DLX_EXPORT_REF);

    gOut.mOutSec[aIx].mpExpStrBase = (char *)gOut.mRawWork;
    gOut.mRawWork += gOut.mOutSec[aIx].mExpStrBytes;

    gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_size = gOut.mRawWork - secBase;

    // relocs
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_offset = gOut.mRawWork - gOut.mRawBase;
    gOut.mOutSec[aIx].mpRelocs = (Elf32_Rel *)gOut.mRawWork;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_size = sizeof(Elf32_Rel) * gOut.mOutSec[aIx].mCount;
    gOut.mRawWork += gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_size;
}

static
void
sEmitSection(
    UINT32   aIx
    )
{
    Elf32_Rel *         pReloc;
    DLX_EXPORT_REF *    pRef;
    EXPORT_SPEC *       pSpec;
    UINT32              symIx;
    UINT8               binding;
    char *              pExpSecSymName;

    if (gOut.mOutSec[aIx].mCount == 0)
        return;

    gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_name = (Elf32_Word)(gOut.mpSecStrWork - gOut.mpSecStrBase);
    K2ASC_Copy(gOut.mpSecStrWork, sgpSecStr_Exp);
    if (aIx == OUTSEC_CODE)
    {
        binding = ELF32_MAKE_SYMBOL_INFO(STB_GLOBAL, STT_FUNC);
        *(gOut.mpSecStrWork + 1) = 'c';
    }
    else
    {
        binding = ELF32_MAKE_SYMBOL_INFO(STB_GLOBAL, STT_OBJECT);
        if (aIx == OUTSEC_READ)
            *(gOut.mpSecStrWork + 1) = 'r';
        else
            *(gOut.mpSecStrWork + 1) = 'd';
    }
    gOut.mpSecStrWork += K2ASC_Len(sgpSecStr_Exp) + 1;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_type = SHT_PROGBITS;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_flags = SHF_ALLOC | DLX_SHF_TYPE_EXPORTS;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_addr = (Elf32_Addr)gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_offset;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_addralign = 4;

    gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_name = (Elf32_Word)(gOut.mpSecStrWork - gOut.mpSecStrBase);
    K2ASC_Copy(gOut.mpSecStrWork, sgpSecStr_Rel);
    if (aIx == OUTSEC_CODE)
        *(gOut.mpSecStrWork + 1) = 'c';
    else if (aIx == OUTSEC_READ)
        *(gOut.mpSecStrWork + 1) = 'r';
    else
        *(gOut.mpSecStrWork + 1) = 'd';
    gOut.mpSecStrWork += K2ASC_Len(sgpSecStr_Rel) + 1;

    gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_type = SHT_REL;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_addralign = 4;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_link = SECIX_SYM;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_info = gOut.mOutSec[aIx].mIx;
    gOut.mpSecHdrs[gOut.mOutSec[aIx].mRelocIx].sh_entsize = sizeof(Elf32_Rel);

    pReloc = gOut.mOutSec[aIx].mpRelocs;
    pRef = &gOut.mOutSec[aIx].mpExpBase->Export[0];
    pSpec = gOut.mOutSec[aIx].mpSpec;
    while (pSpec != NULL)
    {
        K2ASC_Copy(gOut.mpSymStrBase + pSpec->mSymNameOffset, pSpec->mpName);
        K2ASC_Copy(((char *)gOut.mOutSec[aIx].mpExpBase) + pSpec->mExpNameOffset, pSpec->mpName);

        pRef->mNameOffset = pSpec->mExpNameOffset;

        symIx = (UINT32)(gOut.mpSymWork - gOut.mpSymBase);
        gOut.mpSymWork->st_name = pSpec->mSymNameOffset;
        gOut.mpSymWork->st_shndx = SHN_UNDEF;
        gOut.mpSymWork->st_size = sizeof(void *);
        gOut.mpSymWork->st_value = 0;
        gOut.mpSymWork->st_info = binding;
                
        pReloc->r_info = ELF32_MAKE_RELOC_INFO(symIx, gOut.mRelocType);
        pReloc->r_offset = ((UINT32)&pRef->mAddr) - ((UINT32)gOut.mOutSec[aIx].mpExpBase);

        pRef++;
        gOut.mpSymWork++;
        pReloc++;
        pSpec = pSpec->mpNext;
    }
    gOut.mOutSec[aIx].mpExpBase->mCount = gOut.mOutSec[aIx].mCount;
    gOut.mOutSec[aIx].mpExpBase->mCRC32 = K2CRC_Calc32(0, gOut.mOutSec[aIx].mpExpStrBase, gOut.mOutSec[aIx].mExpStrBytes);

    // name of symbol for export section
    pExpSecSymName = gOut.mpSymStrBase + gOut.mOutSec[aIx].mExpSymNameOffset;
    K2ASC_Copy(pExpSecSymName, sgpSymExp);
    if (aIx == OUTSEC_CODE)
        *(pExpSecSymName + 4) = 'c';
    else if (aIx == OUTSEC_READ)
        *(pExpSecSymName + 4) = 'r';
    else
        *(pExpSecSymName + 4) = 'd';

    // symbol for export section
    symIx = (UINT32)(gOut.mpSymWork - gOut.mpSymBase);
    gOut.mpSymWork->st_name = gOut.mOutSec[aIx].mExpSymNameOffset;
    gOut.mpSymWork->st_shndx = gOut.mOutSec[aIx].mIx;
    gOut.mpSymWork->st_size = gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_size;
    // object file (REL, not EXEC), so symbol values are section relative, not absolue
    gOut.mpSymWork->st_value = 0; // gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_addr;
    gOut.mpSymWork->st_info = ELF32_MAKE_SYMBOL_INFO(STB_GLOBAL, STT_OBJECT);
    gOut.mpSymWork++;

    // reloc for export section to pointer in dlx info
    gOut.mpSecRelocWork->r_info = ELF32_MAKE_RELOC_INFO(symIx, gOut.mRelocType);
    gOut.mpSecRelocWork->r_offset = K2_FIELDOFFSET(DLX_INFO, mpExpCode) + (aIx * sizeof(void *));
    gOut.mpSecRelocWork++;

    // object file, so reloc target does not hold file link address
//    DLX_EXPORTS_SECTION ** ppExpSec;
//    ppExpSec = &gOut.mpInfo->mpExpCode;
//    ppExpSec[aIx] = (DLX_EXPORTS_SECTION *)gOut.mpSecHdrs[gOut.mOutSec[aIx].mIx].sh_offset;
}

static
bool
sCreateOutputFile(
    void
    )
{
    bool            ret;
    UINT32          ix;
    K2ELF32PARSE    elfFile;
    HANDLE          hOutFile;

    if (gOut.mElfMachine == EM_A32)
        gOut.mRelocType = R_ARM_ABS32;
    else if (gOut.mElfMachine == EM_X32)
        gOut.mRelocType = R_386_32;
    else
    {
        printf("*** Output is for an unknown machine type\n");
    }

    // file header
    gOut.mFileSizeBytes = sizeof(Elf32_Ehdr);

    // null section header 0
    gOut.mFileSizeBytes += sizeof(Elf32_Shdr);
    gOut.mSectionCount = 1;

    // section strings header 1
    gOut.mFileSizeBytes += sizeof(Elf32_Shdr);
    gOut.mSectionCount++;
   
    gOut.mSecStrTotalBytes =
        1 +
        K2ASC_Len(sgpSecStr_SecStr) + 1 +
        K2ASC_Len(sgpSecStr_SymStr) + 1 +
        K2ASC_Len(sgpSecStr_Sym) + 1 +
        K2ASC_Len(sgpSecStr_DLXInfo) + 1;

    // symbol strings section 2
    gOut.mFileSizeBytes += sizeof(Elf32_Shdr);
    gOut.mSectionCount++;
    gOut.mSymStrTotalBytes = 1; // 0th byte must be null

    // symbol table section 3
    gOut.mFileSizeBytes += sizeof(Elf32_Shdr);
    gOut.mSectionCount++;
    gOut.mFileSizeBytes += sizeof(Elf32_Sym); // 0th symbol is always blank

    // info section 4
    gOut.mFileSizeBytes += sizeof(Elf32_Shdr);
    gOut.mSectionCount++;
    gOut.mFileSizeBytes += sizeof(DLX_INFO) - sizeof(UINT32);
    gOut.mFileSizeBytes += sizeof(Elf32_Sym); // section symbol
    gOut.mSymStrTotalBytes += K2ASC_Len(sgpSymInfo) + 1; 

    if (gOut.mTotalExports > 0)
    {
        // info relocs section 5
        gOut.mFileSizeBytes += sizeof(Elf32_Shdr);
        gOut.mSectionCount++;
        gOut.mSecStrTotalBytes += K2ASC_Len(sgpSecStr_DLXInfoReloc) + 1;

        // prep for export sections now
        for (ix = 0;ix < OUTSEC_COUNT;ix++)
            sPrepSection(ix);
    }

    gOut.mSymStrTotalBytes = K2_ROUNDUP(gOut.mSymStrTotalBytes, 4);
    gOut.mFileSizeBytes += gOut.mSymStrTotalBytes;

    gOut.mSecStrTotalBytes = K2_ROUNDUP(gOut.mSecStrTotalBytes, 4);
    gOut.mFileSizeBytes += gOut.mSecStrTotalBytes;

    gOut.mRawBase = (UINT32)new UINT8[gOut.mFileSizeBytes];
    if (gOut.mRawBase == 0)
    {
        printf("*** Memory allocation failed\n");
        return false;
    }
    
    ret = false;
    do
    {
        K2MEM_Zero((UINT8 *)gOut.mRawBase, gOut.mFileSizeBytes);

        gOut.mRawWork = gOut.mRawBase;
        gOut.mRawWork += sizeof(Elf32_Ehdr);

        gOut.mpSecHdrs = (Elf32_Shdr *)gOut.mRawWork;
        gOut.mpFileHdr->e_shoff = gOut.mRawWork - gOut.mRawBase;
        gOut.mRawWork += sizeof(Elf32_Shdr) * gOut.mSectionCount;

        gOut.mpSecHdrs[SECIX_SEC_STR].sh_offset = gOut.mRawWork - gOut.mRawBase;
        gOut.mpSecHdrs[SECIX_SEC_STR].sh_size = gOut.mSecStrTotalBytes;
        gOut.mpSecStrBase = (char *)gOut.mRawWork;
        gOut.mpSecStrWork = gOut.mpSecStrBase + 1;
        gOut.mRawWork += gOut.mpSecHdrs[SECIX_SEC_STR].sh_size;

        gOut.mpSecHdrs[SECIX_SYM_STR].sh_offset = gOut.mRawWork - gOut.mRawBase;
        gOut.mpSecHdrs[SECIX_SYM_STR].sh_size = gOut.mSymStrTotalBytes;
        gOut.mpSymStrBase = (char *)gOut.mRawWork;
        gOut.mRawWork += gOut.mpSecHdrs[SECIX_SYM_STR].sh_size;

        gOut.mpSecHdrs[SECIX_SYM].sh_offset = gOut.mRawWork - gOut.mRawBase;
        gOut.mpSecHdrs[SECIX_SYM].sh_size = sizeof(Elf32_Sym) * 2;  // null symbol and dlx info symbol
        for (ix = 0;ix < OUTSEC_COUNT;ix++)
        {
            if (gOut.mOutSec[ix].mCount != 0)
            {
                gOut.mpSecHdrs[SECIX_SYM].sh_size += (1 + gOut.mOutSec[ix].mCount) * sizeof(Elf32_Sym);
            }
        }
        gOut.mpSymBase = (Elf32_Sym *)gOut.mRawWork;
        gOut.mpSymWork = gOut.mpSymBase + 2;    // null symbol and dlx info symbol
        gOut.mRawWork += gOut.mpSecHdrs[SECIX_SYM].sh_size;

        gOut.mpSecHdrs[SECIX_DLXINFO].sh_offset = gOut.mRawWork - gOut.mRawBase;
        gOut.mpSecHdrs[SECIX_DLXINFO].sh_addr = (Elf32_Addr)gOut.mpSecHdrs[SECIX_DLXINFO].sh_offset;
        gOut.mpSecHdrs[SECIX_DLXINFO].sh_size = sizeof(DLX_INFO) - sizeof(UINT32);
        gOut.mpInfo = (DLX_INFO *)gOut.mRawWork;
        gOut.mRawWork += gOut.mpSecHdrs[SECIX_DLXINFO].sh_size;

        if (gOut.mTotalExports > 0)
        {
            gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_offset = gOut.mRawWork - gOut.mRawBase;
            gOut.mpSecRelocBase = (Elf32_Rel *)gOut.mRawWork;
            gOut.mpSecRelocWork = gOut.mpSecRelocBase;
            for (ix = 0;ix < OUTSEC_COUNT;ix++)
            {
                if (gOut.mOutSec[ix].mCount > 0)
                    gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_size += sizeof(Elf32_Rel);
            }
            gOut.mRawWork += gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_size;

            for (ix = 0;ix < OUTSEC_COUNT;ix++)
                sPlaceSection(ix);
        }

        if ((gOut.mRawWork - gOut.mRawBase) != gOut.mFileSizeBytes)
        {
            printf("*** Internal error creating export\n");
            return false;
        }

        gOut.mpFileHdr->e_ident[EI_MAG0] = ELFMAG0;
        gOut.mpFileHdr->e_ident[EI_MAG1] = ELFMAG1;
        gOut.mpFileHdr->e_ident[EI_MAG2] = ELFMAG2;
        gOut.mpFileHdr->e_ident[EI_MAG3] = ELFMAG3;
        gOut.mpFileHdr->e_ident[EI_CLASS] = ELFCLASS32;
        gOut.mpFileHdr->e_ident[EI_DATA] = ELFDATA2LSB;
        gOut.mpFileHdr->e_ident[EI_VERSION] = EV_CURRENT;
        gOut.mpFileHdr->e_type = ET_REL;
        gOut.mpFileHdr->e_machine = gOut.mElfMachine;
        gOut.mpFileHdr->e_version = EV_CURRENT;
        if (gOut.mElfMachine == EM_A32)
            gOut.mpFileHdr->e_flags = EF_A32_ABIVER;
        gOut.mpFileHdr->e_ehsize = sizeof(Elf32_Ehdr);
        gOut.mpFileHdr->e_shentsize = sizeof(Elf32_Shdr);
        gOut.mpFileHdr->e_shnum = gOut.mSectionCount;
        gOut.mpFileHdr->e_shstrndx = SECIX_SEC_STR;

        // section header strings
        gOut.mpSecHdrs[SECIX_SEC_STR].sh_name = ((UINT32)gOut.mpSecStrWork) - ((UINT32)gOut.mpSecStrBase);
        K2ASC_Copy(gOut.mpSecStrWork, sgpSecStr_SecStr);
        gOut.mpSecStrWork += K2ASC_Len(sgpSecStr_SecStr) + 1;
        gOut.mpSecHdrs[SECIX_SEC_STR].sh_type = SHT_STRTAB;
        gOut.mpSecHdrs[SECIX_SYM_STR].sh_flags = SHF_STRINGS;
        gOut.mpSecHdrs[SECIX_SYM_STR].sh_addralign = 1;

        // symbol strings
        gOut.mpSecHdrs[SECIX_SYM_STR].sh_name = ((UINT32)gOut.mpSecStrWork) - ((UINT32)gOut.mpSecStrBase);
        K2ASC_Copy(gOut.mpSecStrWork, sgpSecStr_SymStr);
        gOut.mpSecStrWork += K2ASC_Len(sgpSecStr_SymStr) + 1;
        gOut.mpSecHdrs[SECIX_SYM_STR].sh_type = SHT_STRTAB;
        gOut.mpSecHdrs[SECIX_SYM_STR].sh_flags = SHF_STRINGS;
        gOut.mpSecHdrs[SECIX_SYM_STR].sh_addralign = 1;

        // symbol table
        gOut.mpSecHdrs[SECIX_SYM].sh_name = ((UINT32)gOut.mpSecStrWork) - ((UINT32)gOut.mpSecStrBase);
        K2ASC_Copy(gOut.mpSecStrWork,sgpSecStr_Sym);
        gOut.mpSecStrWork += K2ASC_Len(sgpSecStr_Sym) + 1;
        gOut.mpSecHdrs[SECIX_SYM].sh_type = SHT_SYMTAB;
        gOut.mpSecHdrs[SECIX_SYM].sh_addralign = 4;
        gOut.mpSecHdrs[SECIX_SYM].sh_link = SECIX_SYM_STR;
        gOut.mpSecHdrs[SECIX_SYM].sh_entsize = sizeof(Elf32_Sym);

        // dlx info
        gOut.mpSecHdrs[SECIX_DLXINFO].sh_name = ((UINT32)gOut.mpSecStrWork) - ((UINT32)gOut.mpSecStrBase);
        K2ASC_Copy(gOut.mpSecStrWork, sgpSecStr_DLXInfo);
        gOut.mpSecStrWork += K2ASC_Len(sgpSecStr_DLXInfo) + 1;
        gOut.mpSecHdrs[SECIX_DLXINFO].sh_flags = SHF_ALLOC | DLX_SHF_TYPE_DLXINFO;
        gOut.mpSecHdrs[SECIX_DLXINFO].sh_type = SHT_PROGBITS;
        gOut.mpSecHdrs[SECIX_DLXINFO].sh_addralign = 4;

        // dlx info symbol is always symbol 1, and its string is always at offset 1
        gOut.mpSymBase[1].st_name = 1;
        K2ASC_Copy(gOut.mpSymStrBase + gOut.mpSymBase[1].st_name, sgpSymInfo);
        gOut.mpSymBase[1].st_shndx = SECIX_DLXINFO;
        gOut.mpSymBase[1].st_size = sizeof(DLX_INFO) - sizeof(UINT32);
        // object file (REL, not EXEC), so symbol values are section relative, not absolute
        gOut.mpSymBase[1].st_value = 0; // gOut.mpSecHdrs[SECIX_DLXINFO].sh_addr;
        gOut.mpSymBase[1].st_info = ELF32_MAKE_SYMBOL_INFO(STB_GLOBAL, STT_OBJECT);
        K2MEM_Copy(gOut.mpInfo, &gOut.DlxInfo, sizeof(DLX_INFO) - sizeof(UINT32));

        if (gOut.mTotalExports > 0)
        {
            gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_name = ((UINT32)gOut.mpSecStrWork) - ((UINT32)gOut.mpSecStrBase);
            K2ASC_Copy(gOut.mpSecStrWork, sgpSecStr_DLXInfoReloc);
            gOut.mpSecStrWork += K2ASC_Len(sgpSecStr_DLXInfoReloc) + 1;
            gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_type = SHT_REL;
            gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_addralign = 4;
            gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_link = SECIX_SYM;
            gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_info = SECIX_DLXINFO;
            gOut.mpSecHdrs[SECIX_DLXINFO_RELOC].sh_entsize = sizeof(Elf32_Rel);

            for (ix = 0;ix < OUTSEC_COUNT;ix++)
                sEmitSection(ix);
        }

        if (0 != K2ELF32_Parse((UINT8 const *)gOut.mpFileHdr, gOut.mFileSizeBytes, &elfFile))
        {
            printf("*** Emitted file did not verify correctly\n");
            return false;
        }

        hOutFile = CreateFile(gOut.mpOutputFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOutFile == INVALID_HANDLE_VALUE)
        {
            printf("*** Error creating output file\n");
            return false;
        }

        ret = (TRUE == WriteFile(hOutFile, gOut.mpFileHdr, gOut.mFileSizeBytes, (LPDWORD)&ix, NULL));

        CloseHandle(hOutFile);

        if (!ret)
            DeleteFile(gOut.mpOutputFilePath);

    } while (false);

    delete[] ((UINT8 *)gOut.mRawBase);

    return ret;
}

int
Export(
    void
    )
{
    K2TREE_NODE *   pTreeNode;
    GLOBAL_SYMBOL * pGlob;
    EXPORT_SPEC *   pSpec;
    EXPORT_SPEC *   pPrev;
    EXPORT_SPEC *   pHold;
    EXPORT_SPEC *   pSpecEnd;
    int             result;

    if (gOut.mTotalExports > 0)
    {
        result = 0;
        pSpec = gOut.mOutSec[OUTSEC_CODE].mpSpec;
        while (pSpec != NULL)
        {
            pTreeNode = K2TREE_Find(&gOut.SymbolTree, (UINT32)pSpec->mpName);
            if (pTreeNode == NULL)
            {
                printf("*** Exported global code symbol \"%s\" was not found in any input file\n", pSpec->mpName);
                result = -100;
            }
            else
            {
                pGlob = K2_GET_CONTAINER(GLOBAL_SYMBOL, pTreeNode, TreeNode);
                if (!pGlob->mIsCode)
                {
                    printf("*** Exported global code symbol \"%s\" found, but is as data\n", pSpec->mpName);
                    result = -101;
                }
                else if (pGlob->mIsWeak)
                {
                    printf("*** Exported global code symbol \"%s\" cannot be weak\n", pSpec->mpName);
                    result = -102;
                }
                else if ((pGlob->mpObjFile->SectionHeader(pGlob->mpSymEnt->st_shndx)->sh_flags & DLX_SHF_TYPE_MASK) ==
                    DLX_SHF_TYPE_IMPORTS)
                {
                    printf("** Exported global code symbol \"%s\" cannot be an import\n", pSpec->mpName);
                    result = -107;
                }
            }
            pSpec = pSpec->mpNext;
        }
        if (result != 0)
            return result;
        //    printf("Code symbols all found ok\n");

        result = 0;
        pSpec = gOut.mOutSec[OUTSEC_DATA].mpSpec;
        pSpecEnd = NULL;
        pPrev = NULL;
        while (pSpec != NULL)
        {
            pTreeNode = K2TREE_Find(&gOut.SymbolTree, (UINT32)pSpec->mpName);
            if (pTreeNode == NULL)
            {
                printf("*** Exported global data symbol \"%s\" was not found in any input file\n", pSpec->mpName);
                result = -103;
                pSpec = pSpec->mpNext;
            }
            else
            {
                pGlob = K2_GET_CONTAINER(GLOBAL_SYMBOL, pTreeNode, TreeNode);
                if (pGlob->mIsCode)
                {
                    printf("*** Exported global data symbol \"%s\" found, but is as code\n", pSpec->mpName);
                    result = -104;
                }
                else if (pGlob->mIsWeak)
                {
                    printf("*** Exported global data symbol \"%s\" cannot be weak\n", pSpec->mpName);
                    result = -105;
                }
                else if ((pGlob->mpObjFile->SectionHeader(pGlob->mpSymEnt->st_shndx)->sh_flags & DLX_SHF_TYPE_MASK) ==
                    DLX_SHF_TYPE_IMPORTS)
                {
                    printf("** Exported global data or read symbol \"%s\" cannot be an import\n", pSpec->mpName);
                    result = -107;
                }

                if (pGlob->mIsRead)
                {
                    pHold = pSpec->mpNext;

                    if (pPrev == NULL)
                        gOut.mOutSec[OUTSEC_DATA].mpSpec = pSpec->mpNext;
                    else
                        pPrev->mpNext = pSpec->mpNext;
                    pSpec->mpNext = NULL;
                    gOut.mOutSec[OUTSEC_DATA].mCount--;

                    if (pSpecEnd != NULL)
                        pSpecEnd->mpNext = pSpec;
                    else
                        gOut.mOutSec[OUTSEC_READ].mpSpec = pSpec;
                    pSpecEnd = pSpec;
                    gOut.mOutSec[OUTSEC_READ].mCount++;

                    pSpec = pHold;
                }
                else
                {
                    pPrev = pSpec;
                    pSpec = pSpec->mpNext;
                }
            }
        }
        if (result != 0)
            return result;
        //    printf("Data symbols all found ok.  %d are read, %d are data\n", gOut.mOutSec[OUTSEC_READ].mCount, gOut.mOutSec[OUTSEC_DATA].mCount);
    }

    return sCreateOutputFile() ? 0 : -106;
}


