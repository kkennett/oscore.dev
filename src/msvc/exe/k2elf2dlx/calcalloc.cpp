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

#define SECTION_FLAG_PLACED     0x0001
#define SECTION_FLAG_HASOFFSET  0x0002
#define SECTION_FLAG_BSS        0x8000

static
void
sPlace(
    UINT32                      aSecIx,
    UINT32                      aSegIx,
    UINT8 const *               apSectionHeaderTable,
    UINT32                      aSecHdrBytes,
    ELFFILE_ALLOC *             apRetAlloc,
    ELFFILE_SECTION_MAPPING *   apRetMapping
)
{
    Elf32_Shdr const *  pSecHdr;
    Elf32_Shdr const *  pOtherHdr;
    Elf32_Word          align;

    if (apRetMapping[aSecIx].mFlags & SECTION_FLAG_PLACED)
    {
        K2_ASSERT(apRetMapping[aSecIx].mSegmentIndex == aSegIx);
        return;
    }

    pSecHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (aSecHdrBytes * aSecIx));

    // bss sections do not affect the alignment
    // unless they are the only section in the segment
    // in which case the code below takes care of that case  
    if (!(apRetMapping[aSecIx].mFlags & SECTION_FLAG_BSS))
    {
        align = pSecHdr->sh_addralign;
        if (align == 0)
            align = 1;

        if (apRetAlloc->Segment[aSegIx].mMemAlign < align)
        {
            apRetAlloc->Segment[aSegIx].mMemAlign = align;
            apRetAlloc->Segment[aSegIx].mMemByteCount = aSecIx;
        }
    }

    apRetAlloc->Segment[aSegIx].mSecCount++;
    apRetMapping[aSecIx].mSegmentIndex = aSegIx;
    apRetMapping[aSecIx].mFlags |= SECTION_FLAG_PLACED;

    if (pSecHdr->sh_type == SHT_REL)
    {
        // if relocations are for an ALLOC section, then 
        // we need the symbol table to be placed as well
        pOtherHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (aSecHdrBytes * pSecHdr->sh_info));
        if ((pOtherHdr->sh_flags & SHF_ALLOC) &&
            (pOtherHdr->sh_size > 0))
        {
            // these relocations are for an alloc section, so we need to place the symbol table
            sPlace(pSecHdr->sh_link, DlxSeg_Sym, apSectionHeaderTable, aSecHdrBytes, apRetAlloc, apRetMapping);
        }
    }
    else if (pSecHdr->sh_type == SHT_SYMTAB)
    {
        // we need the string table for this symbol table if there is one
        if (pSecHdr->sh_link != 0)
            sPlace(pSecHdr->sh_link, DlxSeg_Sym, apSectionHeaderTable, aSecHdrBytes, apRetAlloc, apRetMapping);
    }
}

K2STAT
CalcElfAlloc(
    Elf32_Ehdr const *          apFileHeader,
    UINT8 const *               apSectionHeaderTable,
    ELFFILE_ALLOC *             apRetAlloc,
    ELFFILE_SECTION_MAPPING *   apRetMapping
)
{
    Elf32_Shdr const *  pSecHdr;
    Elf32_Word          shEntSize;
    Elf32_Word          secIx;
    Elf32_Word          segIx;
    UINT32              bssCount;
    UINT32              target;
    Elf32_Word          align;
    UINT32              entBytes;
    UINT32              entCount;
    UINT32              sectionsInSegment;
    UINT32              offsetToTarget;
    UINT32              leftToCheck;
    UINT32              smallestOffset;
    UINT32              secWithSmallestOffsetToTarget;
    BOOL                firstBss;

    K2_ASSERT(apFileHeader != NULL);
    K2_ASSERT(apSectionHeaderTable != NULL);
    K2_ASSERT(apRetAlloc != NULL);
    K2_ASSERT(apRetMapping != NULL);

    if (apFileHeader->e_shnum == 0)
        return K2STAT_ERROR_ELF32_INVALID_SHNUM;

    shEntSize = apFileHeader->e_shentsize;
    if (shEntSize == 0)
        shEntSize = sizeof(Elf32_Shdr);
    else if (shEntSize < sizeof(Elf32_Shdr))
        return K2STAT_ERROR_ELF32_INVALID_SHENTSIZE;

    K2MEM_Zero(apRetAlloc, sizeof(ELFFILE_ALLOC));
    K2MEM_Zero(apRetMapping, sizeof(ELFFILE_SECTION_MAPPING) * apFileHeader->e_shnum);

    bssCount = 0;

    /* set up segment mapping first, and find largest alignments  */
    for (secIx = 1; secIx<apFileHeader->e_shnum; secIx++)
    {
        pSecHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (shEntSize * secIx));

        if (pSecHdr->sh_type == 0)
            continue;

        if (apRetMapping[secIx].mFlags & SECTION_FLAG_PLACED)
            continue;

        align = pSecHdr->sh_addralign;
        if (align == 0)
            align = 1;

        if (pSecHdr->sh_size == 0)
            target = DlxSeg_Other;
        else if (pSecHdr->sh_type == SHT_REL)
        {
            entBytes = pSecHdr->sh_entsize;
            if (entBytes == 0)
                entBytes = sizeof(Elf32_Rel);
            entCount = pSecHdr->sh_size / entBytes;
            if (entCount == 0)
                target = DlxSeg_Other;
            else
            {
                // symbol table and symbol string table are recursively placed
                target = DlxSeg_Reloc;
            }
        }
        else if ((pSecHdr->sh_flags & DLX_SHF_TYPE_MASK) == DLX_SHF_TYPE_IMPORTS)
            target = DlxSeg_Reloc;
        else if ((pSecHdr->sh_flags & DLX_SHF_TYPE_MASK) == DLX_SHF_TYPE_EXPORTS)
            target = DlxSeg_Read;
        else if ((pSecHdr->sh_flags & DLX_SHF_TYPE_MASK) == DLX_SHF_TYPE_DLXINFO)
            target = DlxSeg_Info;
        else if (pSecHdr->sh_flags & SHF_ALLOC)
        {
            /* text, read, or data */
            if ((pSecHdr->sh_type == SHT_INIT_ARRAY) ||
                (pSecHdr->sh_type == SHT_FINI_ARRAY))
                target = DlxSeg_Read;
            else if (pSecHdr->sh_flags & SHF_EXECINSTR)
                target = DlxSeg_Text;
            else
            {
                if (pSecHdr->sh_flags & SHF_WRITE)
                {
                    target = DlxSeg_Data;
                    if (pSecHdr->sh_type == SHT_NOBITS)
                    {
                        apRetMapping[secIx].mFlags |= SECTION_FLAG_BSS;
                        bssCount++;
                    }
                }
                else
                    target = DlxSeg_Read;
            }
        }
        else
        {
            // how to determine debug sections?
            target = DlxSeg_Other;
        }

        sPlace(secIx, target, apSectionHeaderTable, shEntSize, apRetAlloc, apRetMapping);
    }

    // take care of case where only section in data segment is bss
    if (apRetAlloc->Segment[DlxSeg_Data].mSecCount == 1)
    {
        if (bssCount != 0)
        {
            // only sections in data segment are bss
            leftToCheck = bssCount;
            for (secIx = 1; secIx < apFileHeader->e_shnum; secIx++)
            {
                if (apRetMapping[secIx].mFlags & SECTION_FLAG_BSS)
                {
                    pSecHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (shEntSize * secIx));

                    align = pSecHdr->sh_addralign;
                    if (align == 0)
                        align = 1;

                    if (apRetAlloc->Segment[DlxSeg_Data].mMemAlign < align)
                    {
                        apRetAlloc->Segment[DlxSeg_Data].mMemAlign = align;
                        apRetAlloc->Segment[DlxSeg_Data].mMemByteCount = secIx;
                    }

                    if (--leftToCheck == 0)
                        break;
                }
            }
        }
    }

    // remove count of bss sections from data segment section count
    apRetAlloc->Segment[DlxSeg_Data].mSecCount -= bssCount;

    //
    // all sections have a segment now, and each segment has the section with the highest alignment
    // requirement in its mMemByteCount entry
    //
    for (segIx = 0; segIx < DlxSeg_Count; segIx++)
    {
        // count number of sections in this segment
        sectionsInSegment = apRetAlloc->Segment[segIx].mSecCount;
        if (sectionsInSegment == 0)
            continue;

        // align must have been set when a section was added to the segment
        K2_ASSERT(apRetAlloc->Segment[segIx].mMemAlign != 0);

        // if we get here there is at least one section in this segment
        // and the alloc already has the largest alignment necessary and
        // the index of that section
        secIx = apRetAlloc->Segment[segIx].mMemByteCount;

        apRetMapping[secIx].mOffsetInSegment = 0;
        apRetMapping[secIx].mFlags |= SECTION_FLAG_HASOFFSET;

        pSecHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (shEntSize * secIx));

        apRetAlloc->Segment[segIx].mMemByteCount = pSecHdr->sh_size;

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
            for (secIx = 1; secIx < apFileHeader->e_shnum; secIx++)
            {
                if (apRetMapping[secIx].mSegmentIndex != segIx)
                    continue;
                if (apRetMapping[secIx].mFlags & (SECTION_FLAG_HASOFFSET | SECTION_FLAG_BSS))
                    continue;

                pSecHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (shEntSize * secIx));

                align = pSecHdr->sh_addralign;
                if (align == 0)
                    align = 1;
                target = apRetAlloc->Segment[segIx].mMemByteCount;
                target = K2_ROUNDUP(target, align);
                offsetToTarget = target - apRetAlloc->Segment[segIx].mMemByteCount;

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
            pSecHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (shEntSize * secWithSmallestOffsetToTarget));

            align = pSecHdr->sh_addralign;
            if (align == 0)
                align = 1;
            target = apRetAlloc->Segment[segIx].mMemByteCount;
            target = K2_ROUNDUP(target, align);

            apRetMapping[secWithSmallestOffsetToTarget].mOffsetInSegment = target;
            apRetMapping[secWithSmallestOffsetToTarget].mFlags |= SECTION_FLAG_HASOFFSET;

            apRetAlloc->Segment[segIx].mMemByteCount = target + pSecHdr->sh_size;

            sectionsInSegment--;

        } while (sectionsInSegment > 0);
    }

    if (bssCount == 0)
        return K2STAT_NO_ERROR;

    // put back count of bss sections into data segment section count
    apRetAlloc->Segment[DlxSeg_Data].mSecCount += bssCount;

    firstBss = TRUE;
    do
    {
        leftToCheck = bssCount;
        smallestOffset = (UINT32)-1;
        secWithSmallestOffsetToTarget = 0;

        for (secIx = 1; secIx < apFileHeader->e_shnum; secIx++)
        {
            if (apRetMapping[secIx].mFlags & SECTION_FLAG_HASOFFSET)
                continue;

            pSecHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (shEntSize * secIx));

            K2_ASSERT((apRetMapping[secIx].mFlags & SECTION_FLAG_BSS) != 0);

            align = pSecHdr->sh_addralign;
            if (align == 0)
                align = 1;
            target = apRetAlloc->Segment[DlxSeg_Data].mMemByteCount;
            target = K2_ROUNDUP(target, align);
            offsetToTarget = target - apRetAlloc->Segment[DlxSeg_Data].mMemByteCount;

            if (offsetToTarget < smallestOffset)
            {
                smallestOffset = offsetToTarget;
                secWithSmallestOffsetToTarget = secIx;
            }
            if (--leftToCheck == 0)
                break;
        }

        pSecHdr = (Elf32_Shdr const *)(apSectionHeaderTable + (shEntSize * secWithSmallestOffsetToTarget));

        align = pSecHdr->sh_addralign;
        if (align == 0)
            align = 1;
        target = apRetAlloc->Segment[DlxSeg_Data].mMemByteCount;
        target = K2_ROUNDUP(target, align);

        apRetMapping[secWithSmallestOffsetToTarget].mOffsetInSegment = target;
        apRetMapping[secWithSmallestOffsetToTarget].mFlags |= SECTION_FLAG_HASOFFSET;
        if (firstBss)
        {
            apRetAlloc->mEmptyDataOffset = target;
            firstBss = FALSE;
        }
        apRetAlloc->Segment[DlxSeg_Data].mMemByteCount = target + pSecHdr->sh_size;

        apRetAlloc->mEmptyDataBytes = apRetAlloc->Segment[DlxSeg_Data].mMemByteCount - apRetAlloc->mEmptyDataOffset;

        bssCount--;

    } while (bssCount > 0);

    return K2STAT_NO_ERROR;
}


