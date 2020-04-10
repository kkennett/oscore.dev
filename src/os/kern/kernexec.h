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

#ifndef __KERNEXEC_H
#define __KERNEXEC_H

#include <k2oshal.h>
#include <lib/k2dlxsupp.h>

/* --------------------------------------------------------------------------------- */

typedef struct _K2OSEXEC_INIT_INFO K2OSEXEC_INIT_INFO;
struct _K2OSEXEC_INIT_INFO
{
    //
    // INPUT
    //
    UINT32  mEfiMapSize;
    UINT32  mEfiMemDescSize;
    UINT32  mEfiMemDescVer;

    //
    // OUTPUT
    //
    K2OSKERN_INTR_CONFIG  SysTickDevIntrConfig;    // 1ms tick interrupt
};

typedef
void
(*K2OSEXEC_pf_Init)(
    K2OSEXEC_INIT_INFO * apInitInfo
    );

void
K2OSEXEC_Init(
    K2OSEXEC_INIT_INFO * apInitInfo
    );

typedef
void
(*K2OSEXEC_pf_Run)(
    void
    );

void
K2OSEXEC_Run(
    void
    );

/* --------------------------------------------------------------------------------- */

#endif // __KERNEXEC_H