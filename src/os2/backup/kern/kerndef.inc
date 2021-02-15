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

#ifndef __KERNDEF_INC
#define __KERNDEF_INC

#include "k2osdefs.inc"

/* --------------------------------------------------------------------------------- */

#if K2_TARGET_ARCH_IS_ARM
#define K2OSKERN_PDE_PRESENT_BIT            A32_TTBE_PT_PRESENT
#define K2OSKERN_PTE_PRESENT_BIT            A32_PTE_PRESENT
#define K2OSKERN_PTE_NP_BIT                 0x00000001
#else
#define K2OSKERN_PDE_PRESENT_BIT            X32_PDE_PRESENT
#define K2OSKERN_PTE_PRESENT_BIT            X32_PTE_PRESENT
#define K2OSKERN_PTE_NP_BIT                 0x00000002
#endif
#define K2OSKERN_PTE_NP_TYPE_MASK           0x0000000C
#define K2OSKERN_PTE_NP_TYPE_1              0x0
#define K2OSKERN_PTE_NP_TYPE_2              0x4
#define K2OSKERN_PTE_NP_TYPE_3              0x8
#define K2OSKERN_PTE_NP_TYPE_4              0xC
#define K2OSKERN_PTE_NP_CONTENT_MASK        0xFFFFFFF0    // 256M possible entries

#define K2OSKERN_PTE_NP_1                   (K2OSKERN_PTE_NP_TYPE_1 | K2OSKERN_PTE_NP_BIT)
#define K2OSKERN_PTE_NP_2                   (K2OSKERN_PTE_NP_TYPE_2 | K2OSKERN_PTE_NP_BIT)
#define K2OSKERN_PTE_NP_3                   (K2OSKERN_PTE_NP_TYPE_3 | K2OSKERN_PTE_NP_BIT)
#define K2OSKERN_PTE_NP_4                   (K2OSKERN_PTE_NP_TYPE_4 | K2OSKERN_PTE_NP_BIT)

/* --------------------------------------------------------------------------------- */

#endif // __KERNDEF_INC