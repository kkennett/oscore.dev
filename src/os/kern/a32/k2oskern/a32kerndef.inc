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

#ifndef __A32KERNDEF_INC
#define __A32KERNDEF_INC

#include "../../kerndef.inc"

/* --------------------------------------------------------------------------------- */

#define A32KERN_SIZEOF_KERNEL_EXCEPTION_CONTEXT             (18 * 4)

#define A32KERN_CORE_IRQ_STACK_BYTES                        1152
#define A32KERN_CORE_ABT_STACK_BYTES                        128
#define A32KERN_CORE_UND_STACK_BYTES                        128
#define A32KERN_CORE_SVC_STACK_BYTES                        128

#define A32KERN_EXCEPTION_REASON_UNDEFINED_INSTRUCTION      1
#define A32KERN_EXCEPTION_REASON_SYSTEM_CALL                2
#define A32KERN_EXCEPTION_REASON_PREFETCH_ABORT             3
#define A32KERN_EXCEPTION_REASON_DATA_ABORT                 4
#define A32KERN_EXCEPTION_REASON_IRQ_RESCHED                5
#define A32KERN_EXCEPTION_REASON_RAISE_EXCEPTION            6

//
// TTB must be 16KB aligned physically. best to do it virtually too
//
// vector page is at 0xFFFF0000
//
#define A32KERN_APSTART_PAGETABLE_VIRT                      K2OS_KVA_A32_AP_PAGETABLE
#define A32KERN_APSTART_TRANSIT_PAGE_VIRT                   K2OS_KVA_A32_AP_TRANSIT
#define A32KERN_APSTART_CPUINFO_PAGE_VIRT                   K2OS_KVA_A32_AP_CPUINFO
#define A32KERN_APSTART_TTB_VIRT                            K2OS_KVA_A32_AP_TTB
#define A32KERN_APSTART_UNCACHED_PAGE_VIRT                  K2OS_KVA_A32_UNCACHED_SYNC

#define A32KERN_UP_GICD_PAGE_VIRT                           K2OS_KVA_A32_GICD
#define A32KERN_UP_GICC_PAGE_VIRT                           K2OS_KVA_A32_GICC

#define A32KERN_MP_CONFIGBASE_MAP_VIRT                      K2OS_KVA_A32_PERIPHBASE
#define A32KERN_MP_SCU_VIRT                                 (A32KERN_MP_CONFIGBASE_MAP_VIRT + A32_PERIPH_OFFSET_SCU)
#define A32KERN_MP_GICC_VIRT                                (A32KERN_MP_CONFIGBASE_MAP_VIRT + A32_PERIPH_OFFSET_GICC)
#define A32KERN_MP_GLOBAL_TIMER_VIRT                        (A32KERN_MP_CONFIGBASE_MAP_VIRT + A32_PERIPH_OFFSET_GLOBAL_TIMER)
#define A32KERN_MP_PRIVATE_TIMERS_VIRT                      (A32KERN_MP_CONFIGBASE_MAP_VIRT + A32_PERIPH_OFFSET_PRIVATE_TIMERS)
#define A32KERN_MP_GICD_VIRT                                (A32KERN_MP_CONFIGBASE_MAP_VIRT + A32_PERIPH_OFFSET_GICD)

/* --------------------------------------------------------------------------------- */

#endif // __A32KERNDEF_INC
