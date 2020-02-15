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

enum InfSection
{
    ExpNone=0,
    ExpDLX,
    ExpCode,
    ExpData
};

static InfSection sgInfSection;

#define _K2PARSE_Token(p1,pn1,p2,pn2)     K2PARSE_Token((const char **)(p1), pn1, (const char **)(p2), pn2)
#define _K2PARSE_EatWhitespace(p1,pn1)    K2PARSE_EatWhitespace((const char **)(p1),pn1)
#define _K2PARSE_EatLine(p1,pn1,p2,pn2)   K2PARSE_EatLine((const char **)(p1), pn1, (const char **)(p2), pn2)

static
void
sInsertList(
    UINT32          aLineNum,
    EXPORT_SPEC **  appList,
    EXPORT_SPEC *   apIns
    )
{
    int             comp;
    EXPORT_SPEC *   pPrev;
    EXPORT_SPEC *   pLook;

    apIns->mpNext = NULL;
    pLook = *appList;
    if (pLook == NULL)
    {
        *appList = apIns;
        return;
    }

    pPrev = NULL;
    do
    {
        comp = K2ASC_Comp(apIns->mpName, pLook->mpName);
        if (comp == 0)
        {
            printf("!!! Duplicated export on line %d\n", aLineNum);
            return;  // duplicated symbol in export list
        }

        if (comp < 0)
        {
            apIns->mpNext = pLook;
            if (pPrev == NULL)
                *appList = apIns;
            else
                pPrev->mpNext = apIns;
            return;
        }

        pPrev = pLook;
        pLook = pLook->mpNext;

    } while (pLook != NULL);

    pPrev->mpNext = apIns;
}

static
bool
sSectionDLX(
    UINT32  aLineNumber,
    char *  apLine,
    UINT32  aLineLen
    )
{
    char *  pToken;
    UINT32  tokLen;

    _K2PARSE_Token(&apLine, &aLineLen, &pToken, &tokLen);
    if (tokLen == 0)
    {
        printf("*** Syntax error in dlx inf file on line %d\n", aLineNumber);
        return false;
    }
    if (tokLen == 2)
    {
        if (0 == K2ASC_CompInsLen("ID", pToken, 2))
        {
            _K2PARSE_EatWhitespace(&apLine, &aLineLen);
            if ((aLineLen < 38) || (*apLine != '{') || (apLine[aLineLen-1] != '}'))
            {
                printf("*** Expected GUID but found other on line %d of dlx inf\n", aLineNumber);
                return false;
            }
            apLine ++;
            aLineLen -= 2;
            if (!K2PARSE_Guid128(apLine, aLineLen, &gOut.DlxInfo.ID))
            {
                printf("*** Failed to parse GUID on line %d of dlx inf\n", aLineNumber);
                return false;
            }
            return true;
        }
    }

    printf("*** Unknown token \"%.*s\" in [DLX] section of dlx inf on line %d\n", tokLen, pToken, aLineNumber);

    return false;
}

static
bool
sSectionExport(
    UINT32  aLineNumber,
    char *  apLine,
    UINT32  aLineLen,
    bool    aIsCode
    )
{
    char *          pToken;
    UINT32           tokLen;
    EXPORT_SPEC *   pSpec;

    _K2PARSE_Token(&apLine, &aLineLen, &pToken, &tokLen);
    if (tokLen == 0)
    {
        printf("*** Syntax error in dlx inf file on line %d\n", aLineNumber);
        return false;
    }

    _K2PARSE_EatWhitespace(&apLine, &aLineLen);
    if (aLineLen != 0)
    {
        printf("*** Garbage after export on line %d\n", aLineNumber);
        return false;
    }

    pToken[tokLen] = 0;

    pSpec = new EXPORT_SPEC;
    if (pSpec == NULL)
    {
        printf("*** Memory allocation failed\n");
        return false;
    }
    K2MEM_Zero(pSpec, sizeof(EXPORT_SPEC));

    pSpec->mpName = pToken;
    pSpec->mpNext = NULL;

    if (aIsCode)
    {
        sInsertList(aLineNumber, &gOut.mOutSec[OUTSEC_CODE].mpSpec, pSpec);
        gOut.mOutSec[OUTSEC_CODE].mCount++;
    }
    else
    {
        sInsertList(aLineNumber, &gOut.mOutSec[OUTSEC_DATA].mpSpec, pSpec);
        gOut.mOutSec[OUTSEC_DATA].mCount++;
    }

    gOut.mTotalExports++;

    return true;
}

static
bool
sParseDlxInfLine(
    UINT32  aLineNumber,
    char *  apLine,
    UINT32  aLineLen
    )
{
    char *  pToken;
    UINT32   tokLen;

    if (apLine[0] == '[')
    {
        apLine++;
        aLineLen--;
        _K2PARSE_Token(&apLine, &aLineLen, &pToken, &tokLen);
        if (tokLen == 0)
        {
            printf("*** Cannot parse section type in dlx inf file at line %d\n", aLineNumber);
            return false;
        }
        _K2PARSE_EatWhitespace(&apLine, &aLineLen);
        if ((aLineLen == 0) || (*apLine != ']'))
        {
            printf("*** Unterminated section type in dlx inf file at line %d\n", aLineNumber);
            return false;
        }
        apLine++;
        aLineLen--;
        _K2PARSE_EatWhitespace(&apLine, &aLineLen);
        if (aLineLen != 0)
        {
            printf("*** Export def file has garbage at end of line %d\n", aLineNumber);
            return false;
        }
        if ((tokLen == 3) && (0 == K2ASC_CompInsLen(pToken, "DLX", 3)))
        {
            sgInfSection = ExpDLX;
            return true;
        }
        if (tokLen == 4)
        {
            if (0 == K2ASC_CompInsLen(pToken, "CODE", 4))
            {
                sgInfSection = ExpCode;
                return true;
            }
            else if (0 == K2ASC_CompInsLen(pToken, "DATA", 4))
            {
                sgInfSection = ExpData;
                return true;
            }
        }
        printf("*** Unknown inf section type \"%.*s\" at line %d\n", tokLen, pToken, aLineNumber);
        return false;
    }

    if (sgInfSection == ExpNone)
    {
        printf("*** No inf section at start of dlx inf file\n");
        return false;
    }

    switch (sgInfSection)
    {
    case ExpDLX:
        return sSectionDLX(aLineNumber, apLine, aLineLen);
    case ExpCode:
        return sSectionExport(aLineNumber, apLine, aLineLen, true);
    case ExpData:
        return sSectionExport(aLineNumber, apLine, aLineLen, false);
    default:
        break;
    }
    return false;
}

static
bool
sParseDlxInf(
    void
    )
{
    char *    pPars;
    UINT32    left;
    char *    pLine;
    UINT32    lineLen;
    UINT32    lineNumber;

    pPars = (char *)gOut.mpMappedDlxInf->DataPtr();
    left = gOut.mpMappedDlxInf->FileBytes();

    if (left == 0)
        return true;

    sgInfSection = ExpNone;
    lineNumber = 1;
    do 
    {
        _K2PARSE_EatLine(&pPars, &left, &pLine, &lineLen);
        _K2PARSE_EatWhitespace(&pLine, &lineLen);
        K2PARSE_EatWhitespaceAtEnd(pLine, &lineLen);
        if ((lineLen != 0) && (*pLine != '#'))
        {
            if (!sParseDlxInfLine(lineNumber, pLine, lineLen))
                return false;
        }
        lineNumber++;
    } while (left != 0);

//    printf("%d Code Exports\n", gOut.mOutSec[OUTSEC_CODE].mCount);
//    printf("%d Data Exports\n", gOut.mOutSec[OUTSEC_DATA].mCount);

    return true;
}

bool
SetupDlxInfFile(
    char const *apArgument
    )
{
    DWORD   outPathLen;
    char *  pDlxInfFilePath;
    
    outPathLen = GetFullPathName(apArgument, 0, NULL, NULL);
    if (outPathLen == 0)
    {
        printf("*** Could not parse dlx inf file argument \"%s\"\n", apArgument);
        return false;
    }

    pDlxInfFilePath = new char[(outPathLen + 4)&~3];

    if (pDlxInfFilePath == NULL)
    {
        printf("*** Memory allocation failed.\n");
        return false;
    }

    if (0 == GetFullPathName(apArgument, outPathLen + 1, pDlxInfFilePath, NULL))
    {
        // weird
        delete[] pDlxInfFilePath;
        printf("*** Error creating dlx inf file path.\n");
        return false;
    }

    gOut.mpMappedDlxInf = K2ReadWriteMappedFile::Create(pDlxInfFilePath);

    delete[] pDlxInfFilePath;

    if (gOut.mpMappedDlxInf == NULL)
    {
        printf("*** Error mapping in dlx inf file \"%s\"\n", pDlxInfFilePath);
        return false;
    }

    return sParseDlxInf();
}
