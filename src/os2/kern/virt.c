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

/*

...
|XXX-USED-XXX|  
+------------+  <<- sgTop - used and mapped above this
|            |
+------------+
|            |  (unused but mappable virtual page frames)
...             
|            |
+------------+  <<- sgTopPt, pagetable map boundary (4MB)
|            |
|            |
...             (unused and unmappable virtual page frames)
|            |
|            |
+------------+  <<- sgBotPt, pagetable map boundary (4MB)
|            |
...             
|            |  (unused but mappable virtual page frames)
+------------+
|            |
+------------+  <<- sgBot - used and mapped below this
|XXX-USED-XXX|  
...

*/

UINT32  sgTop;       // top-down address of where first byte after last free page ENDS
UINT32  sgTopPt;     // top-down address of where first page after last free pagetable ENDS
UINT32  sgBot;       // bottom-up address of where first byte after last free page STARTS
UINT32  sgBotPt;     // bottom-up address of where first byte after last free pagetable STARTS

void
KernVirt_Init(
    void
)
{
    UINT32  chk;
    UINT32 *pPTE;

    sgTop = gData.LoadInfo.mKernArenaHigh;
    sgBot = gData.LoadInfo.mKernArenaLow;

    //
    // verify bottom-up remaining page range is really free (no pages mapped)
    //
    if (0 != (sgBot & (K2_VA32_PAGETABLE_MAP_BYTES - 1)))
    {
        chk = sgBot;
        pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(chk);
        do
        {
            // verify this page is not mapped

            K2_ASSERT(0 == ((*pPTE) & K2OSKERN_PTE_PRESENT_BIT));
            pPTE++;
            chk += K2_VA32_MEMPAGE_BYTES;
        } while (0 != (chk & (K2_VA32_PAGETABLE_MAP_BYTES - 1)));
        sgBotPt = chk;
    }
    else
        sgBotPt = sgBot;

    //
    // verify top-down remaining page range is really free (no pages mapped)
    //
    if (0 != (sgTop & (K2_VA32_PAGETABLE_MAP_BYTES - 1)))
    {
        chk = sgTop;
        pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(chk);
        do
        {
            // verify this page is not mapped
            chk -= K2_VA32_MEMPAGE_BYTES;
            pPTE--;
            K2_ASSERT(0 == ((*pPTE) & K2OSKERN_PTE_PRESENT_BIT));
        } while (0 != (chk & (K2_VA32_PAGETABLE_MAP_BYTES - 1)));
        sgTopPt = chk;
    }
    else
        sgTopPt = sgTop;

    //
    // verify all pagetables between bottom and top pt are not mapped
    //
    if (sgBotPt != sgTopPt)
    {
        chk = sgBotPt;
        pPTE = (UINT32 *)K2OS_KVA_TO_PTE_ADDR(K2OS_KVA_TO_PT_ADDR(chk));
        do
        {
            K2_ASSERT(0 == ((*pPTE) & K2OSKERN_PTE_PRESENT_BIT));
            pPTE++;
            chk += K2_VA32_PAGETABLE_MAP_BYTES;
        } while (chk != sgTopPt);
    }
}