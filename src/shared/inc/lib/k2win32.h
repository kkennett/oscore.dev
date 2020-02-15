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
#ifndef __K2WIN32_H
#define __K2WIN32_H

#include <stdio.h>
#include <Windows.h>
#include <k2systype.h>

//
//------------------------------------------------------------------------
//

#ifdef __cplusplus

class K2MappedFile
{
public:
    INT32 AddRef(void);
    INT32 Release(void);

    char const * const FileName(void) const
    {
        return mpFileName; 
    }

    UINT64 const & FileBytes64(void) const
    {
        return mFileBytes;
    }

    UINT32 FileBytes(void) const
    {
        return (UINT32)(mFileBytes & 0xFFFFFFFF);
    }

protected:
    K2MappedFile(char const *apFileName, HANDLE ahFile, HANDLE ahMap, UINT64 aFileBytes, void *apMapped) :
        mhFile(ahFile),
        mhMap(ahMap),
        mFileBytes(aFileBytes),
        mpMapped(apMapped)
    {
        int copyLen = (int)strlen(apFileName);
        mpFileName = new char[copyLen+4];
        strcpy_s(mpFileName,copyLen+1,apFileName);
        mRefs = 1;
    }

    virtual ~K2MappedFile(void)
    {
        UnmapViewOfFile(mpMapped);
        CloseHandle(mhMap);
        CloseHandle(mhFile);
        delete [] mpFileName;
    }

    volatile INT32  mRefs;
    char *          mpFileName;
    HANDLE const    mhFile;
    HANDLE const    mhMap;
    UINT64 const    mFileBytes;
    void * const    mpMapped;
};

class K2ReadOnlyMappedFile : public K2MappedFile
{
public:
    static K2ReadOnlyMappedFile * Create(char const *apFileName);

    void const * DataPtr(void) const
    {
        return mpMapped;
    }
private:
    K2ReadOnlyMappedFile(char const *apFileName, HANDLE ahFile, HANDLE ahMap, UINT64 aFileBytes, void const *apMapped) :
        K2MappedFile(apFileName, ahFile, ahMap, aFileBytes, (void *)apMapped)
    {
    }
};

class K2ReadWriteMappedFile : public K2MappedFile
{
public:
    static K2ReadWriteMappedFile * Create(char const *apFileName);

    void * DataPtr(void) const
    {
        return mpMapped;
    }
private:
    K2ReadWriteMappedFile(char const *apFileName, HANDLE ahFile, HANDLE ahMap, UINT64 aFileBytes, void *apMapped) :
        K2MappedFile(apFileName, ahFile, ahMap, aFileBytes, apMapped)
    {
    }
};

//
//------------------------------------------------------------------------
//

class ArgParser
{
public:
    ArgParser(void);
    ~ArgParser(void);

    bool Init(int cmdArgc, char **cmdArgv);
    void Reset(void);
    void Advance(void);
    bool Left(void);
    char const * Arg(void);

private:
    int         mCmdArgc;
    char **     mpCmdArgv;
    int         mCmdIx;
    void **     mppFiles;
};

extern "C" {
#endif

// c-only defs go here

#ifdef __cplusplus
};  // extern "C"
#endif

//
//------------------------------------------------------------------------
//

#endif // __K2MEM_H
