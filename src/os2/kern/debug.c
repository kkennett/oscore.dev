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

static UINT32               sgSymCount = 0;
static Elf32_Sym const *    sgpSym = NULL;
static char const *         sgpSymNames = NULL;

static
void
sEmitter(
    void *  apContext,
    char    aCh
)
{
    K2OSHAL_DebugOut(aCh);
}

UINT32 
KernDbg_OutputWithArgs(
    char const *apFormat, 
    VALIST      aList
)
{
    UINT32  result;

    K2OSKERN_SeqLock(&gData.DebugSeqLock);
    
    result = K2ASC_Emitf(sEmitter, NULL, (UINT32)-1, apFormat, aList);
    
    K2OSKERN_SeqUnlock(&gData.DebugSeqLock);

    return result;
}

UINT32
K2OSKERN_Debug(
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

void
KernDbg_FindClosestSymbol(
    K2OSKERN_OBJ_PROCESS *  apCurProc, 
    UINT32                  aAddr, 
    char *                  apRetSymName, 
    UINT32                  aRetSymNameBufLen
)
{
    UINT32 b, e, m, v;

    if ((NULL == apRetSymName) ||
        (0 == aRetSymNameBufLen))
        return;

    if (aAddr >= K2OS_KVA_KERN_BASE)
    {
        if ((0 == sgSymCount) ||
            (aAddr < sgpSym[0].st_value))
        {
            if (aRetSymNameBufLen > 0)
                *apRetSymName = 0;
            return;
        }

        //
        // address comes at or after first symbol in table
        //
        b = 0;
        e = sgSymCount;
        do
        {
            m = (b + e) / 2;
            v = sgpSym[m].st_value;
            if (v == aAddr)
                break;
            if (v > aAddr)
                e = m;
            else
                b = m + 1;
        } while (b < e);
        if (aAddr < v)
        {
            // whoa back up.  which we can do since we know
            // that the address comes at or after the first symbol in the table
            v = sgpSym[--m].st_value;
        }
        K2ASC_PrintfLen(apRetSymName, aRetSymNameBufLen,
            "%s+0x%X",
            sgpSymNames + sgpSym[m].st_name, 
            aAddr - v);
    }
    else
    {

    }
}

void
KernDbg_EarlyInit(
    void
)
{
    K2STAT stat;
    K2ELF32PARSE parse;
    UINT32 numSec;
    UINT32 ix;
    UINT32 strSecIx;
    Elf32_Shdr const *pSecHdr;

    K2OSKERN_SeqInit(&gData.DebugSeqLock);

    stat = K2ELF32_Parse((UINT8 const *)K2OS_KVA_KERNEL_PREALLOC, gData.LoadInfo.mKernSizeBytes, &parse);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    numSec = parse.mpRawFileData->e_shnum;
    K2_ASSERT(1 < numSec);
    for (ix = 0; ix < numSec; ix++)
    {
        pSecHdr = K2ELF32_GetSectionHeader(&parse, ix);
        if (pSecHdr->sh_type == SHT_SYMTAB)
            break;
    }
    if (ix < numSec)
    {
        //
        // we found a symbol table
        //
        if (numSec > pSecHdr->sh_link)
        {
            strSecIx = pSecHdr->sh_link;
            pSecHdr = K2ELF32_GetSectionHeader(&parse, strSecIx);
            if (pSecHdr->sh_type == SHT_STRTAB)
            {
                //
                // and it has a valid string table link
                //
                sgpSymNames = K2ELF32_GetSectionData(&parse, strSecIx);
                pSecHdr = K2ELF32_GetSectionHeader(&parse, ix);
                sgSymCount = pSecHdr->sh_size / pSecHdr->sh_entsize;
                sgpSym = (Elf32_Sym const *)K2ELF32_GetSectionData(&parse, ix);

                //
                // ignore symbols below kernel arena
                //
                while (K2OS_KVA_KERNEL_PREALLOC > sgpSym->st_value)
                {
                    sgpSym++;
                    if (--sgSymCount == 0)
                        break;
                }

                //
                // ignore symbols after kernel arena
                //
                while (0 < sgSymCount)
                {
                    if (K2OS_KVA_KERNEL_PREALLOC_END > sgpSym[sgSymCount - 1].st_value)
                        break;
                    sgSymCount--;
                }
            }
            else
            {
                K2OSKERN_Debug("***Found kernel symbol table, but string table index points to wrong section\n");
            }
        }
        else
        {
            K2OSKERN_Debug("***Found kernel symbol table, but string table index is invalid\n");
        }
    }
    else
    {
        K2OSKERN_Debug("***No Kernel Symbol Table Found\n");
    }
}