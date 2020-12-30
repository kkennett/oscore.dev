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
#include <k2asma32.inc>
#include "..\..\..\os\inc\k2osdefs.inc"

/* --------------------------------------------------------------------------------- */

BEGIN_A32_PROC(A32_Transition)
      //
      // put address of data into r0, which is halfway into 
      // the page this code is executing out of
      //
     mov r0, pc
     bic r0, r0, #0xFF
     bic r0, r0, #0xF00
     add r0, r0, #0x800

      // get mpidr to r2 and mask off all but MP bits
     mrc p15, 0, r2, c0, c0, 5
     and r2, r2, #0xC0000000

      // get TT base register physical address
     ldr r5, [r0, #K2OS_UEFI_LOADINFO_OFFSET_TRANSBASE_PHYS]

      // set TTB configuration information (Multicore or not Multicore)
     cmp r2, #0x80000000

       // set Multicore TTB memory configuration
     orreq r5, r5, #0x40   // IRGN bits (set 01b Inner write-back write-allocate cacheable)
     orreq r5, r5, #0x02   // S bit     (set Shareable)

      // set Single-core TTB memory configuration
     orrne r5, r5, #0x01   // C bit     (set Inner Cacheable)

      // set RGN bits (outer write-through cacheable)
     orr   r5, r5, #0x08   // RGN bits  (set 01b Outer write-back write-allocate cacheable)

      // set TTBR and CR
     dsb
     mcr p15, 0, r5, c2, c0, 0   // set TTBR 0
     mcr p15, 0, r5, c2, c0, 1   // set TTBR 1
     mov r6, #1
     dsb
     mcr p15, 0, r6, c2, c0, 2   // set TTBCR to 1
     isb

      // turn off access flag and TEX remap
     mrc p15, 0, r3, c1, c0, 0   
     bic r3, r3, #A32_SCTRL_AFE_ACCESSFLAGENABLE  // make sure turn off AFE (B3.6.1 in TRM)
     bic r3, r3, #A32_SCTRL_TRE_TEXREMAPENABLE    // make sure turn off TEX remap
     mcr p15, 0, r3, c1, c0, 0   
     dsb
     isb

      // invalidate all TLBs
     mov r3, #0
     mcr p15, 0, r3, c8, c7, 0
     isb

      // set domain access control register so that domain 0 has client access
      // and all others have no access
     mov r3, #1
     mcr p15, 0, r3, c3, c0, 0 
     isb

      // invalidate i cache
     mov r7, #0
     mcr p15, 0, r7, c7, c5, 0
     isb

      // invalidate branch predictor
     mcr p15, 0, r7, c7, c5, 6       
     isb 

      //
      // load virtual entrypoint to r4
      // and system entrypoint to r1
      //
     ldr r1, [r0, #K2OS_UEFI_LOADINFO_OFFSET_SYSVIRTENTRY]
     adr r4, _JUMP_TO_KERNEL

      // jump over 32 bytes to flush the pipeline
     mov pc, r4
     nop
     nop
     nop
     nop
     nop
     nop
     nop
     nop

_JUMP_TO_KERNEL:
    // never coming back from here
    mov pc, r1
END_A32_PROC(A32_Transition)


/* --------------------------------------------------------------------------------- */

    .end

