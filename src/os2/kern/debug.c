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

#include "kern.h"

static
void
sEmitter(
    void *  apContext,
    char    aCh
)
{
    KernHal_DebugOut(aCh);
}

UINT32 
KernDbg_OutputWithArgs(
    char const *apFormat, 
    VALIST      aList
)
{
    UINT32  result;

    KernSeqLock_Lock(&gData.DebugSeqLock);
    
    result = K2ASC_Emitf(sEmitter, NULL, (UINT32)-1, apFormat, aList);
    
    KernSeqLock_Unlock(&gData.DebugSeqLock);

    return result;
}

UINT32
KernDbg_Output(
    char const *apFormat,
    ...
)
{
    VALIST  vList;
    UINT32  result;

    K2_VASTART(vList, apFormat);

    result = KernDbg_OutputWithArgs(apFormat, vList);

    K2_VAEND(vList);

    return result;
}

UINT32 
KernDbg_FindClosestSymbol(
    K2OSKERN_OBJ_PROCESS *  apCurProc, 
    UINT32                  aAddr, 
    char *                  apRetSymName, 
    UINT32                  aRetSymNameBufLen
)
{
    return 0;
}
