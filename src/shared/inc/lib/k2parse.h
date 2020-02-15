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
#ifndef __K2PARSE_H
#define __K2PARSE_H

#include <k2systype.h>

//
//------------------------------------------------------------------------
//

#ifdef __cplusplus
extern "C" {
#endif
    
void 
K2PARSE_EatToEOL(
    char const **   appPars, 
    UINT32 *        apLeft
    );

void
K2PARSE_EatEOL(
    char const **   appPars,
    UINT32 *        apLeft
    );

void
K2PARSE_EatWhitespace(
    char const **   appPars,
    UINT32 *        apLeft
    );

void
K2PARSE_EatToChar(
    char const **   appPars,
    UINT32 *        apLeft,
    char            aCh
    );

void
K2PARSE_EatWhitespaceAtEnd(
    char const *    apPars,
    UINT32 *        apLineLen
    );

void
K2PARSE_EatLine(
    char const **   appPars,
    UINT32 *        apLeft,
    char const **   apRetLine,
    UINT32 *         apRetLineLen
    );

void
K2PARSE_EatWord(
    char const **   appPars,
    UINT32 *        apLeft,
    char const **   apRetWord,
    UINT32 *        apRetWordLen
    );

void
K2PARSE_Token(
    char const **   appPars,
    UINT32 *        apLeft,
    char const **   apRetToken,
    UINT32 *        apRetTokLen
    );

BOOL
K2PARSE_Guid128(
    char const *    apText,
    UINT32          aLen,
    K2_GUID128 *    apRetGuid
    );

#ifdef __cplusplus
};  // extern "C"
#endif

//
//------------------------------------------------------------------------
//

#endif  // __K2PARSE_H

