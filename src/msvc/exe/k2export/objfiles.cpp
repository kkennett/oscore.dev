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

static
bool
sAddOneGlobalSymbol(
    ELFObjectFile *     apObjFile,
    char const *        apSymName,
    Elf32_Sym const *   apSymEnt
    )
{
    Elf32_Shdr const *  pFoundSecHdr;
    Elf32_Shdr const *  pSymSecHdr;
    bool                isWeak;
    bool                isCode;
    bool                isRead;
    GLOBAL_SYMBOL *     pGlob;
    K2TREE_NODE *       pTreeNode;
    GLOBAL_SYMBOL *     pFound;
    UINT8 const *       pFoundData;
    UINT8 const *       pSymData;

    pSymSecHdr = apObjFile->SectionHeader(apSymEnt->st_shndx);

    isWeak = (ELF32_ST_BIND(apSymEnt->st_info) == STB_WEAK);
    isCode = (pSymSecHdr->sh_flags & SHF_EXECINSTR) ? true : false;
    if (isCode)
       isRead = true;
    else
       isRead = (pSymSecHdr->sh_flags & SHF_WRITE) ? false : true;

    pTreeNode = K2TREE_Find(&gOut.SymbolTree, (UINT32)apSymName);
    if (pTreeNode != NULL)
    {
        pFound = K2_GET_CONTAINER(GLOBAL_SYMBOL, pTreeNode, TreeNode);

        if ((pFound->mIsCode != isCode) ||
            (pFound->mIsRead != isRead))
        {
            printf("Symbol \"%s\" found with two different types:\n", apSymName);
            printf("%c%c in %s(%.*s)\n", 
                pFound->mIsCode ? 'x' : '-',
                pFound->mIsRead ? 'r' : '-',
                pFound->mpObjFile->ParentFile().FileName(), pFound->mpObjFile->ObjNameLen(), pFound->mpObjFile->ObjName());
            printf("%c%c in %s(%.*s)\n", 
                isCode ? 'x' : '-',
                isRead ? 'r' : '-',
                apObjFile->ParentFile().FileName(), apObjFile->ObjNameLen(), apObjFile->ObjName());
            return false;
        }

        if (pFound->mIsWeak)
        {
            if (!isWeak)
            {
                /* found weak symbol strong name. alter tree node */
                pFound->mIsWeak = false;
                pFound->mpObjFile = apObjFile;
                pFound->mpSymEnt = apSymEnt;
            }
            return true;
        }
        else if (!isWeak)
        {
            //
            // duplicate symbol
            //
            if ((pFound->mIsCode == isCode) &&
                (pFound->mpSymEnt->st_size == apSymEnt->st_size))
            {
                if (apSymEnt->st_size == 0)
                    pFound = NULL;
                else
                {
                    pFoundSecHdr = pFound->mpObjFile->SectionHeader(pFound->mpSymEnt->st_shndx);
                    pFoundData = pFound->mpObjFile->SectionData(pFound->mpSymEnt->st_shndx);
                    pFoundData += pFound->mpSymEnt->st_value;

                    pSymData = apObjFile->SectionData(apSymEnt->st_shndx);
                    pSymData += apSymEnt->st_value;

                    if (!K2MEM_Compare(pFoundData, pSymData, apSymEnt->st_size))
                    {
                        //
                        // symbol contents are identical
                        //
                        pFound = NULL;
                    }
                }
            }
            if (pFound)
            {
                printf("Duplicate symbol \"%s\" found:\n", apSymName);
                printf("in  %s(%.*s)\n", pFound->mpObjFile->ParentFile().FileName(), pFound->mpObjFile->ObjNameLen(), pFound->mpObjFile->ObjName());
                printf("and %s(%.*s)\n", apObjFile->ParentFile().FileName(), apObjFile->ObjNameLen(), apObjFile->ObjName());
                return false;
            }
            return true;
        }
    }

    pGlob = new GLOBAL_SYMBOL;
    if (pGlob == NULL)
    {
        printf("*** Memory allocation error\n");
        return false;
    }

    pGlob->mpObjFile = apObjFile;
    pGlob->mpSymEnt = apSymEnt;
    pGlob->mpSymName = apSymName;
    pGlob->mIsWeak = isWeak;
    pGlob->mIsCode = isCode; 
    pGlob->mIsRead = isRead;

    pGlob->TreeNode.mUserVal = (UINT32)apSymName;
    K2TREE_Insert(&gOut.SymbolTree, (UINT32)apSymName, &pGlob->TreeNode);

    return true;
}

static
bool
sAddOneObject(
    K2ReadOnlyMappedFile &  aSrcFile,
    char const *            apObjectFileName,
    UINT32                  aObjectFileNameLen,
    UINT8 const *           apFileData,
    UINT32                  aFileDataBytes
    )
{
    K2STAT              status;
    ELFObjectFile *     pObjFile;
    UINT32              secIx;
    Elf32_Shdr const *  pSymSecHdr;
    Elf32_Shdr const *  pStrSecHdr;
    UINT32              symLeft;
    UINT32              symBytes;
    char const *        pStr;
    UINT8 const *       pScan;
    char const *        pSymName;
    Elf32_Sym const *   pSym;
    UINT32              symNameLen;
    UINT32              symBind;

//    printf("Add Object \"%.*s\" for %d bytes\n", aObjectFileNameLen, apObjectFileName, aFileDataBytes);

    status = ELFObjectFile::Create(
        &aSrcFile, 
        apObjectFileName, 
        aObjectFileNameLen, 
        apFileData, 
        aFileDataBytes, 
        &pObjFile
        );
    if (K2STAT_IS_ERROR(status))
    {
        printf("*** Failed to parse object file \"%.*s\", error 0x%08X\n", aObjectFileNameLen, apObjectFileName, status);
        return false;
    }

    if (gOut.mElfMachine == 0)
        gOut.mElfMachine = pObjFile->FileHeader()->e_machine;
    else if (pObjFile->FileHeader()->e_machine != gOut.mElfMachine)
    {
        printf("*** Mismatched ELF machine types in object files\n");
        return false;
    }

    for (secIx = 1;secIx < pObjFile->NumSections();secIx++)
    {
        pSymSecHdr = pObjFile->SectionHeader(secIx);
        if (pSymSecHdr->sh_type != SHT_SYMTAB)
            continue;

        symBytes = pSymSecHdr->sh_entsize;
        if (symBytes == 0)
            symBytes = sizeof(Elf32_Sym);
        symLeft = pSymSecHdr->sh_size / symBytes;
        if (symLeft == 0)
            continue;

        pStrSecHdr = pObjFile->SectionHeader(pSymSecHdr->sh_link);
        pStr = (char const *)pObjFile->SectionData(pSymSecHdr->sh_link);

        pScan = pObjFile->SectionData(secIx);
        do
        {
            pSym = (Elf32_Sym *)pScan;
            pScan += symBytes;

            pSymName = pStr + pSym->st_name;

            symBind = ELF32_ST_BIND(pSym->st_info);

            if (((symBind==STB_GLOBAL) ||
                 (symBind==STB_WEAK)) &&
                (pSym->st_shndx != 0))
            {
                /* this is a global symbol in this object file */
                symNameLen = K2ASC_Len(pSymName);
                if ((symNameLen != 0) && (pSym->st_shndx < pObjFile->NumSections()))
                {
                    if (!sAddOneGlobalSymbol(pObjFile, pSymName, pSym))
                    {
                        printf("*** Could not add global symbol \"%s\"\n", pSymName);
                        return false;
                    }
                }
            }

        } while (--symLeft);
    }

    return true;
}

static
bool
sAddObjectLibrary(
    K2ReadOnlyMappedFile & aSrcFile
    )
{
    Elf32_LibRec    record;
    char *          pEnd;
    char *          pName;
    char            chk;
    UINT32          nameLen;
    UINT32          fileBytes;
    bool            ignoreThisFile;
    UINT32          validChars;
    char *          pLongNames;
    UINT32          longNamesLen;
    UINT32          nameOffset;
    UINT32          extLen;
    UINT8 const *   pParse;
    UINT32          parseLeft;

    pLongNames = NULL;

    pParse = (UINT8 const *)aSrcFile.DataPtr();
    parseLeft = aSrcFile.FileBytes();

    // bypass header
    pParse += 8;
    parseLeft -= 8;

    do
    {
        K2MEM_Copy(&record, pParse, ELF32_LIBREC_LENGTH);
        if ((record.mMagic[0] != 0x60) ||
            (record.mMagic[1] != 0x0A))
        {
            printf("*** File \"%s\" has invalid library file format.\n", aSrcFile.FileName());
            return false;
        }

        pParse += ELF32_LIBREC_LENGTH;
        parseLeft -= ELF32_LIBREC_LENGTH;

        record.mTime[0] = 0;
        validChars = 0;
        pEnd = record.mName + 15;
        do {
            chk = *pEnd;
            if (chk==' ')
            {
                if (validChars == 0)
                    *pEnd = 0;
            }
            else if (chk=='/')
                break;
            else
                validChars++;
        } while (--pEnd != record.mName);
        nameLen = (int)(pEnd - record.mName);

        record.mMagic[0] = 0;
        for (validChars = 0;validChars < 10;validChars++)
        {
            if ((record.mFileBytes[validChars] < '0') ||
                (record.mFileBytes[validChars] > '9'))
                break;
        }
        // this is fine if we overwrite the magic field
        record.mFileBytes[validChars] = 0;
        
        fileBytes = K2ASC_NumValue32(record.mFileBytes);
        if (fileBytes == 0)
        {
            printf("*** Invalid file size for file \"%s\" inside \"%s\"\n", record.mName, aSrcFile.FileName());
            return false;
        }

        ignoreThisFile = false;

        if (nameLen == 0)
        {
            if (record.mName[1] == 0)
            {
                // this is file '/' which is the symbol lookup table
                ignoreThisFile = true;
            }
            else
            {
                // this is a file with a long name
                if (pLongNames != NULL)
                {
                    nameOffset = K2ASC_NumValue32(&record.mName[1]);
                    if (nameOffset > longNamesLen)
                    {
                        ignoreThisFile = 1;
                        pName = (char *)"<error>";
                        nameLen = (int)strlen(pName);
                    }
                    else
                    {
                        pName = pLongNames + nameOffset;
                        nameLen = longNamesLen - nameOffset;
                        pEnd = pName;
                        do {
                            if (*pEnd=='/')
                                break;
                            pEnd++;
                        } while (--nameLen);
                        nameLen = (int) (pEnd - pName);
                        if (!nameLen)
                            ignoreThisFile = true;
                    }
                }
                else
                    ignoreThisFile = true;
            }
        }
        else if ((nameLen == 1) && (record.mName[1] == '/'))
        {
            // this is file '//' which is the long filenames table
            pLongNames = (char *)pParse;
            longNamesLen = fileBytes;
            ignoreThisFile = true;
        }
        else
            pName = record.mName;

        if (!ignoreThisFile)
        {
            if (nameLen)
            {
                pEnd = pName + nameLen - 1;
                extLen = 0;
                do {
                    chk = *pEnd;
                    if (chk=='.')
                        break;
                    extLen++;
                } while (--pEnd != pName);
                if (pEnd != pName)
                {
                    pEnd++;
                    chk = *pEnd;
                    if ((chk!='o') && (chk!='O'))
                        ignoreThisFile = true;
                }
                else
                {
                    /* no extension */
                    ignoreThisFile = true;
                }
            }
        }

        if (!ignoreThisFile)
        {
            if (!sAddOneObject(aSrcFile, pName, nameLen, pParse, fileBytes))
                return false;
        }

        if (fileBytes & 1)
            fileBytes++;
        pParse += fileBytes;
        if (parseLeft > fileBytes)
            parseLeft -= fileBytes;
        else
            parseLeft = 0;

    } while (parseLeft >= ELF32_LIBREC_LENGTH);

    return true;
}

bool
AddObjectFiles(
    K2ReadOnlyMappedFile & aSrcFile
    )
{
    static UINT8 const sgElfSig[4] = { 
        ELFMAG0,
        ELFMAG1,
        ELFMAG2,
        ELFMAG3 };
    UINT8 const *pChk;

    if (aSrcFile.FileBytes() < K2ELF32_MIN_FILE_SIZE)
    {
        printf("*** Input file \"%s\" is too small\n", aSrcFile.FileName());
        return false;
    }

    pChk = (UINT8 const *)aSrcFile.DataPtr();

    if (!K2MEM_Compare(pChk,"!<arch>\n",8))
        return sAddObjectLibrary(aSrcFile);

    if (!K2MEM_Compare(pChk,sgElfSig,4))
        return sAddOneObject(aSrcFile, aSrcFile.FileName(), K2ASC_Len(aSrcFile.FileName()), pChk, aSrcFile.FileBytes());

    printf("*** Input file \"%s\" is not recognized as an object file or object library.\n", aSrcFile.FileName());

    return false;
}