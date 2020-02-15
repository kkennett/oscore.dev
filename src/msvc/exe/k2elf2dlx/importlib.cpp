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

typedef union _WORKPTR WORKPTR;
union _WORKPTR
{
    UINT32   mAsVal;
    void *  mAsPtr;
};

static char const * const sgpSecStr_SecStr = ".sechdrstr";
static char const * const sgpSecStr_SymStr = ".symstr";
static char const * const sgpSecStr_Sym = ".symtable";
static char const * const sgpSecStr_Imp = ".?import_";

int CreateImportLib(void)
{
    UINT32                  importKey;
    int                     ret;
    UINT32                  fileSizeBytes;
    UINT32                  secStrBytes;
    UINT32                  symStrBytes;
    UINT32                  sectionCount;
    WORKPTR                 workFile;
    WORKPTR                 workElfBase;
    WORKPTR                 work;

    UINT32                  symStrSecSize;
    Elf32_LibRec *          pOutLibRecord;
    Elf32_Ehdr *            pOutHdr;
    Elf32_Shdr *            pOutSecHdr;
    char *                  pOutSecHdrStr;
    Elf32_Sym *             pOutSymTab;
    DLX_EXPORTS_SECTION *   pOutExpSec[3];
    UINT32                  outSecIx[3];
    UINT32                  ixExpType;
    UINT32                  expSecBytes;
    UINT32                  ixExport;
    UINT32                  ixSym;
    WORKPTR                 outSymStrBase;
    WORKPTR                 outSymStrWork;
    char *                  pOutStr;
    UINT8                   symType;
    K2TREE_ANCHOR           symTree;
    K2TREE_NODE *           pTreeNode;
    UINT32                  indexBytes;
    UINT8 *                 pLibIndex;
    Elf32_LibRec *          pLibIndexRecord;
    UINT32                  rev;

    K2TREE_Init(&symTree, TreeStrCompare);

    importKey = K2CRC_Calc32(0, gOut.mpTargetName, gOut.mTargetNameLen);

    // start with empty symbol string table
    symStrBytes = 1;

    // start with 'empty' section header string table
    secStrBytes = 1;

    // start with library header and the single object file record
    fileSizeBytes = 8 + ELF32_LIBREC_LENGTH;

    // object file header
    fileSizeBytes += sizeof(Elf32_Ehdr);

    // null section header
    sectionCount = 1;

    // section header string table header
    secStrBytes += K2ASC_Len(sgpSecStr_SecStr) + 1;
    sectionCount++;

    // symbol table section 
    fileSizeBytes += sizeof(Elf32_Sym) * (gOut.mTotalExportCount + 1);
    secStrBytes += K2ASC_Len(sgpSecStr_Sym) + 1;
    sectionCount++;

    // symbol string table section header
    symStrSecSize = gOut.mTotalExportStrSize + 1;
    symStrSecSize = K2_ROUNDUP(symStrSecSize, 4);
    fileSizeBytes += symStrSecSize;
    secStrBytes += K2ASC_Len(sgpSecStr_SymStr) + 1;
    sectionCount++;

    // import sections and symbol index entry for library (stupidly necessary)
    for (ixExpType = 0;ixExpType < 3;ixExpType++)
    {
        if (gOut.mpExpSec[ixExpType] != NULL)
        {
            secStrBytes += K2ASC_Len(sgpSecStr_Imp) + gOut.mTargetNameLen + 1;

            // import data (strings are going into string table)
            expSecBytes = sizeof(DLX_EXPORTS_SECTION) - sizeof(DLX_EXPORT_REF);
            expSecBytes += gOut.mpExpSec[ixExpType]->mCount * sizeof(DLX_EXPORT_REF);
            expSecBytes += sizeof(K2_GUID128);
            expSecBytes += gOut.mTargetNameLen + 1;
            expSecBytes = K2_ROUNDUP(expSecBytes, 4);
            fileSizeBytes += expSecBytes;
            outSecIx[ixExpType] = sectionCount++;

            for (ixExport = 0;ixExport < gOut.mpExpSec[ixExpType]->mCount;ixExport++)
            {
                K2TREE_NODE *pNode = new K2TREE_NODE;
                if (pNode == NULL)
                {
                    printf("*** Memory allocation failed\n");
                    return -120;
                }
                pNode->mUserVal = (UINT32)(((char *)gOut.mpExpSec[ixExpType]) + gOut.mpExpSec[ixExpType]->Export[ixExport].mNameOffset);
                K2TREE_Insert(&symTree, pNode->mUserVal, pNode);
            }
        }
        else
            outSecIx[ixExpType] = 0;
    }
    K2_ASSERT(symTree.mNodeCount == gOut.mTotalExportCount);
    indexBytes = (sizeof(UINT32) * (gOut.mTotalExportCount + 1)) + gOut.mTotalExportStrSize;
    indexBytes = K2_ROUNDUP(indexBytes, 4);
    fileSizeBytes += ELF32_LIBREC_LENGTH + indexBytes;

    secStrBytes = K2_ROUNDUP(secStrBytes, 4);

    fileSizeBytes += sizeof(Elf32_Shdr) * sectionCount;

    fileSizeBytes += secStrBytes;

    workFile.mAsPtr = new UINT8[(fileSizeBytes + 3) & ~3];
    if (workFile.mAsPtr == NULL)
    {
        printf("*** Memory allocation failed\n");
        return -100;
    }
    K2MEM_Zero(workFile.mAsPtr, fileSizeBytes);

    ret = -101;
    do
    {
        work.mAsPtr = workFile.mAsPtr;

        K2ASC_Copy((char *)work.mAsPtr, "!<arch>\n");
        work.mAsVal += 8;

        pLibIndexRecord = (Elf32_LibRec *)work.mAsPtr;
        K2ASC_Copy(pLibIndexRecord->mName, "/               ");
        K2ASC_Copy(pLibIndexRecord->mTime, "1367265298  ");
        K2ASC_Copy(pLibIndexRecord->mJunk, "0     0     100666  ");
        K2ASC_PrintfLen(pLibIndexRecord->mFileBytes, 11, "%-10d", indexBytes);
        pLibIndexRecord->mMagic[0] = 0x60;
        pLibIndexRecord->mMagic[1] = 0x0A;
        work.mAsVal += ELF32_LIBREC_LENGTH;

        pLibIndex = (UINT8 *)work.mAsPtr;
        work.mAsVal += indexBytes;
        *((UINT32 *)pLibIndex) = K2_SWAP32(gOut.mTotalExportCount);
        pLibIndex += sizeof(UINT32);
        rev = work.mAsVal - workFile.mAsVal;
        rev = K2_SWAP32(rev);
        for (ixExport = 0;ixExport < gOut.mTotalExportCount;ixExport++)
        {
            *((UINT32 *)pLibIndex) = rev;
            pLibIndex += sizeof(UINT32);
        }
        pTreeNode = K2TREE_FirstNode(&symTree);
        do
        {
            K2TREE_Remove(&symTree, pTreeNode);
            K2ASC_Copy((char *)pLibIndex, (char *)pTreeNode->mUserVal);
            pLibIndex += K2ASC_Len((char *)pLibIndex) + 1;
            pTreeNode = K2TREE_FirstNode(&symTree);
        } while (pTreeNode != NULL);

        pOutLibRecord = (Elf32_LibRec *)work.mAsPtr;
        K2ASC_PrintfLen(pOutLibRecord->mName, 16, "%d.o/", importKey);
        K2ASC_Copy(pOutLibRecord->mTime, "1367265298  ");
        K2ASC_Copy(pOutLibRecord->mJunk, "0     0     100666  ");
        K2ASC_PrintfLen(pOutLibRecord->mFileBytes, 11, "%-10d", fileSizeBytes - (8 + ELF32_LIBREC_LENGTH));
        pOutLibRecord->mMagic[0] = 0x60;
        pOutLibRecord->mMagic[1] = 0x0A;
        work.mAsVal += ELF32_LIBREC_LENGTH;

        workElfBase.mAsPtr = work.mAsPtr;
        pOutHdr = (Elf32_Ehdr *)work.mAsPtr;
        work.mAsVal += sizeof(Elf32_Ehdr);
        pOutHdr->e_ident[EI_MAG0] = ELFMAG0;
        pOutHdr->e_ident[EI_MAG1] = ELFMAG1;
        pOutHdr->e_ident[EI_MAG2] = ELFMAG2;
        pOutHdr->e_ident[EI_MAG3] = ELFMAG3;
        pOutHdr->e_ident[EI_CLASS] = ELFCLASS32;
        pOutHdr->e_ident[EI_DATA] =  ELFDATA2LSB;
        pOutHdr->e_ident[EI_VERSION] =  EV_CURRENT;
        pOutHdr->e_type = ET_REL;
        pOutHdr->e_machine = gOut.Parse.mpRawFileData->e_machine;
        pOutHdr->e_version = EV_CURRENT;
        if (pOutHdr->e_machine == EM_A32)
            pOutHdr->e_flags = EF_A32_ABIVER;
        pOutHdr->e_ehsize = sizeof(Elf32_Ehdr);
        pOutHdr->e_shentsize = sizeof(Elf32_Shdr);
        pOutHdr->e_shnum = sectionCount;
        pOutHdr->e_shstrndx = 1;
        pOutHdr->e_shoff = work.mAsVal - workElfBase.mAsVal;

        pOutSecHdr = (Elf32_Shdr *)work.mAsPtr;
        work.mAsVal += sizeof(Elf32_Shdr) * sectionCount;

        pOutSecHdrStr = (char *)work.mAsPtr;
        pOutSecHdr[1].sh_name = 1;
        pOutSecHdr[1].sh_type = SHT_STRTAB;
        pOutSecHdr[1].sh_size = secStrBytes;
        pOutSecHdr[1].sh_offset = work.mAsVal - workElfBase.mAsVal;
        pOutSecHdr[1].sh_addralign = 1;
        K2ASC_Copy(pOutSecHdrStr + pOutSecHdr[1].sh_name, sgpSecStr_SecStr);
        secStrBytes = 1 + K2ASC_Len(sgpSecStr_SecStr) + 1;
        work.mAsVal += pOutSecHdr[1].sh_size;

        pOutSymTab = (Elf32_Sym *)work.mAsPtr;
        pOutSecHdr[2].sh_name = secStrBytes;
        pOutSecHdr[2].sh_type = SHT_SYMTAB;
        pOutSecHdr[2].sh_size = (gOut.mTotalExportCount + 1) * sizeof(Elf32_Sym);
        pOutSecHdr[2].sh_offset = work.mAsVal - workElfBase.mAsVal;
        pOutSecHdr[2].sh_link = 3;
        pOutSecHdr[2].sh_addralign = 4;
        pOutSecHdr[2].sh_entsize = sizeof(Elf32_Sym);
        K2ASC_Copy(pOutSecHdrStr + pOutSecHdr[2].sh_name, sgpSecStr_Sym);
        secStrBytes += K2ASC_Len(sgpSecStr_Sym) + 1;
        work.mAsVal += pOutSecHdr[2].sh_size;
        ixSym = 1;

        outSymStrBase = outSymStrWork = work;
        outSymStrWork.mAsVal++;
        pOutSecHdr[3].sh_name = secStrBytes;
        pOutSecHdr[3].sh_type = SHT_STRTAB;
        pOutSecHdr[3].sh_size = symStrSecSize;
        pOutSecHdr[3].sh_offset = work.mAsVal - workElfBase.mAsVal;
        pOutSecHdr[3].sh_addralign = 1;
        K2ASC_Copy(pOutSecHdrStr + pOutSecHdr[3].sh_name, sgpSecStr_SymStr);
        secStrBytes += K2ASC_Len(sgpSecStr_SymStr) + 1;
        work.mAsVal += pOutSecHdr[3].sh_size;

        // imports
        for (ixExpType = 0;ixExpType < 3;ixExpType++)
        {
            if (gOut.mpExpSec[ixExpType] != NULL)
            {
                pOutExpSec[ixExpType] = (DLX_EXPORTS_SECTION *)work.mAsPtr;
                pOutExpSec[ixExpType]->mCount = gOut.mpExpSec[ixExpType]->mCount;
                pOutExpSec[ixExpType]->mCRC32 = gOut.mpExpSec[ixExpType]->mCRC32;
                pOutSecHdr[outSecIx[ixExpType]].sh_name = secStrBytes;
                pOutStr = pOutSecHdrStr + pOutSecHdr[outSecIx[ixExpType]].sh_name;
                K2ASC_Printf(pOutStr, "%s%.*s", sgpSecStr_Imp, gOut.mTargetNameLen, gOut.mpTargetName);
                if (ixExpType == 0)
                    pOutStr[1] = 'c';
                else if (ixExpType == 1)
                    pOutStr[1] = 'r';
                else
                    pOutStr[1] = 'd';
                secStrBytes += K2ASC_Len(sgpSecStr_Imp) + gOut.mTargetNameLen + 1;

                pOutSecHdr[outSecIx[ixExpType]].sh_type = SHT_PROGBITS;
                pOutSecHdr[outSecIx[ixExpType]].sh_flags = SHF_ALLOC | DLX_SHF_TYPE_IMPORTS;
                if (ixExpType == 0)
                    pOutSecHdr[outSecIx[ixExpType]].sh_flags |= SHF_EXECINSTR;
                else if (ixExpType == 2)
                    pOutSecHdr[outSecIx[ixExpType]].sh_flags |= SHF_WRITE;

                expSecBytes = sizeof(DLX_EXPORTS_SECTION) - sizeof(DLX_EXPORT_REF);
                expSecBytes += gOut.mpExpSec[ixExpType]->mCount * sizeof(DLX_EXPORT_REF);
                K2MEM_Copy(((UINT8 *)work.mAsPtr) + expSecBytes, &gOut.mpSrcInfo->ID, sizeof(K2_GUID128));
                expSecBytes += sizeof(K2_GUID128);
                K2ASC_CopyLen(((char *)pOutExpSec[ixExpType]) + expSecBytes, gOut.mpTargetName, gOut.mTargetNameLen);
                expSecBytes += gOut.mTargetNameLen + 1;
                expSecBytes = K2_ROUNDUP(expSecBytes, 4);

                if (ixExpType == 0)
                    symType = ELF32_MAKE_SYMBOL_INFO(STB_GLOBAL, STT_FUNC);
                else
                    symType = ELF32_MAKE_SYMBOL_INFO(STB_GLOBAL, STT_OBJECT);
                for (ixExport = 0;ixExport < gOut.mpExpSec[ixExpType]->mCount;ixExport++)
                {
                    pOutSymTab[ixSym].st_name = (outSymStrWork.mAsVal - outSymStrBase.mAsVal);
                    pOutSymTab[ixSym].st_value = (sizeof(DLX_EXPORTS_SECTION) - sizeof(DLX_EXPORT_REF)) +
                        (ixExport * sizeof(DLX_EXPORT_REF));
                    pOutSymTab[ixSym].st_size = sizeof(DLX_EXPORT_REF);
                    pOutSymTab[ixSym].st_info = symType;
                    pOutSymTab[ixSym].st_shndx = outSecIx[ixExpType];
                    ixSym++;

                    K2ASC_Copy((char *)outSymStrWork.mAsPtr, 
                               ((char *)gOut.mpExpSec[ixExpType]) + gOut.mpExpSec[ixExpType]->Export[ixExport].mNameOffset);
                    outSymStrWork.mAsVal += K2ASC_Len((char *)outSymStrWork.mAsPtr) + 1;
                }

                pOutSecHdr[outSecIx[ixExpType]].sh_size = expSecBytes;
                pOutSecHdr[outSecIx[ixExpType]].sh_offset = work.mAsVal - workElfBase.mAsVal;
                pOutSecHdr[outSecIx[ixExpType]].sh_addr = 0x10000000 | pOutSecHdr[outSecIx[ixExpType]].sh_offset;
                pOutSecHdr[outSecIx[ixExpType]].sh_addralign = 4;
                work.mAsVal += expSecBytes;
            }
        }
        if ((ixSym != gOut.mTotalExportCount + 1) ||
            ((work.mAsVal - workFile.mAsVal) != fileSizeBytes))
        {
            printf("*** Internal error\n");
        }
        else
        {
#if 0
            K2ELF32PARSE file32;
            K2STAT status;

            status = K2ELF32_Parse(workElfBase.mAsPtr, fileSizeBytes - (8 + ELF32_LIBREC_LENGTH), &file32);
            if (K2STAT_IS_ERROR(status))
            {
                printf("*** File LoadBack failed\n");
                ret = -110;
            }
            else
#endif
                ret = 0;
        }

    } while (false);

    if (ret == 0)
    {
        DWORD wrot = 0;
        if (!WriteFile(gOut.mhOutLib, workFile.mAsPtr, fileSizeBytes, &wrot, NULL))
        {
            printf("*** Import library file write error\n");
            ret = GetLastError();
        }
    }

    delete[] ((UINT8 *)workFile.mAsPtr);

    return ret;
}