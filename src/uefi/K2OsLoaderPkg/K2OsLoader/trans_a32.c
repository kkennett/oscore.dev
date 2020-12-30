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

static UINT32 const sTransitionCode[] =
{
    0xe1a0000f  , //      mov     r0, pc
    0xe3c000ff  , //      bic     r0, r0, #255; 0xff
    0xe3c00c0f  , //      bic     r0, r0, #3840; 0xf00
    0xe2800b02  , //      add     r0, r0, #2048; 0x800

    0xee102fb0  , //      mrc     15, 0, r2, cr0, cr0, {5}
    0xe2022103  , //      and r2, r2, # - 1073741824; 0xc0000000

    0xe5905018  , //      ldr     r5,[r0, #24]

    0xe3520102  , //      cmp     r2, # - 2147483648; 0x80000000
    0x03855040  , //      orreq   r5, r5, #64; 0x40
    0x03855002  , //      orreq   r5, r5, #2
    0x13855001  , //      orrne   r5, r5, #1
    0xe3855008  , //      orr     r5, r5, #8
    0xf57ff04f  , //      dsb     sy
    0xee025f10  , //      mcr     15, 0, r5, cr2, cr0, {0}
    0xee025f30  , //      mcr     15, 0, r5, cr2, cr0, {1}
    0xe3a06001  , //      mov     r6, #1
    0xf57ff04f  , //      dsb     sy
    0xee026f50  , //      mcr     15, 0, r6, cr2, cr0, {2}
    0xf57ff06f  , //      isb     sy

    0xee113f10  , //      mrc     15, 0, r3, cr1, cr0, {0}
    0xe3c33202  , //      bic     r3, r3, #536870912; 0x20000000
    0xe3c33201  , //      bic     r3, r3, #268435456; 0x10000000
    0xee013f10  , //      mcr     15, 0, r3, cr1, cr0, {0}
    0xf57ff04f  , //      dsb     sy
    0xf57ff06f  , //      isb     sy

    0xe3a03000  , //      mov     r3, #0
    0xee083f17  , //      mcr     15, 0, r3, cr8, cr7, {0}
    0xf57ff06f  , //      isb     sy

    0xe3a03001  , //      mov     r3, #1
    0xee033f10  , //      mcr     15, 0, r3, cr3, cr0, {0}
    0xf57ff06f  , //      isb     sy

    0xe3a07000  , //      mov     r7, #0
    0xee077f15  , //      mcr     15, 0, r7, cr7, cr5, {0}
    0xf57ff06f  , //      isb     sy
    0xee077fd5  , //      mcr     15, 0, r7, cr7, cr5, {6}
    0xf57ff06f  , //      isb     sy

    0xe5901008  , //      ldr     r1,[r0, #8]
    0xe28f4020  , //      add     r4, pc, #32
    0xe1a0f004  , //      mov     pc, r4
    0xe320f000  , //      nop     {0}
    0xe320f000  , //      nop     {0}
    0xe320f000  , //      nop     {0}
    0xe320f000  , //      nop     {0}
    0xe320f000  , //      nop     {0}
    0xe320f000  , //      nop     {0}
    0xe320f000  , //      nop     {0}
    0xe320f000  , //      nop     {0}
    0xe1a0f001  , //      mov     pc, r1 
};

typedef void(*pfEntry)(void);
        
void Loader_Arch_Transition(void)
{
    // Interrupts should have been disabled by the GIC driver via ExitBootServices

    //
    // copy transition page code into transition page
    //
    K2MEM_Copy((void *)gData.LoadInfo.mTransitionPageAddr, sTransitionCode, sizeof(sTransitionCode));

    //
    // executing physically with caches disabled. jump into direct-mapped page
    // to set up the basics and jump into the kernel core DLX entrypoint at its 
    // virtual entrypoint address
    //
    ((pfEntry)gData.LoadInfo.mTransitionPageAddr)();

    while (1);
}

