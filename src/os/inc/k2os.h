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
#ifndef __K2OS_H
#define __K2OS_H

#include "k2osbase.h"

#if __cplusplus
extern "C" {
#endif

//
//------------------------------------------------------------------------
//

UINT32  K2_CALLCONV_CALLERCLEANS K2OS_SysGetInfo(K2OS_SYSINFO *apRetInfo);

typedef UINT64 (K2_CALLCONV_REGS *K2OS_pf_SysUpTimeMs)(void);
UINT64  K2_CALLCONV_REGS         K2OS_SysUpTimeMs(void);

BOOL    K2_CALLCONV_CALLERCLEANS K2OS_SysGetProperty(UINT32 aPropertyId, void *apRetValue, UINT32 aValueBufferBytes);

//
//------------------------------------------------------------------------
//

UINT32  K2_CALLCONV_CALLERCLEANS K2OS_DebugPrint(char const *apString);
BOOL    K2_CALLCONV_CALLERCLEANS K2OS_DebugPresent(void);
void    K2_CALLCONV_CALLERCLEANS K2OS_DebugBreak(void);

//
//------------------------------------------------------------------------
//

BOOL    K2_CALLCONV_CALLERCLEANS K2OS_AddrIsReadable(void *apAddr, UINT32 aLengthBytes);
BOOL    K2_CALLCONV_CALLERCLEANS K2OS_AddrIsWriteable(void *apAddr, UINT32 aLengthBytes);
BOOL    K2_CALLCONV_CALLERCLEANS K2OS_AddrIsExecutable(void *apAddr);

//
//------------------------------------------------------------------------
//

typedef enum _K2OS_ObjectType K2OS_ObjectType;
enum _K2OS_ObjectType
{
    K2OS_Obj_None = 0,
    K2OS_Obj_Segment = 1,
    K2OS_Obj_DLX = 2,
    K2OS_Obj_Name = 3,
    K2OS_Obj_Process = 4,
    K2OS_Obj_Thread = 5,
    K2OS_Obj_Event = 6,
    K2OS_Obj_Alarm = 7,
    K2OS_Obj_Semaphore = 8,
    K2OS_Obj_Mailbox = 9,
    K2OS_Obj_Msg = 10,
    K2OS_Obj_Interrupt = 11,
    K2OS_Obj_Service = 12,
    K2OS_Obj_Publish = 13,
    K2OS_Obj_Notify = 14,
    K2OS_Obj_Subscrip = 15,

    K2OS_Obj_Ext_Base = 128
};

BOOL K2_CALLCONV_CALLERCLEANS K2OS_TokenDestroy(K2OS_TOKEN aToken);

//
//------------------------------------------------------------------------
//

#define K2OS_CRITSEC_BYTES  K2OS_CACHELINE_ALIGNED_BUFFER_MAX

typedef struct _K2OS_CRITSEC K2OS_CRITSEC;
struct K2_ALIGN_ATTRIB(K2OS_MAX_CACHELINE_BYTES) _K2OS_CRITSEC
{
    UINT8 mOpaque[K2OS_CRITSEC_BYTES];
};

typedef BOOL (K2_CALLCONV_CALLERCLEANS *K2OS_pf_CritSecInit)(K2OS_CRITSEC *apSec);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecInit(K2OS_CRITSEC *apSec);

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecTryEnter(K2OS_CRITSEC *apSec);

typedef BOOL (K2_CALLCONV_CALLERCLEANS *K2OS_pf_CritSecEnter)(K2OS_CRITSEC *apSec);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecEnter(K2OS_CRITSEC *apSec);

typedef BOOL (K2_CALLCONV_CALLERCLEANS *K2OS_pf_CritSecLeave)(K2OS_CRITSEC *apSec);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecLeave(K2OS_CRITSEC *apSec);

BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecDone(K2OS_CRITSEC *apSec);

//
//------------------------------------------------------------------------
//
#define K2OS_NAME_MAX_LEN   47

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_NameDefine(char const *apName);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_NameString(K2OS_TOKEN aNameToken, char *apRetName, UINT32 aBufferBytes);
K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_NameAcquire(K2OS_TOKEN aObjectToken);

//
//------------------------------------------------------------------------
//

typedef enum _K2OS_ThreadState K2OS_ThreadState;
enum _K2OS_ThreadState
{
    K2OS_ThreadState_Ready,
    K2OS_ThreadState_Running,
    K2OS_ThreadState_Waiting,
    K2OS_ThreadState_Stopped,
    K2OS_ThreadState_Dead
};

#define K2OS_THREADATTR_PRIORITY            1
#define K2OS_THREADATTR_AFFINITYMASK        2
#define K2OS_THREADATTR_QUANTUM             4
#define K2OS_THREADATTR_VALID_MASK          (0x7)

typedef struct _K2OS_THREADATTR K2OS_THREADATTR;
struct _K2OS_THREADATTR
{
    UINT32 mFieldMask;
    UINT32 mPriority;
    UINT32 mAffinityMask;
    UINT32 mQuantum;
};

typedef struct _K2OS_THREADCREATE K2OS_THREADCREATE;
struct _K2OS_THREADCREATE
{
    UINT32                  mStructBytes;
    K2OS_pf_THREAD_ENTRY    mEntrypoint;
    void *                  mpArg;
    UINT32                  mStackPages;
    K2OS_THREADATTR         Attr;
};

typedef struct _K2OS_THREADINFO K2OS_THREADINFO;
struct _K2OS_THREADINFO
{
    UINT32              mStructBytes;
    UINT32              mThreadId;
    UINT32              mProcessId;
    UINT32              mLastStatus;
    K2OS_ThreadState    mState;
    UINT32              mExitCode;
    UINT32              mPauseCount;
    UINT32              mTotalRunTimeMs;
    K2OS_THREADCREATE   CreateInfo;
};

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ThreadAcquireByName(K2OS_TOKEN aNameToken);
K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ThreadAcquireById(UINT32 aThreadId);

UINT32      K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnId(void);

K2STAT      K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetStatus(void);
void        K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetStatus(K2STAT aStatus);

BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadAllocLocal(UINT32 *apRetSlotId);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadFreeLocal(UINT32 aSlotId);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetLocal(UINT32 aSlotId, UINT32 aValue);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetLocal(UINT32 aSlotId, UINT32 *apRetValue);

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ThreadCreate(K2OS_THREADCREATE const *apCreate);

BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetInfo(K2OS_TOKEN aThreadToken, K2OS_THREADINFO *apRetInfo);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnInfo(K2OS_THREADINFO *apRetInfo);

BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadPause(K2OS_TOKEN aThreadToken, UINT32 *apRetPauseCount, UINT32 aTimeoutMs);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadResume(K2OS_TOKEN aThreadToken, UINT32 *apRetPauseCount);

void        K2_CALLCONV_CALLERCLEANS K2OS_ThreadExit(UINT32 aExitCode);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadKill(K2OS_TOKEN aThreadToken, UINT32 aForcedExitCode);

BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetAttr(K2OS_TOKEN aThreadToken, K2OS_THREADATTR const *apAttr);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetAttr(K2OS_TOKEN aThreadToken, K2OS_THREADATTR *apRetAttr);

BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetOwnAttr(K2OS_THREADATTR const *apInfo);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnAttr(K2OS_THREADATTR *apRetInfo);

BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetPauseLock(BOOL aSetLock);

void        K2_CALLCONV_CALLERCLEANS K2OS_ThreadSleep(UINT32 aMilliseconds);

#define K2OS_TIMEOUT_INFINITE   ((UINT32)-1)

#define K2OS_WAIT_MAX_TOKENS    64
#define K2OS_WAIT_SIGNALLED_0   0
#define K2OS_WAIT_ABANDONED_0   K2OS_WAIT_MAX_TOKENS
#define K2OS_WAIT_SUCCEEDED(x)  ((K2OS_WAIT_SIGNALLED_0 <= (x)) && ((x) < K2OS_WAIT_ABANDONED_0))
#define K2OS_WAIT_ERROR         ((UINT32)-1)

UINT32 K2_CALLCONV_CALLERCLEANS K2OS_ThreadWait(UINT32 aTokenCount, K2OS_TOKEN const *apTokenArray, BOOL aWaitAll, UINT32 aTimeoutMs);

//
//------------------------------------------------------------------------
//

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_EventCreate(K2OS_TOKEN aNameToken, BOOL aAutoReset, BOOL aInitialState);
K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_EventAcquireByName(K2OS_TOKEN aNameToken);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_EventSet(K2OS_TOKEN aEventToken);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_EventReset(K2OS_TOKEN aEventToken);

//
//------------------------------------------------------------------------
//

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_AlarmCreate(K2OS_TOKEN aNameToken, UINT32 aIntervalMs, BOOL aPeriodic);
K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_AlarmAcquireByName(K2OS_TOKEN aNameToken);

//
//------------------------------------------------------------------------
//

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_SemaphoreCreate(K2OS_TOKEN aNameToken, UINT32 aMaxCount, UINT32 aInitCount);
K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_SemaphoreAcquireByName(K2OS_TOKEN aNameToken);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_SemaphoreRelease(K2OS_TOKEN aSemaphoreToken, UINT32 aRelCount, UINT32 *apRetNewCount);

//
//------------------------------------------------------------------------
//

#define K2OS_MSGOPCODE_HAS_RESPONSE     0x80000000
#define K2OS_MSGOPCODE_ID_MASK          0x7FFFFFFF
#define K2OS_MSGOPCODE_NOTIFY(x)        ((x) & K2OS_MSGOPCODE_ID_MASK)
#define K2OS_MSGOPCODE_TRANSACT(x)      ((x) | K2OS_MSGOPCODE_HAS_RESPONSE)

typedef struct _K2OS_MSGIO K2OS_MSGIO;
struct _K2OS_MSGIO
{
    union {
        UINT32 mOpCode;
        UINT32 mStatus;
    };
    UINT32 mPayload[7];
};
K2_STATIC_ASSERT(sizeof(K2OS_MSGIO) == (8 * sizeof(UINT32)));

K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_MailboxCreate(K2OS_TOKEN aTokName, BOOL aInitBlocked);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_MailboxSetBlock(K2OS_TOKEN aTokMailbox, BOOL aBlock);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_MailboxRecv(K2OS_TOKEN aTokMailbox, K2OS_MSGIO *apRetMsgIo, UINT32 *apRetRequestId);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_MailboxRespond(K2OS_TOKEN aTokMailbox, UINT32 aRequestId, K2OS_MSGIO const *apRespIo);

BOOL        K2_CALLCONV_CALLERCLEANS K2OS_MsgCreate(UINT32 aMsgCount, K2OS_TOKEN *apRetTokMsgs);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_MsgSend(K2OS_TOKEN aTokMailboxName, K2OS_TOKEN aTokMsg, K2OS_MSGIO const *apMsgIo);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_MsgAbort(K2OS_TOKEN aTokMsg, BOOL aClear);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_MsgReadResponse(K2OS_TOKEN aTokMsg, K2OS_MSGIO *apRetRespIo, BOOL aClear);

//
//------------------------------------------------------------------------
//

typedef struct _K2OS_HEAP_STATE K2OS_HEAP_STATE;
struct _K2OS_HEAP_STATE
{
    UINT32  mAllocCount;
    UINT32  mTotalAlloc;
    UINT32  mTotalOverhead;
    UINT32  mLargestFree;
};

typedef void * (K2_CALLCONV_CALLERCLEANS *K2OS_pf_HeapAlloc)(UINT32 aByteCount);
void * K2_CALLCONV_CALLERCLEANS K2OS_HeapAlloc(UINT32 aByteCount);

typedef BOOL   (K2_CALLCONV_CALLERCLEANS *K2OS_pf_HeapFree)(void *aPtr);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_HeapFree(void *aPtr);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_HeapGetState(K2OS_HEAP_STATE *apRetState);


//
//------------------------------------------------------------------------
//

#define K2OS_VIRTALLOCFLAG_ALSO_COMMIT  0x00000001
#define K2OS_VIRTALLOCFLAG_TOP_DOWN     0x80000000

BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesAlloc(UINT32 *apAddr, UINT32 aPageCount, UINT32 aVirtAllocFlags, UINT32 aPageAttrFlags);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesCommit(UINT32 aPagesAddr, UINT32 aPageCount, UINT32 aPageAttrFlags);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesDecommit(UINT32 aPagesAddr, UINT32 aPageCount);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesSetAttr(UINT32 aPagesAddr, UINT32 aPageCount, UINT32 aPageAttrFlags);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesGetAttr(UINT32 aPagesAddr, UINT32 *apRetPageCount, UINT32 *apRetPagesAttrFlags);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesSetLock(UINT32 aPagesAddr, UINT32 aPageCount, BOOL aSetLocked);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_VirtPagesFree(UINT32 aPagesAllocAddr);

//
//------------------------------------------------------------------------
//

BOOL K2_CALLCONV_CALLERCLEANS K2OS_TimeGet(K2_EARTHTIME *apRetTime);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_TimeSet(K2_EARTHTIME const *apTime);

//
//------------------------------------------------------------------------
//

typedef K2OS_TOKEN K2OS_FS_TOKEN;
typedef K2OS_TOKEN K2OS_DIR_TOKEN;
typedef K2OS_TOKEN K2OS_FILE_TOKEN;
typedef K2OS_TOKEN K2OS_PATH_TOKEN;

#define K2OS_FILE_MODE_READ             0x00000001
#define K2OS_FILE_MODE_WRITE            0x00000002

#define K2OS_FILE_USE_SHARE_READ        0x80000000
#define K2OS_FILE_USE_SHARE_WRITE       0x40000000
#define K2OS_FILE_USE_SHARE_NONE        0x00000000
#define K2OS_FILE_USE_WRITE_THRU        0x08000000
#define K2OS_FILE_USE_NO_CACHE          0x04000000

#define K2OS_FILE_PROP_READ_ONLY        0x00000001
#define K2OS_FILE_PROP_HIDDEN           0x00000002
#define K2OS_FILE_PROP_SYSTEM           0x00000004
#define K2OS_FILE_PROP_RESERVED         0x00000008
#define K2OS_FILE_PROP_DIRECTORY        0x00000010
#define K2OS_FILE_PROP_ARCHIVE          0x00000020
#define K2OS_FILE_PROP_EXECUTABLE       0x00000040
#define K2OS_FILE_PROP_EXISTS           0x80000000
#define K2OS_FILE_PROP_VALID_SET_MASK   0x80000077

#define K2OS_FSPROVINFO_NAME_BUF_COUNT  16

typedef struct _K2OS_FSPROVINFO K2OS_FSPROVINFO;
struct _K2OS_FSPROVINFO
{
    K2_GUID128  mProvId;
    char        ProvName[K2OS_FSPROVINFO_NAME_BUF_COUNT];
    UINT32      mProvVersion;
};

typedef struct _K2OS_FILE_INFO K2OS_FILE_INFO;
struct _K2OS_FILE_INFO
{
    char        FileName[K2OS_FILE_MAX_NAME_BUF_SIZE];
    UINT64      mSizeBytes;
    UINT64      mCreateTime;
    UINT64      mLastAccessTime;
    UINT64      mLastWriteTime;
    UINT32      mProp;
};

BOOL            K2_CALLCONV_CALLERCLEANS K2OS_FsVolEnum(UINT32 *apInOutCount, K2_GUID128 *apRetVolIds);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_FsVolGetProv(K2_GUID128 const *apVolId, K2OS_FSPROVINFO *apRetProvInfo);
K2OS_DIR_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_FsVolAcquireRootDir(K2_GUID128 const *apVolId);

BOOL            K2_CALLCONV_CALLERCLEANS K2OS_DirGetVolId(K2OS_DIR_TOKEN aTokDir, K2_GUID128 *apRetVolId);
K2OS_DIR_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_DirAcquireSub(K2OS_DIR_TOKEN aTokDir, char const *apRelPath);
K2OS_DIR_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_DirCreateSub(K2OS_DIR_TOKEN aTokDir, char const *apRelPath, BOOL aForce);
K2OS_DIR_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_DirAcquireParent(K2OS_DIR_TOKEN aTokDir);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_DirDeleteSub(K2OS_DIR_TOKEN aTokDir, char const *apRelPath);

K2OS_FILE_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_DirAcquireFile(K2OS_DIR_TOKEN aTokDir, char const *apRelPath, UINT32 aModeAndUse);
K2OS_FILE_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_DirCreateFile(K2OS_DIR_TOKEN aTokDir, char const *apRelPath, UINT64 aUseAndProp, BOOL aForce);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_DirGetFileInfo(K2OS_DIR_TOKEN aTokDir, char const *apRelPath, K2OS_FILE_INFO *apRetInfo);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_DirSetFileProp(K2OS_DIR_TOKEN aTokDir, char const *apRelPath, UINT32 aNewProp);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_DirGetName(K2OS_DIR_TOKEN aTokDir, char *apRetNameBuf, UINT32 *apIoBufLen);

BOOL            K2_CALLCONV_CALLERCLEANS K2OS_FileRead(K2OS_FILE_TOKEN aTokFile, UINT32 * apSizeIo, void *apBuffer);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_FileWrite(K2OS_FILE_TOKEN aTokFile, UINT32 aSizeIo, void const *apBuffer);
UINT64          K2_CALLCONV_CALLERCLEANS K2OS_FileSize(K2OS_FILE_TOKEN aTokFile);
UINT64          K2_CALLCONV_CALLERCLEANS K2OS_FileGetPos(K2OS_FILE_TOKEN aTokFile);
UINT64          K2_CALLCONV_CALLERCLEANS K2OS_FileSetPos(K2OS_FILE_TOKEN aTokFile, UINT64 aNewPos);
void            K2_CALLCONV_CALLERCLEANS K2OS_FileFlush(K2OS_FILE_TOKEN aTokFile);
UINT32          K2_CALLCONV_CALLERCLEANS K2OS_FileGetProp(K2OS_FILE_TOKEN aTokFile);
K2OS_DIR_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_FileAcquireParentDir(K2OS_FILE_TOKEN aTokFile);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_FileGetInfo(K2OS_FILE_TOKEN aTokFile, K2OS_FILE_INFO *apRetFileInfo);

K2OS_PATH_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_PathCreate(K2OS_TOKEN aTokNewPathName);
K2OS_PATH_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_PathCreateCopy(K2OS_TOKEN aTokNewPathName, K2OS_PATH_TOKEN aTokSource);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_PathAddDir(K2OS_PATH_TOKEN aTokPath, K2OS_DIR_TOKEN aTokDir);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_PathDelDir(K2OS_PATH_TOKEN aTokPath, K2OS_DIR_TOKEN aTokDir);
K2OS_PATH_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_PathAcquireByName(K2OS_TOKEN aTokName);

//
//------------------------------------------------------------------------
//

K2OS_TOKEN      K2_CALLCONV_CALLERCLEANS K2OS_DlxLoad(K2OS_PATH_TOKEN aTokPath, char const *apRelFilePath, K2_GUID128 const *apMatchId);
K2OS_FILE_TOKEN K2_CALLCONV_CALLERCLEANS K2OS_DlxAcquireFile(K2OS_TOKEN aTokDlx);
BOOL            K2_CALLCONV_CALLERCLEANS K2OS_DlxGetId(K2OS_TOKEN aTokDlx, K2_GUID128 * apRetId);
void *          K2_CALLCONV_CALLERCLEANS K2OS_DlxFindExport(K2OS_TOKEN aTokDlx, UINT32 aDlxSeg, char const *apExportName);
K2OS_TOKEN      K2_CALLCONV_CALLERCLEANS K2OS_DlxAcquireAddressOwner(UINT32 aAddress, UINT32 *apRetSegment);

//
//------------------------------------------------------------------------
//

typedef struct _K2OS_PROCESSCREATE K2OS_PROCESSCREATE;
struct _K2OS_PROCESSCREATE
{
    UINT32              mStructBytes;
    UINT32              mStackBytes;
    void *              mpThreadArg;
    K2OS_THREADATTR     ThreadAttr;
};

typedef struct _K2OS_IMAGEINFO K2OS_IMAGEINFO;
struct _K2OS_IMAGEINFO
{
    UINT32      mStructBytes;
    K2_GUID128  mFsProvId;
    K2_GUID128  mFsVolumeId;
    char        mFilePath[4];
};

typedef struct _K2OS_PROCESS_CREATED K2OS_PROCESS_CREATED;
struct _K2OS_PROCESS_CREATED
{
    UINT32      mProcessId;
    K2OS_TOKEN  mTokProcess;
    UINT32      mThreadId;
    K2OS_TOKEN  mTokThread;
};

BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ProcessCreate(K2OS_TOKEN aNameToken, K2OS_FILE_TOKEN aDlxFileToken, K2OS_PROCESSCREATE const *apProcessCreate, K2OS_PROCESS_CREATED *apRetCreated);
K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ProcessAcquireByName(K2OS_TOKEN aNameToken);
K2OS_TOKEN  K2_CALLCONV_CALLERCLEANS K2OS_ProcessAcquireById(UINT32 aProcessId);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ProcessGetId(K2OS_TOKEN aProcessToken, UINT32 *apRetId);
UINT32      K2_CALLCONV_CALLERCLEANS K2OS_ProcessGetOwnId(void);
void        K2_CALLCONV_CALLERCLEANS K2OS_ProcessExit(UINT32 aExitCode);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ProcessGetImageInfo(K2OS_TOKEN aProcessToken, K2OS_IMAGEINFO *apRetImageInfo);
BOOL        K2_CALLCONV_CALLERCLEANS K2OS_ProcessGetOwnImageInfo(K2OS_IMAGEINFO *apRetImageInfo);

//
//------------------------------------------------------------------------
//

#if __cplusplus
}
#endif

#endif // __K2OS_H