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

#ifndef __K2OSDEFS_INC
#define __K2OSDEFS_INC

#include <k2basedefs.inc>

/* --------------------------------------------------------------------------------- */

#define K2OS_MAX_CPU_COUNT  8

//
// must exactly match K2OS_UEFI_LOADINFO in k2osbase.h
//
#define K2OS_UEFI_LOADINFO_OFFSET_MARKER                        0x0000      // 'K2OS'
#define K2OS_UEFI_LOADINFO_OFFSET_CPU_CORE_COUNT                0x0004
#define K2OS_UEFI_LOADINFO_OFFSET_EFIST                         0x0008
#define K2OS_UEFI_LOADINFO_OFFSET_KERN_SIZE_BYTES               0x000C 
#define K2OS_UEFI_LOADINFO_OFFSET_KERN_ENTRY_POINT              0x0010      // encoded in code bytes in uefi loader. careful
#define K2OS_UEFI_LOADINFO_OFFSET_BUILTIN_ROFS_PHYS             0x0014
#define K2OS_UEFI_LOADINFO_OFFSET_TRANSBASE_PHYS                0x0018      // encoded in code bytes in uefi loader. careful
#define K2OS_UEFI_LOADINFO_OFFSET_ZEROPAGE_PHYS                 0x001C
#define K2OS_UEFI_LOADINFO_OFFSET_TRANS_PAGE_ADDR               0x0020
#define K2OS_UEFI_LOADINFO_OFFSET_DEBUG_PAGE_VIRT               0x0024
#define K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_SIZE                   0x0028
#define K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_DESC_SIZE              0x002C 
#define K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_DESC_VER               0x0030
#define K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGES_PHYS              0x0034
#define K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGES_VIRT              0x0038
#define K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGE_COUNT              0x003C
#define K2OS_UEFI_LOADINFO_OFFSET_FWFACS_PHYS                   0x0040
#define K2OS_UEFI_LOADINFO_OFFSET_XFWFACS_PHYS                  0x0044
#define K2OS_UEFI_LOADINFO_SIZE_BYTES                           0x0048

//
// UEFI 2.5 Spec Page 152, EFI_BOOT_SERVICES.AllocatePages
//
// "MemoryType values in the range 0x80000000..0xFFFFFFFF are reserved for use by UEFI OS loaders
// that are provided by operating system vendors."
//
#define K2OS_EFIMEMTYPE_KERNEL_ELF      0x80000000  //  kernel elf image
#define K2OS_EFIMEMTYPE_PAGING          0x80000001  //  page directory and page tables
#define K2OS_EFIMEMTYPE_PROC1           0x80000002  //  process 1 default memory 
#define K2OS_EFIMEMTYPE_ZERO            0x80000003  //  one and only zero page
#define K2OS_EFIMEMTYPE_CORES           0x80000004  //  4 pages per core
#define K2OS_EFIMEMTYPE_PHYS_TRACK      0x80000005  //  used for tracking physical memory
#define K2OS_EFIMEMTYPE_TRANSITION      0x80000006  //  one and only transition page
#define K2OS_EFIMEMTYPE_EFI_MAP         0x80000007  //  EFI memory map
#define K2OS_EFIMEMTYPE_FW_TABLES       0x80000008  //  Firmware ACPI and SMBIOS tables
#define K2OS_EFIMEMTYPE_ARCH_SPEC       0x80000009  //  Architecture Specific reservation
#define K2OS_EFIMEMTYPE_BUILTIN         0x8000000A  //  Builtin Read Only File System (ROFS)
#define K2OS_EFIMEMTYPE_THREADPTRS      0x8000000B  //  thread pointers page 
#define K2OS_EFIMEMTYPE_LAST            K2OS_EFIMEMTYPE_THREADPTRS

//
// size of physical per-page tracking structure
//
#define K2OS_PHYSTRACK_BYTES                        (4 * 4)

//
// number of memory pages to hold a process 
//
#define K2OS_PROC_PAGECOUNT                         4
#define K2OS_PROC_PAGES_OFFSET_PROC_ITSELF          0   // process structure itself 
#define K2OS_PROC_PAGES_OFFSET_TOKEN_TABLE          1   // root token table page 
#define K2OS_PROC_PAGES_OFFSET_TLS_PAGETABLE        2   // pagetable that maps 0 -> 4MB in this process 
#define K2OS_PROC_PAGES_OFFSET_PAGECOUNT            3   // process pagecount 

//
// max memory pages to use for EFI memory map coming from UEFI 
//
#define K2OS_EFIMAP_PAGECOUNT                       2

//
// max # of threads in the system 
//
#define K2OS_MAX_THREAD_COUNT                       1024

/* --------------------------------------------------------------------------------- */

//
// Kernel Virtual Address Range (KVA) top down
//
#define K2OS_PUBLICAPI_SIZE                         0x00001000
#define K2OS_KVA_PUBLICAPI_BASE                     0xFFFFF000

#define K2OS_KVA_FACS_SIZE                          0x00001000
#define K2OS_KVA_FACS_BASE                          0xFFFFE000

#define K2OS_KVA_XFACS_SIZE                         0x00001000
#define K2OS_KVA_XFACS_BASE                         0xFFFFD000

#define K2OS_KVA_ARCHSPEC_SIZE                      0x0000D000
#define K2OS_KVA_ARCHSPEC_BASE                      0xFFFF0000  // must be here for A32

#define K2OS_KVA_COREMEMORY_SIZE                    0x00020000  // 0x4000 * 8 = 0x20000
#define K2OS_KVA_COREMEMORY_BASE                    0xFFFD0000
#define K2OS_KVA_COREMEMORY_BASE_HIGH16             0xFFFD
#define K2OS_KVA_COREMEMORY_BASE_LOW16              0x0000

#define K2OS_KVA_KERNEL_ARENA                       0xFFC00000
#define K2OS_KVA_KERNEL_ARENA_SIZE                  0x003D0000 // 3904 KB

// this must be virtually 4MB aligned and pagetable not auto mapper by loader
#define K2OS_KVA_THREAD_TLS_BASE                    0xFF800000
#define K2OS_KVA_THREAD_TLS_SIZE                    0x00400000  

#define K2OS_KVA_FREE_TOP                           K2OS_KVA_THREAD_TLS_BASE

/* --------------------------------------------------------------------------------- */

#define K2OS_X32_ARCH_PAGES_BASE                    0
#define K2OS_X32_APSTART_TRANSIT_PAGE_ADDR          0x00001000
#define K2OS_X32_APSTART_PAGEDIR_ADDR               0x00002000
#define K2OS_X32_APSTART_PAGETABLE_ADDR             0x00003000
#define K2OS_X32_ARCH_PAGES_COUNT                   4
#define K2OS_X32_MAX_IOAPICS_COUNT                  2

// 0xFFFF0000
#define K2OS_KVA_X32_LOCAPIC                        K2OS_KVA_ARCHSPEC_BASE
#define K2OS_KVA_X32_LOCAPIC_SIZE                   K2_VA32_MEMPAGE_BYTES

// 0xFFFF1000
#define K2OS_KVA_X32_IOAPICS                        (K2OS_KVA_X32_LOCAPIC + K2OS_KVA_X32_LOCAPIC_SIZE)
#define K2OS_KVA_X32_IOAPICS_SIZE                   (K2_VA32_MEMPAGE_BYTES * K2OS_X32_MAX_IOAPICS_COUNT)

// 0xFFFF3000
#define K2OS_KVA_X32_HPET                           (K2OS_KVA_X32_IOAPICS + K2OS_KVA_X32_IOAPICS_SIZE)
#define K2OS_KVA_X32_HPET_SIZE                      K2_VA32_MEMPAGE_BYTES

// 0xFFFF4000
#define K2OS_KVA_X32_AP_TRANSITION                  (K2OS_KVA_X32_HPET + K2OS_KVA_X32_HPET_SIZE)
#define K2OS_KVA_X32_AP_TRANSITION_SIZE             K2_VA32_MEMPAGE_BYTES

// 0xFFFF5000
#define K2OS_KVA_X32_AP_PAGEDIR                     (K2OS_KVA_X32_AP_TRANSITION + K2OS_KVA_X32_AP_TRANSITION_SIZE)
#define K2OS_KVA_X32_AP_PAGEDIR_SIZE                K2_VA32_MEMPAGE_BYTES

// 0xFFFF6000
#define K2OS_KVA_X32_AP_PAGETABLE                   (K2OS_KVA_X32_AP_PAGEDIR + K2OS_KVA_X32_AP_PAGEDIR_SIZE)
#define K2OS_KVA_X32_AP_PAGETABLE_SIZE              K2_VA32_MEMPAGE_BYTES

// 0xFFFF7000
#define K2OS_KVA_X32_ARCHSPEC_END                   (K2OS_KVA_X32_AP_PAGETABLE + K2OS_KVA_X32_AP_PAGETABLE_SIZE)

/* --------------------------------------------------------------------------------- */

#define K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT       10  

// 0xFFFF0000
#define K2OS_KVA_A32_VECTORS                        K2OS_KVA_ARCHSPEC_BASE  // MUST BE HERE
#define K2OS_KVA_A32_VECTORS_SIZE                   K2_VA32_MEMPAGE_BYTES

// 0xFFFF1000
#define K2OS_KVA_A32_PERIPHBASE                     (K2OS_KVA_A32_VECTORS + K2OS_KVA_A32_VECTORS_SIZE)
#define K2OS_KVA_A32_PERIPHBASE_SIZE                K2_VA32_MEMPAGE_BYTES

// 0xFFFF2000
#define K2OS_KVA_A32_GICD                           (K2OS_KVA_A32_PERIPHBASE + K2OS_KVA_A32_PERIPHBASE_SIZE)
#define K2OS_KVA_A32_GICD_SIZE                      K2_VA32_MEMPAGE_BYTES

// 0xFFFF3000
#define K2OS_KVA_A32_GICC                           (K2OS_KVA_A32_GICD + K2OS_KVA_A32_GICD_SIZE)
#define K2OS_KVA_A32_GICC_SIZE                      K2_VA32_MEMPAGE_BYTES

// 0xFFFF4000
#define K2OS_KVA_A32_AP_TTB                         (K2OS_KVA_A32_GICC + K2OS_KVA_A32_GICC_SIZE)  // must be 16K aligned
#define K2OS_KVA_A32_AP_TTB_SIZE                    (K2_VA32_MEMPAGE_BYTES * 4)

// 0xFFFF8000
#define K2OS_KVA_A32_AP_PAGETABLE                   (K2OS_KVA_A32_AP_TTB + K2OS_KVA_A32_AP_TTB_SIZE)
#define K2OS_KVA_A32_AP_PAGETABLE_SIZE              K2_VA32_MEMPAGE_BYTES

// 0xFFFF9000
#define K2OS_KVA_A32_AP_TRANSIT                     (K2OS_KVA_A32_AP_PAGETABLE + K2OS_KVA_A32_AP_PAGETABLE_SIZE)
#define K2OS_KVA_A32_AP_TRANSIT_SIZE                K2_VA32_MEMPAGE_BYTES

// 0xFFFFA000 
#define K2OS_KVA_A32_ARCHSPEC_END                   (K2OS_KVA_A32_AP_TRANSIT + K2OS_KVA_A32_AP_TRANSIT_SIZE)

/* --------------------------------------------------------------------------------- */

//
// Kernel Virtual Address Range (KVA) bottom up
//
#define K2OS_KVA_KERN_BASE                          0x80000000             
#define K2OS_KVA_KERN_SELFMAP_ADDR                  0x80000000  // virtual address of PTE mapping itself

// 0x80000000 - 0x803FFFFF -- this must be virtually 4MB aligned 
#define K2OS_KVA_KERNVAMAP_BASE                     K2OS_KVA_KERN_BASE
#define K2OS_KVA_KERNVAMAP_SIZE                     (K2_VA32_PAGEFRAMES_FOR_4G * 4)     

// 0x80400000 - 0x813FFFFF
#define K2OS_KVA_PHYSTRACKAREA_BASE                 (K2OS_KVA_KERNVAMAP_BASE + K2OS_KVA_KERNVAMAP_SIZE)
#define K2OS_KVA_PHYSTRACKAREA_SIZE                 (K2_VA32_PAGEFRAMES_FOR_4G * K2OS_PHYSTRACK_BYTES)
#define K2OS_KVA_PHYSTRACKAREA_END                  (K2OS_KVA_PHYSTRACKAREA_BASE + K2OS_KVA_PHYSTRACKAREA_SIZE)

//
// translation table base must be aligned on 16KB boundary on A32
//
// 0x81400000
#define K2OS_KVA_TRANSTAB_BASE                      K2OS_KVA_PHYSTRACKAREA_END

// 0x81401000 x32 / 0x81404000 a32
#define K2OS_KVA_EFIMAP_BASE                        (K2OS_KVA_TRANSTAB_BASE + K2_VA32_TRANSTAB_SIZE)
#define K2OS_KVA_EFIMAP_SIZE                        (K2OS_EFIMAP_PAGECOUNT * K2_VA32_MEMPAGE_BYTES)

// 0x81403000 x32 / 0x81406000 a32
#define K2OS_KVA_PROC1_AREA_BASE                    (K2OS_KVA_EFIMAP_BASE + K2OS_KVA_EFIMAP_SIZE)
#define K2OS_KVA_PROC1_AREA_SIZE                    (K2OS_PROC_PAGECOUNT * K2_VA32_MEMPAGE_BYTES)

// 0x81407000 x32 / 0x8140A000 a32
#define K2OS_KVA_THREADPTRS_BASE                    (K2OS_KVA_PROC1_AREA_BASE + K2OS_KVA_PROC1_AREA_SIZE)

// 0x81408000 x32 / 0x8140B000 a32
#define K2OS_KVA_FREE_BOTTOM                        (K2OS_KVA_THREADPTRS_BASE + K2_VA32_MEMPAGE_BYTES)

/* --------------------------------------------------------------------------------- */

//
// User Virtual Address Range (UVA)
//
#define K2OS_UVA_PUBLICAPI_BASE                     0x7FFFF000 
#define K2OS_UVA_TLSAREA_BASE                       0x00000000 
#define K2OS_UVA_LOW_BASE                           0x00400000 

/* --------------------------------------------------------------------------------- */

//
// derived definitions
//

// 0xFFFD3FFC 
#define K2OS_KVA_CORE0_STACK_INIT                   (K2OS_KVA_COREMEMORY_BASE + (K2_VA32_MEMPAGE_BYTES * 4) - 4)

#define K2OS_KVA_TO_PTE_ADDR(x)                     K2_VA32_TO_PTE_ADDR(K2OS_KVA_KERNVAMAP_BASE, (x))
#define K2OS_KVA_TO_PT_ADDR(x)                      K2_VA32_TO_PT_ADDR(K2OS_KVA_KERNVAMAP_BASE, (x))

#define K2OS_PHYS32_TO_PHYSTRACK(x)                 (K2OS_KVA_PHYSTRACKAREA_BASE + (((x) >> K2_VA32_MEMPAGE_BYTES_POW2) * K2OS_PHYSTRACK_BYTES))
#define K2OS_PHYSTRACK_TO_PHYS32(x)                 ((((x) - K2OS_KVA_PHYSTRACKAREA_BASE) / K2OS_PHYSTRACK_BYTES) << K2_VA32_MEMPAGE_BYTES_POW2)

#define K2OS_KVA_PROC1                              (K2OS_KVA_PROC1_AREA_BASE + (K2_VA32_MEMPAGE_BYTES * K2OS_PROC_PAGES_OFFSET_PROC_ITSELF))
#define K2OS_KVA_PROC1_TOKEN_TABLE                  (K2OS_KVA_PROC1_AREA_BASE + (K2_VA32_MEMPAGE_BYTES * K2OS_PROC_PAGES_OFFSET_TOKEN_TABLE))
#define K2OS_KVA_PROC1_TLS_PAGETABLE                (K2OS_KVA_PROC1_AREA_BASE + (K2_VA32_MEMPAGE_BYTES * K2OS_PROC_PAGES_OFFSET_TLS_PAGETABLE))
#define K2OS_KVA_PROC1_PAGECOUNT                    (K2OS_KVA_PROC1_AREA_BASE + (K2_VA32_MEMPAGE_BYTES * K2OS_PROC_PAGES_OFFSET_PAGECOUNT))

/* --------------------------------------------------------------------------------- */

#define K2OS_MEMPAGE_ATTR_NONE                      0
#define K2OS_MEMPAGE_ATTR_EXEC                      PF_X        // elf32, 0x00000001
#define K2OS_MEMPAGE_ATTR_WRITEABLE                 PF_W        // elf32, 0x00000002
#define K2OS_MEMPAGE_ATTR_READABLE                  PF_R        // elf32, 0x00000004
#define K2OS_MEMPAGE_ATTR_READWRITE                 (K2OS_MEMPAGE_ATTR_READABLE | K2OS_MEMPAGE_ATTR_WRITEABLE)
#define K2OS_MEMPAGE_ATTR_UNCACHED                  0x00000008
#define K2OS_MEMPAGE_ATTR_DEVICEIO                  0x00000010
#define K2OS_MEMPAGE_ATTR_WRITE_THRU                0x00000020
#define K2OS_MEMPAGE_ATTR_USER                      0x00000040
#define K2OS_MEMPAGE_ATTR_PAGING                    0x00000080
#define K2OS_MEMPAGE_ATTR_TRANSITION                0x00000100
#define K2OS_MEMPAGE_ATTR_AUX                       0x00000200 
#define K2OS_MEMPAGE_ATTR_SPEC_NP                   0x00000400 
#define K2OS_MEMPAGE_ATTR_MASK                      0x00000FFF  // can't be any bigger

/* --------------------------------------------------------------------------------- */

#define iK2OS_MAPTYPE_READ                          K2OS_MEMPAGE_ATTR_READABLE
#define iK2OS_MAPTYPE_TEXT                          (iK2OS_MAPTYPE_READ | K2OS_MEMPAGE_ATTR_EXEC)
#define iK2OS_MAPTYPE_DATA                          (iK2OS_MAPTYPE_READ | K2OS_MEMPAGE_ATTR_WRITEABLE)
#define iK2OS_MAPTYPE_DEVICEIO                      (iK2OS_MAPTYPE_DATA | K2OS_MEMPAGE_ATTR_DEVICEIO)
#define iK2OS_MAPTYPE_WRITETHRU_CACHED              (iK2OS_MAPTYPE_DATA | K2OS_MEMPAGE_ATTR_WRITE_THRU)
#define iK2OS_MAPTYPE_UNCACHED                      (iK2OS_MAPTYPE_DATA | K2OS_MEMPAGE_ATTR_UNCACHED)

#define K2OS_MAPTYPE_KERN_TEXT                      iK2OS_MAPTYPE_READ            
#define K2OS_MAPTYPE_KERN_READ                      iK2OS_MAPTYPE_TEXT            
#define K2OS_MAPTYPE_KERN_DATA                      iK2OS_MAPTYPE_DATA            
#define K2OS_MAPTYPE_KERN_DEVICEIO                  iK2OS_MAPTYPE_DEVICEIO        
#define K2OS_MAPTYPE_KERN_WRITETHRU_CACHED          iK2OS_MAPTYPE_WRITETHRU_CACHED
#define K2OS_MAPTYPE_KERN_UNCACHED                  iK2OS_MAPTYPE_UNCACHED        

#define K2OS_MAPTYPE_USER_TEXT                      (K2OS_MEMPAGE_ATTR_USER | iK2OS_MAPTYPE_READ)         
#define K2OS_MAPTYPE_USER_READ                      (K2OS_MEMPAGE_ATTR_USER | iK2OS_MAPTYPE_TEXT)
#define K2OS_MAPTYPE_USER_DATA                      (K2OS_MEMPAGE_ATTR_USER | iK2OS_MAPTYPE_DATA)
#define K2OS_MAPTYPE_USER_DEVICEIO                  (K2OS_MEMPAGE_ATTR_USER | iK2OS_MAPTYPE_DEVICEIO)
#define K2OS_MAPTYPE_USER_WRITETHRU_CACHED          (K2OS_MEMPAGE_ATTR_USER | iK2OS_MAPTYPE_WRITETHRU_CACHED)
#define K2OS_MAPTYPE_USER_UNCACHED                  (K2OS_MEMPAGE_ATTR_USER | iK2OS_MAPTYPE_UNCACHED)

#define K2OS_MAPTYPE_KERN_TRANSITION                (K2OS_MAPTYPE_KERN_UNCACHED | K2OS_MEMPAGE_ATTR_EXEC | K2OS_MEMPAGE_ATTR_TRANSITION)
                                                       
#define K2OS_MAPTYPE_KERN_PAGETABLE                 (K2OS_MAPTYPE_KERN_DATA | K2OS_MEMPAGE_ATTR_PAGING)
#define K2OS_MAPTYPE_KERN_PAGEDIR                   K2OS_MAPTYPE_KERN_PAGETABLE

/* --------------------------------------------------------------------------------- */

#define K2OS_PUBLICAPI_OFFSET_MSTIMER64             0xFF8
#define K2OS_PUBLICAPI_OFFSET_DATETIME              0xFE0
#define K2OS_PUBLIACPI_OFFSET_SYSCALL               0

#define K2OS_KVA_PUBLICAPI_MSTIMER64                (K2OS_KVA_PUBLICAPI_BASE + K2OS_PUBLICAPI_OFFSET_MSTIMER64)
#define K2OS_KVA_PUBLICAPI_DATETIME                 (K2OS_KVA_PUBLICAPI_BASE + K2OS_PUBLICAPI_OFFSET_DATETIME)
#define K2OS_KVA_PUBLICAPI_SYSCALL                  (K2OS_KVA_PUBLICAPI_BASE + K2OS_PUBLIACPI_OFFSET_SYSCALL)

#define K2OS_UVA_PUBLICAPI_MSTIMER64                (K2OS_KUA_PUBLICAPI_BASE + K2OS_PUBLICAPI_OFFSET_MSTIMER64)
#define K2OS_UVA_PUBLICAPI_DATETIME                 (K2OS_KUA_PUBLICAPI_BASE + K2OS_PUBLICAPI_OFFSET_DATETIME)
#define K2OS_UVA_PUBLICAPI_SYSCALL                  (K2OS_KUA_PUBLICAPI_BASE + K2OS_PUBLIACPI_OFFSET_SYSCALL)

/* --------------------------------------------------------------------------------- */

#endif // __K2OSDEFS_INC
