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

#define K2OS_MEMPAGE_ATTR_NONE              0
#define K2OS_MEMPAGE_ATTR_EXEC              PF_X        // elf32, 0x00000001
#define K2OS_MEMPAGE_ATTR_WRITEABLE         PF_W        // elf32, 0x00000002
#define K2OS_MEMPAGE_ATTR_READABLE          PF_R        // elf32, 0x00000004
#define K2OS_MEMPAGE_ATTR_READWRITE         (K2OS_MEMPAGE_ATTR_READABLE | K2OS_MEMPAGE_ATTR_WRITEABLE)
#define K2OS_MEMPAGE_ATTR_UNCACHED          0x00000008
#define K2OS_MEMPAGE_ATTR_DEVICEIO          0x00000010
#define K2OS_MEMPAGE_ATTR_WRITE_THRU        0x00000020
#define K2OS_MEMPAGE_ATTR_COPY_ON_WRITE     0x00000040
#define K2OS_MEMPAGE_ATTR_SPEC_NP           0x00000080
#define K2OS_MEMPAGE_ATTR_KERNEL            0x00000100
#define K2OS_MEMPAGE_ATTR_PAGING            0x00000200
#define K2OS_MEMPAGE_ATTR_TRANSITION        0x00000400
#define K2OS_MEMPAGE_ATTR_AUX               0x00000800 
#define K2OS_MEMPAGE_ATTR_MASK              0x00000FFF  // can't be any bigger

/* --------------------------------------------------------------------------------- */

#define K2OS_MAPTYPE_USER_TEXT                      (K2OS_MEMPAGE_ATTR_EXEC | K2OS_MEMPAGE_ATTR_READABLE)
#define K2OS_MAPTYPE_USER_READ                      K2OS_MEMPAGE_ATTR_READABLE
#define K2OS_MAPTYPE_USER_DATA                      (K2OS_MEMPAGE_ATTR_READABLE | K2OS_MEMPAGE_ATTR_WRITEABLE)
#define K2OS_MAPTYPE_USER_DEVICEIO                  (K2OS_MAPTYPE_USER_DATA | K2OS_MEMPAGE_ATTR_UNCACHED | K2OS_MEMPAGE_ATTR_DEVICEIO)
#define K2OS_MAPTYPE_USER_WRITETHRU_CACHED          (K2OS_MAPTYPE_USER_DATA | K2OS_MEMPAGE_ATTR_WRITE_THRU)
#define K2OS_MAPTYPE_USER_UNCACHED                  (K2OS_MAPTYPE_USER_DATA | K2OS_MEMPAGE_ATTR_UNCACHED)

#define K2OS_MAPTYPE_KERN_TEXT                      (K2OS_MEMPAGE_ATTR_KERNEL | K2OS_MAPTYPE_USER_TEXT            )
#define K2OS_MAPTYPE_KERN_READ                      (K2OS_MEMPAGE_ATTR_KERNEL | K2OS_MAPTYPE_USER_READ            )
#define K2OS_MAPTYPE_KERN_DATA                      (K2OS_MEMPAGE_ATTR_KERNEL | K2OS_MAPTYPE_USER_DATA            )
#define K2OS_MAPTYPE_KERN_DEVICEIO                  (K2OS_MEMPAGE_ATTR_KERNEL | K2OS_MAPTYPE_USER_DEVICEIO        )
#define K2OS_MAPTYPE_KERN_WRITETHRU_CACHED          (K2OS_MEMPAGE_ATTR_KERNEL | K2OS_MAPTYPE_USER_WRITETHRU_CACHED)
#define K2OS_MAPTYPE_KERN_UNCACHED                  (K2OS_MEMPAGE_ATTR_KERNEL | K2OS_MAPTYPE_USER_UNCACHED        )

#define K2OS_MAPTYPE_KERN_TRANSITION                (K2OS_MAPTYPE_KERN_UNCACHED | K2OS_MEMPAGE_ATTR_EXEC | K2OS_MEMPAGE_ATTR_TRANSITION)
                                                  
#define K2OS_MAPTYPE_KERN_PAGETABLE                 (K2OS_MAPTYPE_KERN_DATA | K2OS_MEMPAGE_ATTR_PAGING)
#define K2OS_MAPTYPE_KERN_PAGEDIR                   K2OS_MAPTYPE_KERN_PAGETABLE

/* --------------------------------------------------------------------------------- */

//
// must exactly match K2OS_UEFI_LOADINFO in k2osbase.h
//
#define K2OS_UEFI_LOADINFO_OFFSET_MARKER                        0x0000      // 'K2OS'
#define K2OS_UEFI_LOADINFO_OFFSET_EFIST                         0x0004
#define K2OS_UEFI_LOADINFO_OFFSET_SYSVIRTENTRY                  0x0008      // encoded in code bytes in uefi loader. careful
#define K2OS_UEFI_LOADINFO_OFFSET_KERNDLX_ENTRY                 0x000C
#define K2OS_UEFI_LOADINFO_OFFSET_BUILTIN_ROFS_PHYS             0x0010
#define K2OS_UEFI_LOADINFO_OFFSET_DLXCRT                        0x0014
#define K2OS_UEFI_LOADINFO_OFFSET_TRANSBASE_PHYS                0x0018      // encoded in code bytes in uefi loader. careful
#define K2OS_UEFI_LOADINFO_OFFSET_ZEROPAGE_PHYS                 0x001C
#define K2OS_UEFI_LOADINFO_OFFSET_TRANS_PAGE_ADDR               0x0020
#define K2OS_UEFI_LOADINFO_OFFSET_DEBUG_PAGE_VIRT               0x0024
#define K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_SIZE                   0x0028
#define K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_DESC_SIZE              0x002C 
#define K2OS_UEFI_LOADINFO_OFFSET_EFIMAP_DESC_VER               0x0030
#define K2OS_UEFI_LOADINFO_OFFSET_CPU_CORE_COUNT                0x0034
#define K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGES_PHYS              0x0038
#define K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGES_VIRT              0x003C
#define K2OS_UEFI_LOADINFO_OFFSET_FWTAB_PAGE_COUNT              0x0040
#define K2OS_UEFI_LOADINFO_OFFSET_FWFACS_PHYS                   0x0044
#define K2OS_UEFI_LOADINFO_OFFSET_XFWFACS_PHYS                  0x0048
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_FRAMEBUFFERPHYS      0x004C
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_FRAMEBUFFERBYTES     0x0050
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_MODE_VERSION         0x0054
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_HORIZONTALRESOLUTION 0x0058
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_VERTICALRESOLUTION   0x005C
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_PIXELFORMATID        0x0060
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_PIXELREDMASK         0x0064
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_PIXELGREENMASK       0x0068
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_PIXELBLUEMASK        0x006C
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_PIXELRESERVEDMASK    0x0070
#define K2OS_UEFI_LOADINFO_OFFSET_BOOTGRAF_PIXELSPERSCANLINE    0x0074
#define K2OS_UEFI_LOADINFO_OFFSET_CPU_INFO                      0x0078

//
// UEFI 2.5 Spec Page 152, EFI_BOOT_SERVICES.AllocatePages
//
// "MemoryType values in the range 0x80000000..0xFFFFFFFF are reserved for use by UEFI OS loaders
// that are provided by operating system vendors."
//
#define K2OS_EFIMEMTYPE_PAGING          0x80000000  //  page directory and page tables
#define K2OS_EFIMEMTYPE_TEXT            0x80000001  //  dlx code
#define K2OS_EFIMEMTYPE_READ            0x80000002  //  dlx read-only
#define K2OS_EFIMEMTYPE_DATA            0x80000003  //  dlx read/write
#define K2OS_EFIMEMTYPE_PROC0           0x80000004  //  process 0 default memory 
#define K2OS_EFIMEMTYPE_DLX             0x80000005  //  loader DLX page
#define K2OS_EFIMEMTYPE_LOADER          0x80000006  //  loader master page
#define K2OS_EFIMEMTYPE_ZERO            0x80000007  //  one and only zero page
#define K2OS_EFIMEMTYPE_CORES           0x80000008  //  one page per core
#define K2OS_EFIMEMTYPE_PHYS_TRACK      0x80000009  //  used for tracking physical memory
#define K2OS_EFIMEMTYPE_TRANSITION      0x8000000A  //  one and only transition page
#define K2OS_EFIMEMTYPE_EFI_MAP         0x8000000B  //  EFI memory map
#define K2OS_EFIMEMTYPE_FW_TABLES       0x8000000C  //  Firmware ACPI and SMBIOS tables
#define K2OS_EFIMEMTYPE_ARCH_SPEC       0x8000000D  //  Architecture Specific reservation
#define K2OS_EFIMEMTYPE_BUILTIN         0x8000000E  //  Builtin Read Only File System (ROFS)
#define K2OS_EFIMEMTYPE_LAST            K2OS_EFIMEMTYPE_BUILTIN

/* --------------------------------------------------------------------------------- */

#define K2OS_PROC_PAGECOUNT                         4
#define K2OS_EFIMAP_PAGECOUNT                       2

#define K2OS_MAX_CACHELINE_BYTES                    64
#define K2OS_CACHELINE_ALIGNED_BUFFER_MAX           128

#define K2OS_X32_ARCH_PAGES_BASE                    0
#define K2OS_X32_APSTART_TRANSIT_PAGE_ADDR          0x00001000
#define K2OS_X32_APSTART_PAGEDIR_ADDR               0x00002000
#define K2OS_X32_APSTART_PAGETABLE_ADDR             0x00003000
#define K2OS_X32_ARCH_PAGES_COUNT                   4
#define K2OS_X32_MAX_IOAPICS_COUNT                  4

#define K2OS_A32_APSTART_SPACE_PHYS_PAGECOUNT       10  

// 0xFFFF0000
#define K2OS_KVA_X32_LOCAPIC                        K2OS_KVA_ARCHSPEC_BASE
#define K2OS_KVA_X32_LOCAPIC_SIZE                   K2_VA32_MEMPAGE_BYTES
// 0xFFFF1000
#define K2OS_KVA_X32_IOAPICS                        (K2OS_KVA_X32_LOCAPIC + K2OS_KVA_X32_LOCAPIC_SIZE)
#define K2OS_KVA_X32_IOAPICS_SIZE                   (K2_VA32_MEMPAGE_BYTES * K2OS_X32_MAX_IOAPICS_COUNT)
// 0xFFFF2000
#define K2OS_KVA_X32_HPET                           (K2OS_KVA_X32_IOAPICS + K2OS_KVA_X32_IOAPICS_SIZE)
#define K2OS_KVA_X32_HPET_SIZE                      K2_VA32_MEMPAGE_BYTES
// 0xFFFF3000
#define K2OS_KVA_X32_AP_TRANSITION                  (K2OS_KVA_X32_HPET + K2OS_KVA_X32_HPET_SIZE)
#define K2OS_KVA_X32_AP_TRANSITION_SIZE             K2_VA32_MEMPAGE_BYTES
// 0xFFFF4000
#define K2OS_KVA_X32_AP_PAGEDIR                     (K2OS_KVA_X32_AP_TRANSITION + K2OS_KVA_X32_AP_TRANSITION_SIZE)
#define K2OS_KVA_X32_AP_PAGEDIR_SIZE                K2_VA32_MEMPAGE_BYTES
// 0xFFFF5000
#define K2OS_KVA_X32_AP_PAGETABLE                   (K2OS_KVA_X32_AP_PAGEDIR + K2OS_KVA_X32_AP_PAGEDIR_SIZE)
#define K2OS_KVA_X32_AP_PAGETABLE_SIZE              K2_VA32_MEMPAGE_BYTES
// 0xFFFF6000
#define K2OS_KVA_X32_ARCHSPEC_END                   (K2OS_KVA_X32_AP_PAGETABLE + K2OS_KVA_X32_AP_PAGETABLE_SIZE)

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
#define K2OS_KVA_A32_AP_CPUINFO                     (K2OS_KVA_A32_AP_TRANSIT + K2OS_KVA_A32_AP_TRANSIT_SIZE)
#define K2OS_KVA_A32_AP_CPUINFO_SIZE                K2_VA32_MEMPAGE_BYTES
// 0xFFFFB000
#define K2OS_KVA_A32_UNCACHED_SYNC                  (K2OS_KVA_A32_AP_CPUINFO + K2OS_KVA_A32_AP_CPUINFO_SIZE)
#define K2OS_KVA_A32_UNCACHED_SYNC_SIZE             K2_VA32_MEMPAGE_BYTES
// 0xFFFFC000
#define K2OS_KVA_A32_PL310                          (K2OS_KVA_A32_UNCACHED_SYNC + K2OS_KVA_A32_UNCACHED_SYNC_SIZE)
#define K2OS_KVA_A32_PL310_SIZE                     K2_VA32_MEMPAGE_BYTES
// 0xFFFFD000 
#define K2OS_KVA_A32_ARCHSPEC_END                   (K2OS_KVA_A32_PL310 + K2OS_KVA_A32_PL310_SIZE)

#define K2OS_THREAD_ENV_TLS_COUNT                   16

#define K2OS_THREADPRIO_HIGHEST                     0
#define K2OS_THREADPRIO_CRITICAL                    1
#define K2OS_THREADPRIO_INTERRUPT                   2
#define K2OS_THREADPRIO_ABOVE_NORMAL                3
#define K2OS_THREADPRIO_NORMAL                      4
#define K2OS_THREADPRIO_BELOW_NORMAL                5
#define K2OS_THREADPRIO_LAZY                        6
#define K2OS_THREADPRIO_IDLE                        7
#define K2OS_THREADPRIO_LEVELS                      8

#define K2OS_THREAD_DEFAULT_QUANTUM                 10
#define K2OS_THREAD_INFINITE_QUANTUM                0xFFFFFFFF
#define K2OS_THREAD_DEFAULT_STACK_PAGES             8

#define K2OS_FILE_MAX_NAME_BUF_SIZE                 256
//
// Kernel Virtual Address Range (KVA) top down
//
#define K2OS_KVA_PUBLICAPI_SIZE                     0x00001000
#define K2OS_KVA_PUBLICAPI_BASE                     0xFFFFF000

#define K2OS_KVA_FACS_SIZE                          0x00001000
#define K2OS_KVA_FACS_BASE                          0xFFFFE000

#define K2OS_KVA_XFACS_SIZE                         0x00001000
#define K2OS_KVA_XFACS_BASE                         0xFFFFD000

#define K2OS_KVA_ARCHSPEC_SIZE                      0x0000D000
#define K2OS_KVA_ARCHSPEC_BASE                      0xFFFF0000

#define K2OS_KVA_COREPAGES_SIZE                     0x00008000
#define K2OS_KVA_COREPAGES_BASE                     0xFFFE8000
#define K2OS_KVA_COREPAGES_BASE_HIGH                0xFFFE
#define K2OS_KVA_COREPAGES_BASE_LOW                 0x8000

#define K2OS_KVA_CORECLEARPAGES_SIZE                0x00008000
#define K2OS_KVA_CORECLEARPAGES_BASE                0xFFFE0000

#define K2OS_KVA_THREAD0_STACK_LOW_GUARD            0xFFFD0000
#define K2OS_KVA_FREE_TOP                           K2OS_KVA_THREAD0_STACK_LOW_GUARD

//
// Max CPU count is defined by # of pages in corepages area
//
#define K2OS_MAX_CPU_COUNT                          (K2OS_KVA_COREPAGES_SIZE >> K2_VA32_MEMPAGE_BYTES_POW2)   // strange cant use / in asm

//
// size of physical per-page tracking structure
//
#define K2OS_PHYSTRACK_BYTES                        (4 * 4)

//
// Kernel Virtual Address Range (KVA) bottom up
//
#define K2OS_KVA_KERN_BASE                          0x80000000             
#define K2OS_KVA_KERN_SELFMAP_ADDR                  0x80000000  // virtual address of PTE mapping itself

#define K2OS_KVA_KERNVAMAP_BASE                     K2OS_KVA_KERN_BASE
#define K2OS_KVA_KERNVAMAP_SIZE                     (K2_VA32_PAGEFRAMES_FOR_4G * 4)

#define K2OS_KVA_PHYSTRACKAREA_BASE                 (K2OS_KVA_KERNVAMAP_BASE + K2OS_KVA_KERNVAMAP_SIZE)
#define K2OS_KVA_PHYSTRACKAREA_SIZE                 (K2_VA32_PAGEFRAMES_FOR_4G * K2OS_PHYSTRACK_BYTES)
#define K2OS_KVA_PHYSTRACKAREA_END                  (K2OS_KVA_PHYSTRACKAREA_BASE + K2OS_KVA_PHYSTRACKAREA_SIZE)

//
// translation table base must be aligned on 16KB boundary on A32
//
#define K2OS_KVA_TRANSTAB_BASE                      K2OS_KVA_PHYSTRACKAREA_END

#define K2OS_KVA_EFIMAP_BASE                        (K2OS_KVA_TRANSTAB_BASE + K2_VA32_TRANSTAB_SIZE)
#define K2OS_KVA_EFIMAP_SIZE                        (K2OS_EFIMAP_PAGECOUNT * K2_VA32_MEMPAGE_BYTES)

#define K2OS_KVA_PTPAGECOUNT_BASE                   (K2OS_KVA_EFIMAP_BASE + K2OS_KVA_EFIMAP_SIZE)

#define K2OS_KVA_LOADERPAGE_BASE                    (K2OS_KVA_PTPAGECOUNT_BASE + K2_VA32_MEMPAGE_BYTES)

#define K2OS_KVA_PROC0_BASE                         (K2OS_KVA_LOADERPAGE_BASE + K2_VA32_MEMPAGE_BYTES)
#define K2OS_KVA_PROC0_SIZE                         (K2OS_PROC_PAGECOUNT * K2_VA32_MEMPAGE_BYTES)

#define K2OS_KVA_FREE_BOTTOM                        (K2OS_KVA_PROC0_BASE + K2OS_KVA_PROC0_SIZE)


/* --------------------------------------------------------------------------------- */

//
// derived definitions
//

#define K2OS_KVA_CORE0_STACK_INIT                   (K2OS_KVA_COREPAGES_BASE + K2_VA32_MEMPAGE_BYTES - 4)

#define K2OS_KVA_THREAD0_AREA_HIGH                  K2OS_KVA_CORECLEARPAGES_BASE
#define K2OS_KVA_THREAD0_AREA_LOW                   (K2OS_KVA_THREAD0_STACK_LOW_GUARD + K2_VA32_MEMPAGE_BYTES)
#define K2OS_KVA_THREAD0_PHYS_BYTES                 (K2OS_KVA_THREAD0_AREA_HIGH - K2OS_KVA_THREAD0_AREA_LOW)

#define K2OS_KVA_TO_PTE_ADDR(x)                     K2_VA32_TO_PTE_ADDR(K2OS_KVA_KERNVAMAP_BASE, (x))
#define K2OS_KVA_TO_PT_ADDR(x)                      K2_VA32_TO_PT_ADDR(K2OS_KVA_KERNVAMAP_BASE, (x))

#define K2OS_PHYS32_TO_PHYSTRACK(x)                 (K2OS_KVA_PHYSTRACKAREA_BASE + (((x) >> K2_VA32_MEMPAGE_BYTES_POW2) * K2OS_PHYSTRACK_BYTES))
#define K2OS_PHYSTRACK_TO_PHYS32(x)                 ((((x) - K2OS_KVA_PHYSTRACKAREA_BASE) / K2OS_PHYSTRACK_BYTES) << K2_VA32_MEMPAGE_BYTES_POW2)


/* --------------------------------------------------------------------------------- */


#endif // __K2OSDEFS_INC