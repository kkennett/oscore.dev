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

static UINT8 const sTransitionCode[] =
{
    0xe8, 0x00, 0x00, 0x00, 0x00,           // call   5 <address of next instruction>
    0x59,                                   // pop    ecx
    0x81, 0xe1, 0x00, 0xf0, 0xff, 0xff,     // and    ecx,0xfffff000
    0x81, 0xc1, 0x00, 0x08, 0x00, 0x00,     // add    ecx,0x800
    0x8b, 0x41, 0x18,                       // mov    eax,DWORD PTR [ecx+0x18]    :  0x18 is K2_UEFI_LOADINFO_OFFSET_TRANSBASE_PHYS
    0x0f, 0x22, 0xd8,                       // mov    cr3,eax
    0x0f, 0x20, 0xc0,                       // mov    eax,cr0
    0x0d, 0x00, 0x00, 0x00, 0x80,           // or     eax,0x80000000
    0x0d, 0x00, 0x00, 0x01, 0x00,           // or     eax,0x00010000
    0x0f, 0x09,                             // wbinvd
    0x0f, 0x22, 0xc0,                       // mov    cr0,eax
    0x0f, 0x09,                             // wbinvd
    0x0f, 0x20, 0xd8,                       // mov    eax,cr3
    0x0f, 0x22, 0xd8,                       // mov    cr3,eax
    0x8b, 0x41, 0x08,                       // mov    eax,DWORD PTR [ecx+0x8]     : 0x8 is K2OS_UEFI_LOADINFO_OFFSET_SYSVIRTENTRY
    0xff, 0xe0,                             // jmp    eax
    0x00, 0x00, 0x00, 0x00
};

typedef void(*pfEntry)(void);

void Loader_Arch_Transition(void)
{
    // Interrupts should have been disabled by ExitBootServices

    //
    // copy transition page code into transition page
    //
    K2MEM_Copy((void *)gData.LoadInfo.mTransitionPageAddr, sTransitionCode, sizeof(sTransitionCode));

    //
    // jump into the transition page that turns on paging and 
    // jumps into the kernel at its target address
    //
    ((pfEntry)gData.LoadInfo.mTransitionPageAddr)();

    while (1);
}

