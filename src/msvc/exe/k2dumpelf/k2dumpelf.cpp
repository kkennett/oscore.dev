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
#include <lib/k2win32.h>
#include <lib/k2elf32.h>

static
void
K2_CALLCONV_REGS
myAssert(
    char const *    apFile,
    int             aLineNum,
    char const *    apCondition
)
{	
    DebugBreak();
}

extern "C" K2_pf_ASSERT K2_Assert = myAssert;

int main(int argc, char **argv)
{
    int ix;

    if (argc < 2)
    {
        printf("\nk2dumpelf: Need an Arg\n\n");
        return -1;
    }

    K2ReadOnlyMappedFile *pFile = K2ReadOnlyMappedFile::Create(argv[1]);
    if (pFile == NULL)
    {
        printf("\nk2dumpelf: could not map in file \"%s\"\n", argv[1]);
        return -2;
    }

	K2ELF32PARSE parse;
    K2STAT stat = K2ELF32_Parse((UINT8 const *)pFile->DataPtr(), pFile->FileBytes(), &parse);
    if (!K2STAT_IS_ERROR(stat))
    {
		UINT32 sizeText = 0;
		UINT32 sizeRead = 0;
		UINT32 sizeData = 0;
		UINT32 align;
		K2Elf32File *pElfFile;
        stat = K2Elf32File::Create(&parse, &pElfFile);
        if (!K2STAT_IS_ERROR(stat))
        {
            for (ix = 0;ix < pElfFile->Header().e_shnum;ix++)
            {
                Elf32_Shdr const &secHdr = pElfFile->Section(ix).Header();
                if (!(secHdr.sh_flags & SHF_ALLOC))
                    continue;

                align = (((UINT32)1) << secHdr.sh_addralign);

                if (secHdr.sh_flags & SHF_EXECINSTR)
                {
                    sizeText = ((sizeText + align - 1) / align) * align;
                    sizeText += secHdr.sh_size;
                }
                else if (secHdr.sh_flags & SHF_WRITE)
                {
                    sizeData = ((sizeData + align - 1) / align) * align;
                    sizeData += secHdr.sh_size;
                }
                else
                {
                    sizeRead = ((sizeRead + align - 1) / align) * align;
                    sizeRead += secHdr.sh_size;
                }
            }
            printf("x %08I32X r %08I32X d %08I32X\n", sizeText, sizeRead, sizeData);
        }
        else
        {
            printf("\nk2dumpelf: error %08X parsing elf file \"%s\"\n", stat, argv[1]);
        }
    }
    else
    {
        printf("Error %d (%08X) parsing file\n", stat, stat);
    }

    pFile->Release();

    return stat;
}