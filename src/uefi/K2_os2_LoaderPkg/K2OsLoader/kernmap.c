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
#include "K2OsLoader.h"

#if 0
example:
    00     0 addr 00000000 size 00000000 off 00000000
    01     1 addr FFF01000 size 000016EE off 00001000 .text
    02     1 addr FFF03000 size 00000484 off 00003000 .rodata
    03     1 addr FFF04000 size 00000074 off 00004000 .data
    04     1 addr FFF04080 size 00000FFF off 00004080 .bss_padding
    05     1 addr 00000000 size 00000011 off 0000507F .comment
    06     3 addr 00000000 size 00000045 off 00005090 .shstrtab
    07     2 addr 00000000 size 00000240 off 00005240 .symtab
    08     3 addr 00000000 size 000001C5 off 00005480 .strtab
 map read-only up to .text
 map text-only up to .rodata
 map read-only up to .data
 round up bss_padding START address to next page boundary.  map read-write up to that point
 map read-only from that point to the end of the file
 ELF header is the first byte of the region

 ROFS virtual is right after the end of that (page aligned).
 size of the ROFS is embedded inside of it.  

#endif

static int 
sSymCompare(void const *aPtr1, void const *aPtr2)
{
    UINT32 v1;
    UINT32 v2;

    v1 = ((Elf32_Sym const *)aPtr1)->st_value;
    v2 = ((Elf32_Sym const *)aPtr2)->st_value;
    if (v1 < v2)
        return -1;
    if (v1 == v2)
        return 0;
    return 1;
}

EFI_STATUS  
Loader_MapKernelArena(
    void
)
{
    //
    // gData.mKernElfPhys has physical address where k2oskern.elf was loaded
    // gData.LoadInfo.mKernSizeBytes has the count of data that was loaded
    // into the position
    //
    // we need to map the different portions of the kernel.elf with the right
    // attributes.  the file segments should be page aligned already and we
    // should barf if they are not
    //

    K2ELF32PARSE        parse;
    K2STAT              stat;
    UINT32              ix;
    UINT32              numSecHdr;
    Elf32_Shdr const *  pSecHdr = NULL;
    char const *        pSecStr;
    UINT32              virtAddr;
    UINT32              physAddr;
    UINT32              lastOffset;
    UINT32              dataEnd;

//    K2Printf(L"Kernel phys is %08X\n", gData.mKernElfPhys);
//    K2Printf(L"Kernel virt is %08X\n", K2OS_KVA_KERNEL_PREALLOC);
//    K2Printf(L"Kernel size is %08X\n", gData.LoadInfo.mKernSizeBytes);
    stat = K2ELF32_Parse((UINT8 const *)gData.mKernElfPhys, gData.LoadInfo.mKernSizeBytes, &parse);
    if (K2STAT_IS_ERROR(stat))
    {
        return EFI_DEVICE_ERROR;
    }

    gData.LoadInfo.mKernEntryPoint = (K2OSKERN_EntryPoint)parse.mpRawFileData->e_entry;
//    K2Printf(L"Kernel entrypoint at %08X\n", gData.LoadInfo.mKernEntryPoint);

    pSecStr = (char const *)K2ELF32_GetSectionData(&parse, parse.mpRawFileData->e_shstrndx);
    numSecHdr = parse.mpRawFileData->e_shnum;

    //
    // find and sort the symbol table in place to help out the kernel debugging
    //
    for (ix = 0; ix < numSecHdr; ix++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&parse, ix);
        if (pSecHdr->sh_type != SHT_SYMTAB)
            continue;
        break;
    }
    if (ix != numSecHdr)
    {
        K2SORT_Quick((void *)K2ELF32_GetSectionData(&parse, ix), pSecHdr->sh_size / pSecHdr->sh_entsize, sizeof(Elf32_Sym), sSymCompare);
    }

    //
    // find text section
    //
    for (ix = 1; ix < numSecHdr; ix++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&parse, ix);
        if (0 == K2ASC_Comp(pSecStr + pSecHdr->sh_name, ".text"))
            break;
    }
    if (ix == numSecHdr)
    {
        K2Printf(L"kernel .text section not found\n");
        return EFI_NOT_FOUND;
    }
    if (pSecHdr->sh_offset & 0xFFF)
    {
        K2Printf(L"kernel .text section is not page aligned\n");
        return EFI_NOT_FOUND;
    }

    physAddr = gData.mKernElfPhys;
    virtAddr = K2OS_KVA_KERNEL_PREALLOC;

    ix = pSecHdr->sh_offset / K2_VA32_MEMPAGE_BYTES;
//    K2Printf(L"Mapping %d pages r/o for elf header at start of image\n", ix);
    do
    {
        stat = K2VMAP32_MapPage(&gData.Map, virtAddr, physAddr, K2OS_MAPTYPE_KERN_READ);
        if (K2STAT_IS_ERROR(stat))
        {
            K2Printf(L"mapping %08X->%08X failed\n", virtAddr, physAddr);
            return EFI_NOT_FOUND;
        }
//        K2Printf(L"mapped %08X->%08X r\n", virtAddr, physAddr);
        physAddr += K2_VA32_MEMPAGE_BYTES;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--ix);
    lastOffset = pSecHdr->sh_offset;

    //
    // find rodata section
    //
    for (ix = 1; ix < numSecHdr; ix++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&parse, ix);
        if (0 == K2ASC_Comp(pSecStr + pSecHdr->sh_name, ".rodata"))
            break;
    }
    if (ix == numSecHdr)
    {
        K2Printf(L"kernel .rodata section not found\n");
        return EFI_NOT_FOUND;
    }
    if (pSecHdr->sh_offset & 0xFFF)
    {
        K2Printf(L"kernel .rodata section is not page aligned\n");
        return EFI_NOT_FOUND;
    }
    ix = (pSecHdr->sh_offset - lastOffset) / K2_VA32_MEMPAGE_BYTES;
//    K2Printf(L"Mapping %d pages r+x for text area\n", ix);
    do
    {
        stat = K2VMAP32_MapPage(&gData.Map, virtAddr, physAddr, K2OS_MAPTYPE_KERN_TEXT);
        if (K2STAT_IS_ERROR(stat))
        {
            K2Printf(L"mapping %08X->%08X failed\n", virtAddr, physAddr);
            return EFI_NOT_FOUND;
        }
//        K2Printf(L"mapped %08X->%08X rx\n", virtAddr, physAddr);
        physAddr += K2_VA32_MEMPAGE_BYTES;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--ix);
    lastOffset = pSecHdr->sh_offset;

    //
    // find data section
    //
    for (ix = 1; ix < numSecHdr; ix++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&parse, ix);
        if (0 == K2ASC_Comp(pSecStr + pSecHdr->sh_name, ".data"))
            break;
    }
    if (ix == numSecHdr)
    {
        K2Printf(L"kernel .data section not found\n");
        return EFI_NOT_FOUND;
    }
    if (pSecHdr->sh_offset & 0xFFF)
    {
        K2Printf(L"kernel .data section is not page aligned\n");
        return EFI_NOT_FOUND;
    }
    ix = (pSecHdr->sh_offset - lastOffset) / K2_VA32_MEMPAGE_BYTES;
//    K2Printf(L"Mapping %d pages ro for read area\n", ix);
    do
    {
        stat = K2VMAP32_MapPage(&gData.Map, virtAddr, physAddr, K2OS_MAPTYPE_KERN_READ);
        if (K2STAT_IS_ERROR(stat))
        {
            K2Printf(L"mapping %08X->%08X failed\n", virtAddr, physAddr);
            return EFI_NOT_FOUND;
        }
//        K2Printf(L"mapped %08X->%08X r\n", virtAddr, physAddr);
        physAddr += K2_VA32_MEMPAGE_BYTES;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--ix);
    lastOffset = pSecHdr->sh_offset;

    //
    // find bss_padding section
    //
    for (ix = 1; ix < numSecHdr; ix++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&parse, ix);
        if (0 == K2ASC_Comp(pSecStr + pSecHdr->sh_name, ".bss_padding"))
            break;
    }
    if (ix == numSecHdr)
    {
        K2Printf(L"kernel .bss_padding section not found\n");
        return EFI_NOT_FOUND;
    }
    dataEnd = (pSecHdr->sh_offset + 0xFFF) & ~0xFFF;

    ix = (dataEnd - lastOffset) / K2_VA32_MEMPAGE_BYTES;
//    K2Printf(L"Mapping %d pages rw for data area\n", ix);
    do
    {
        stat = K2VMAP32_MapPage(&gData.Map, virtAddr, physAddr, K2OS_MAPTYPE_KERN_DATA);
        if (K2STAT_IS_ERROR(stat))
        {
            K2Printf(L"mapping %08X->%08X failed\n", virtAddr, physAddr);
            return EFI_NOT_FOUND;
        }
//        K2Printf(L"mapped %08X->%08X rw\n", virtAddr, physAddr);
        physAddr += K2_VA32_MEMPAGE_BYTES;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--ix);
    lastOffset = dataEnd;

    dataEnd = (gData.LoadInfo.mKernSizeBytes + 0xFFF) & ~0xFFF;
    ix = (dataEnd - lastOffset) / K2_VA32_MEMPAGE_BYTES;
//    K2Printf(L"Mapping %d pages ro for extra goo\n", ix);
    do
    {
        stat = K2VMAP32_MapPage(&gData.Map, virtAddr, physAddr, K2OS_MAPTYPE_KERN_READ);
        if (K2STAT_IS_ERROR(stat))
        {
            K2Printf(L"mapping %08X->%08X failed\n", virtAddr, physAddr);
            return EFI_NOT_FOUND;
        }
//        K2Printf(L"mapped %08X->%08X r\n", virtAddr, physAddr);
        physAddr += K2_VA32_MEMPAGE_BYTES;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--ix);
//    K2Printf(L"KernElf end is %08X, this is where ROFS starts\n", virtAddr);

    physAddr = gData.LoadInfo.mBuiltinRofsPhys;
    ix = ((gData.mRofsBytes + 0xFFF) & ~0xFFF) / K2_VA32_MEMPAGE_BYTES;
//    K2Printf(L"Mapping %d pages ro for ROFS\n", ix);
    do
    {
        stat = K2VMAP32_MapPage(&gData.Map, virtAddr, physAddr, K2OS_MAPTYPE_KERN_READ);
        if (K2STAT_IS_ERROR(stat))
        {
            K2Printf(L"mapping %08X->%08X failed\n", virtAddr, physAddr);
            return EFI_NOT_FOUND;
        }
//        K2Printf(L"mapped %08X->%08X r\n", virtAddr, physAddr);
        physAddr += K2_VA32_MEMPAGE_BYTES;
        virtAddr += K2_VA32_MEMPAGE_BYTES;
    } while (--ix);
//    K2Printf(L"ROFS v  end is %08X\n", virtAddr);

    return EFI_SUCCESS;
}

