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
#ifndef __SPEC_ELF32_H
#define __SPEC_ELF32_H

//
// --------------------------------------------------------------------------------- 
//

#include <k2basetype.h>

#include <spec/elf32def.inc>

#ifdef __cplusplus
extern "C" {
#endif

//
// From
//    System V Application Binary Interface - DRAFT - 10 June 2013
//
//    http://www.sco.com/developers/gabi/latest/contents.html
//

typedef UINT32  Elf32_Addr;
typedef UINT32  Elf32_Off;
typedef UINT16  Elf32_Half;
typedef INT32   Elf32_Sword;
typedef UINT32  Elf32_Word;

K2_PACKED_PUSH
struct _Elf32_Ehdr 
{
	UINT8       e_ident[EI_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half  e_machine;
	Elf32_Word  e_version;
	Elf32_Addr  e_entry;
	Elf32_Off   e_phoff;
	Elf32_Off   e_shoff;
	Elf32_Word  e_flags;
	Elf32_Half  e_ehsize;
	Elf32_Half  e_phentsize;
	Elf32_Half  e_phnum;
	Elf32_Half  e_shentsize;
	Elf32_Half  e_shnum;
	Elf32_Half  e_shstrndx;
} K2_PACKED_ATTRIB;
K2_PACKED_POP
typedef struct _Elf32_Ehdr Elf32_Ehdr;

K2_PACKED_PUSH
struct _Elf32_Shdr 
{
    Elf32_Word	sh_name;
    Elf32_Word	sh_type;
    Elf32_Word	sh_flags;
    Elf32_Addr	sh_addr;
    Elf32_Off	sh_offset;
    Elf32_Word	sh_size;
    Elf32_Word	sh_link;
    Elf32_Word	sh_info;
    Elf32_Word	sh_addralign;
    Elf32_Word	sh_entsize;
} K2_PACKED_ATTRIB;
K2_PACKED_POP
typedef struct _Elf32_Shdr Elf32_Shdr;

K2_PACKED_PUSH
struct _Elf32_Chdr 
{
    Elf32_Word	ch_type;
    Elf32_Word	ch_size;
    Elf32_Word	ch_addralign;
}  K2_PACKED_ATTRIB;
K2_PACKED_POP
typedef struct _Elf32_Chdr Elf32_Chdr;

K2_PACKED_PUSH
struct _Elf32_Sym
{
    Elf32_Word  st_name;
    Elf32_Addr  st_value;
    Elf32_Word  st_size;
    UINT8       st_info;
    UINT8       st_other;
    Elf32_Half  st_shndx;
} K2_PACKED_ATTRIB;
K2_PACKED_POP
typedef struct _Elf32_Sym Elf32_Sym;

K2_PACKED_PUSH
struct _Elf32_Rel
{
    Elf32_Addr  r_offset;
    Elf32_Word  r_info;
} K2_PACKED_ATTRIB;
K2_PACKED_POP
typedef struct _Elf32_Rel Elf32_Rel;

K2_PACKED_PUSH
struct _Elf32_Phdr
{
    Elf32_Word  p_type;
    Elf32_Off   p_offset;
    Elf32_Addr  p_vaddr;
    Elf32_Addr  p_paddr;
    Elf32_Word  p_filesz;
    Elf32_Word  p_memsz;
    Elf32_Word  p_flags;
    Elf32_Word  p_align;
} K2_PACKED_ATTRIB;
K2_PACKED_POP
typedef struct _Elf32_Phdr Elf32_Phdr;

K2_PACKED_PUSH
struct _Elf32_LibRec
{
    char    mName[16];
    char    mTime[12];
    char    mJunk[20];
    char    mFileBytes[10];
    char    mMagic[2];
} K2_PACKED_ATTRIB;
K2_PACKED_POP
typedef struct _Elf32_LibRec Elf32_LibRec;
K2_STATIC_ASSERT(sizeof(Elf32_LibRec) == ELF32_LIBREC_LENGTH);

#define ELF32_ST_TYPE(st_info)                  (((UINT8)(st_info)) & 0xF)
#define ELF32_ST_BIND(st_info)                  ((((UINT8)(st_info))>>4) & 0xF)
#define ELF32_ST_INFO(binding,stype)            ((((UINT8)(binding))<<4) | (((UINT8)(stype))&0xF))

#define ELF32_ST_VISIBILITY(st_other)           (((UINT8)(st_other)) & 0x3)

#define ELF32_R_SYM(r_info)                     (((Elf32_Word)(r_info))>>8)
#define ELF32_R_TYPE(r_info)                    (((Elf32_Word)(r_info))&0xFF)
#define ELF32_R_INFO(symbol, rtype)             ((((Elf32_Word)(symbol))<<8)|((((Elf32_Word)(rtype))&0xFF)))

#define ELF32_MAKE_RELOC_INFO(symbol, rtype)    ((((Elf32_Word)(symbol))<<8) | ((((Elf32_Word)(rtype))&0xFF)))
#define ELF32_MAKE_SYMBOL_INFO(binding,stype)   ((((UINT8)(binding))<<4) | (((UINT8)(stype))&0xF))

#ifdef __cplusplus
};  // extern "C"
#endif

/* ------------------------------------------------------------------------- */

#endif  // __SPEC_ELF32_H

