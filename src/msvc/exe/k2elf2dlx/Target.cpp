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

typedef struct _RELCTX RELCTX;
struct _RELCTX
{
    UINT32      mType;

    Elf32_Sym   OldSym;
    Elf32_Sym   NewSym;

    UINT32      mTargetSecIx;
    UINT8 *     mpTargetSecData;
    UINT32      mOffset;

    UINT32      mOldTargetSecAddr;
    UINT32      mNewTargetSecAddr;
};

static RELCTX sgRel;

typedef bool(*pfDoReloc)(void);

static pfDoReloc sDoReloc = NULL;

static bool sDoRelocX32(void)
{
    UINT8 * pTrgData;
    UINT32  targetVal;
    UINT32  origTargetAddr;
    UINT32  targetAddend;

    pTrgData = sgRel.mpTargetSecData + sgRel.mOffset;
    K2MEM_Copy(&targetVal, pTrgData, sizeof(UINT32));

    switch (sgRel.mType)
    {
    case R_386_32:
        targetAddend = targetVal - sgRel.OldSym.st_value;
        targetVal = sgRel.NewSym.st_value + targetAddend;
        break;

    case R_386_PC32:
        if (sgRel.mTargetSecIx == sgRel.NewSym.st_shndx)
        {
            // symbol being relocated is pc-relative in the same section as the target
            // so no relocation is actually needed.
            break;
        }

        // original target branch address
        origTargetAddr = sgRel.mOldTargetSecAddr + sgRel.mOffset;

        // take into account offset baked into original branch from point after instruction
        targetAddend = targetVal - (sgRel.OldSym.st_value - (origTargetAddr + 4));

        // offset from AFTER instruction with built-in target to new address
        targetVal = (sgRel.NewSym.st_value + targetAddend) -
            (sgRel.mNewTargetSecAddr + sgRel.mOffset + 4);
        break;

    default:
        printf("Unknown X32 relocation type %d\n", sgRel.mType);
        return false;
    }

    K2MEM_Copy(pTrgData, &targetVal, sizeof(UINT32));

    return true;
}

static bool sDoRelocA32(void)
{
    UINT8 * pTrgData;
    UINT32  targetVal;
    UINT32  origTargetAddr;
    UINT32  targetAddend;
    UINT32  newVal;

    pTrgData = sgRel.mpTargetSecData + sgRel.mOffset;
    K2MEM_Copy(&targetVal, pTrgData, sizeof(UINT32));
    switch (sgRel.mType)
    {
    case R_ARM_PC24:
    case R_ARM_CALL:
    case R_ARM_JUMP24:
        if (sgRel.mTargetSecIx == sgRel.NewSym.st_shndx)
        {
            // symbol being relocated is pc-relative in the same section as the target
            // so no relocation is actually needed.
            break;
        }
        targetAddend = (targetVal & 0xFFFFFF)<<2;
        if (targetAddend & 0x2000000)
            targetAddend |= 0xFC000000;
        origTargetAddr = sgRel.mOldTargetSecAddr + sgRel.mOffset;
        targetAddend -= (sgRel.OldSym.st_value - (origTargetAddr + 8));
        newVal = ((sgRel.NewSym.st_value + targetAddend) - ((sgRel.mNewTargetSecAddr + sgRel.mOffset) + 8));
        targetVal = (targetVal & 0xFF000000) | ((newVal >> 2) & 0xFFFFFF);
        break;

    case R_ARM_ABS32:
        targetAddend = targetVal - sgRel.OldSym.st_value;
        targetVal = sgRel.NewSym.st_value + targetAddend;
        break;

    case R_ARM_MOVW_ABS_NC:
        targetAddend = ((targetVal & 0xF0000) >> 4) | (targetVal & 0xFFF);
        targetAddend -= (sgRel.OldSym.st_value & 0xFFFF);
        newVal = ((sgRel.NewSym.st_value & 0xFFFF) + targetAddend) & 0xFFFF;
        targetVal = ((targetVal) & 0xFFF0F000) | ((newVal << 4) & 0xF0000) | (newVal & 0xFFF);
        break;

    case R_ARM_MOVT_ABS:
        targetAddend = ((targetVal & 0xF0000) >> 4) | (targetVal & 0xFFF);
        targetAddend -= ((sgRel.OldSym.st_value>>16) & 0xFFFF);
        newVal = ((((sgRel.NewSym.st_value & 0xFFFF0000) + (targetAddend<<16))) >> 16) & 0xFFFF;
        targetVal = ((targetVal) & 0xFFF0F000) | ((newVal << 4) & 0xF0000) | (newVal & 0xFFF);
        break;

    case R_ARM_PREL31:
        targetAddend = targetVal + sgRel.NewSym.st_value - (sgRel.mNewTargetSecAddr + sgRel.mOffset);
        targetVal = targetAddend & 0x7FFFFFFF;
        break;

    default:
        printf("Unknown A32 relocation type %d\n", sgRel.mType);
        return false;
    }
    K2MEM_Copy(pTrgData, &targetVal, sizeof(UINT32));

    return true;
}

int CreateTargetDLX(void)
{
    UINT8 *         pRawOut;
    UINT8 *         pAlignOut;
    Elf32_Ehdr *    pOutHdr;
    UINT32          secIx;
    UINT32          symEntBytes;
    UINT32          symEntCount;
    UINT32          relEntBytes;
    UINT32          relEntCount;
    UINT32 *        pOrigSecAddr;
    Elf32_Shdr *    pSecHdr;
    UINT32          symIx;
    UINT8 *         pSymNew;
    UINT8 *         pSymOld;
    UINT8 *         pRelWork;
    Elf32_Sym       symEnt;
    Elf32_Rel       relEnt;
    UINT32          segIx;
    UINT32          fileOffset;

    if (gOut.Parse.mpRawFileData->e_machine == EM_X32)
        sDoReloc = sDoRelocX32;
    else
        sDoReloc = sDoRelocA32;

    pOrigSecAddr = new UINT32[gOut.mOutSecCount];
    if (pOrigSecAddr == NULL)
    {
        printf("*** Memory allocation failed\n");
        return -500;
    }
    for (secIx = 1; secIx < gOut.mOutSecCount; secIx++)
    {
        pSecHdr = (Elf32_Shdr *)K2ELF32_GetSectionHeader(&gOut.Parse, gOut.mpRevRemap[secIx]);
        pOrigSecAddr[secIx] = pSecHdr->sh_addr;
    }

    // allocate the linear file contents
    pRawOut = new UINT8[gOut.mOutFileBytes + (DLX_SECTOR_BYTES - 1)];
    pAlignOut = (UINT8 *)K2_ROUNDUP(((UINT32)pRawOut), DLX_SECTOR_BYTES);
    K2MEM_Zero(pAlignOut, gOut.mOutFileBytes);

    // move the section headers and work on the real ones now
    pOutHdr = (Elf32_Ehdr *)pAlignOut;
    K2MEM_Copy(pAlignOut + sizeof(Elf32_Ehdr), gOut.mpOutSecHdr, sizeof(Elf32_Shdr) * gOut.mOutSecCount);
    delete[] gOut.mpOutSecHdr;
    gOut.mpOutSecHdr = (Elf32_Shdr *)(pAlignOut + sizeof(Elf32_Ehdr));

    // assemble the file
    pOutHdr->e_ident[EI_MAG0] =     ELFMAG0;
    pOutHdr->e_ident[EI_MAG1] =     ELFMAG1;
    pOutHdr->e_ident[EI_MAG2] =     ELFMAG2;
    pOutHdr->e_ident[EI_MAG3] =     ELFMAG3;
    pOutHdr->e_ident[EI_CLASS] =    ELFCLASS32;
    pOutHdr->e_ident[EI_DATA] =     ELFDATA2LSB;
    pOutHdr->e_ident[EI_VERSION] =  EV_CURRENT;
    pOutHdr->e_ident[EI_OSABI] =    DLX_ELFOSABI_K2;
    pOutHdr->e_ident[EI_ABIVERSION] = DLX_ELFOSABIVER_DLX;
    pOutHdr->e_type = DLX_ET_DLX;
    pOutHdr->e_machine = gOut.Parse.mpRawFileData->e_machine;
    pOutHdr->e_version = EV_CURRENT;
    pOutHdr->e_entry = gOut.Parse.mpRawFileData->e_entry;
    if (gOut.mIsKernelDLX)
        pOutHdr->e_flags = DLX_EF_KERNEL_ONLY;
    if (pOutHdr->e_machine == EM_A32)
        pOutHdr->e_flags |= EF_A32_ABIVER;
    pOutHdr->e_ehsize = sizeof(Elf32_Ehdr);
    pOutHdr->e_shentsize = sizeof(Elf32_Shdr);
    pOutHdr->e_shnum = gOut.mOutSecCount;
    pOutHdr->e_shstrndx = 2;
    pOutHdr->e_shoff = sizeof(Elf32_Ehdr);

    // copy over section contents to target
    for (secIx = 1; secIx < gOut.mOutSecCount;secIx++)
    {
        if (gOut.mpOutSecHdr[secIx].sh_type != SHT_NOBITS)
        {
            K2MEM_Copy(pAlignOut + gOut.mpOutSecHdr[secIx].sh_offset,
                gOut.mppWorkSecData[secIx],
                gOut.mpOutSecHdr[secIx].sh_size);
        }
    }

    // change symbol addresses and entry point address
    for (secIx = 1; secIx < gOut.mOutSecCount;secIx++)
    {
        if ((pOutHdr->e_entry >= pOrigSecAddr[secIx]) &&
            ((pOutHdr->e_entry - pOrigSecAddr[secIx]) < gOut.mpOutSecHdr[secIx].sh_size))
        {
            pOutHdr->e_entry -= pOrigSecAddr[secIx];
            pOutHdr->e_entry += gOut.mpOutSecHdr[secIx].sh_addr;
        }
        if (gOut.mpOutSecHdr[secIx].sh_type == SHT_SYMTAB)
        {
            symEntBytes = gOut.mpOutSecHdr[secIx].sh_entsize;
            symEntCount = gOut.mpOutSecHdr[secIx].sh_size / symEntBytes;
            pSymNew = pAlignOut + gOut.mpOutSecHdr[secIx].sh_offset;
            if (!K2MEM_Verify(pSymNew, 0, sizeof(Elf32_Sym)))
            {
                printf("*** Internal error\n");
                return -501;
            }
            for (symIx = 1; symIx < symEntCount;symIx++)
            {
                K2MEM_Copy(&symEnt, pSymNew + (symIx * symEntBytes), sizeof(Elf32_Sym));
                if ((symEnt.st_shndx != 0) &&
                    (symEnt.st_shndx < gOut.mOutSecCount))
                {
                    symEnt.st_value -= pOrigSecAddr[symEnt.st_shndx];
                    symEnt.st_value += gOut.mpOutSecHdr[symEnt.st_shndx].sh_addr;
                    K2MEM_Copy(pSymNew + (symIx * symEntBytes), &symEnt, sizeof(Elf32_Sym));
                }
            }
        }
    }

    // apply relocations
    K2MEM_Zero(&sgRel, sizeof(sgRel));
    for (secIx = 1; secIx < gOut.mOutSecCount;secIx++)
    {
        pSecHdr = &gOut.mpOutSecHdr[secIx];
        if (pSecHdr->sh_type != SHT_REL)
            continue;

        pRelWork = pAlignOut + pSecHdr->sh_offset;
        relEntBytes = pSecHdr->sh_entsize;
        relEntCount = pSecHdr->sh_size / relEntBytes;

        pSymNew = pAlignOut + gOut.mpOutSecHdr[pSecHdr->sh_link].sh_offset;
        pSymOld = gOut.mppWorkSecData[pSecHdr->sh_link];
        symEntBytes = gOut.mpOutSecHdr[pSecHdr->sh_link].sh_entsize;
        symEntCount = gOut.mpOutSecHdr[pSecHdr->sh_link].sh_size / symEntBytes;

        sgRel.mTargetSecIx = pSecHdr->sh_info;
        sgRel.mpTargetSecData = pAlignOut + gOut.mpOutSecHdr[sgRel.mTargetSecIx].sh_offset;
        sgRel.mOldTargetSecAddr = pOrigSecAddr[sgRel.mTargetSecIx];
        sgRel.mNewTargetSecAddr = gOut.mpOutSecHdr[sgRel.mTargetSecIx].sh_addr;

        do
        {
            K2MEM_Copy(&relEnt, pRelWork, sizeof(Elf32_Rel));
            symIx = ELF32_R_SYM(relEnt.r_info);
            K2_ASSERT((symIx != 0) && (symIx < symEntCount));
            K2MEM_Copy(&sgRel.OldSym, pSymOld + (symIx * symEntBytes), sizeof(Elf32_Sym));
            K2MEM_Copy(&sgRel.NewSym, pSymNew + (symIx * symEntBytes), sizeof(Elf32_Sym));
            sgRel.mType = ELF32_R_TYPE(relEnt.r_info);
            sgRel.mOffset = relEnt.r_offset - sgRel.mOldTargetSecAddr;
            if (!sDoReloc())
                return -505;
            relEnt.r_offset = sgRel.mNewTargetSecAddr + sgRel.mOffset;
            K2MEM_Copy(pRelWork, &relEnt, sizeof(Elf32_Rel));
            pRelWork += relEntBytes;
        } while (--relEntCount);
    }

    // set segment CRCs
    gOut.mpDstInfo = (DLX_INFO *)(pAlignOut + gOut.mpOutSecHdr[1].sh_offset);
    fileOffset = sizeof(Elf32_Ehdr) + (sizeof(Elf32_Shdr) * gOut.mOutSecCount);
    fileOffset += gOut.mpDstInfo->SegInfo[0].mFileBytes;
    for (segIx = DlxSeg_Text; segIx < DlxSeg_Count; segIx++)
    {
        if (fileOffset & DLX_SECTOROFFSET_MASK)
        {
            printf("*** file offset wrong when creating target\n");
            return -505;
        }
        if (gOut.mpDstInfo->SegInfo[segIx].mFileBytes > 0)
        {
            gOut.mpDstInfo->SegInfo[segIx].mCRC32 = K2CRC_Calc32(0, pAlignOut + fileOffset, gOut.mpDstInfo->SegInfo[segIx].mFileBytes);
            fileOffset += gOut.mpDstInfo->SegInfo[segIx].mFileBytes;
        }
    }

    // set elf headers crc
    gOut.mpDstInfo->mElfCRC = K2CRC_Calc32(0, pAlignOut, gOut.mpOutSecHdr[1].sh_offset);

    // dlx info crc is last and is with it's own crc segment field as zero
    gOut.mpDstInfo->SegInfo[DlxSeg_Info].mCRC32 = 0;
    fileOffset = sizeof(Elf32_Ehdr) + (sizeof(Elf32_Shdr) * gOut.mOutSecCount);
    gOut.mpDstInfo->SegInfo[DlxSeg_Info].mCRC32 = 
        K2CRC_Calc32(0, pAlignOut + fileOffset, gOut.mpDstInfo->SegInfo[DlxSeg_Info].mFileBytes);

    if (!WriteFile(gOut.mhOutput, pAlignOut, gOut.mOutFileBytes, (DWORD *)&secIx, NULL))
        return GetLastError();
    
    delete pRawOut;

    return 0;
}
