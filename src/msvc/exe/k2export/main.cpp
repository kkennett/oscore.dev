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
#include "k2export.h"

OUTCTX gOut;

static
int 
sTreeStringCompare(UINT32 aKey, K2TREE_NODE *apNode)
{
    return K2ASC_Comp((char const *)aKey, (char const *)apNode->mUserVal);
}

int main(int argc, char **argv)
{
    ArgParser parse;

    if (argc < 2)
    {
        printf("*** Need input arguments\n");
        return -1;
    }

    K2MEM_Zero(&gOut, sizeof(gOut));
    K2TREE_Init(&gOut.SymbolTree, sTreeStringCompare);

    if (!parse.Init(argc, argv))
    {
        printf("*** Could not parse command line arguments\n");
        return -1;
    }

    // first pass. ignore things that aren't switches

    // bypass program name
    parse.Advance();

    while (parse.Left())
    {
        char const *pArg = parse.Arg();
        parse.Advance();

        if (*pArg == '-')
        {
            if ((pArg[1] == 0) ||
                (pArg[2] != 0))
            {
                printf("*** Argument error near \"%s\"\n", pArg);
                return -2;
            }
            if (!parse.Left())
            {
                printf("*** Switch '%c' with no argument\n", pArg[1]);
                return -3;
            }
            switch (pArg[1])
            {
            case 'o':
            case 'O':
                if (!SetupOutputFilePath(parse.Arg()))
                    return -4;
                break;

            case 'i':
            case 'I':
                if (!SetupDlxInfFile(parse.Arg()))
                    return -5;
                break;

            case 'l':
            case 'L':
                if (!AddFilePath(parse.Arg()))
                    return -6;
                break;

            default:
                printf("*** Unknown switch '%c' found\n", pArg[1]);
                return -7;
            }
            parse.Advance();
        }
    }

    // if we get here we are ready to start loading files if necessary
    if (NULL == gOut.mpMappedDlxInf)
    {
        printf("No DLX file specified on command line\n");
        return -2;
    }

    // second pass. load files now that we have all the paths
    parse.Reset();

    // bypass program name
    parse.Advance();

    while (parse.Left())
    {
        char const *pArg = parse.Arg();
        parse.Advance();

        if (*pArg == '-')
        {
            // we took care of this already in the first pass
            parse.Advance();
        }
        else
        {
            if (!Export_Load(pArg))
                return -8;
        }
    }

    // if we get here all files are loaded and all global 
    // symbols have been added to the global symbol tree

    return Export();
}

