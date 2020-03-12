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
    K2OS_Obj_RwLock = 9,
    K2OS_Obj_Mailbox = 10,
    K2OS_Obj_Mailslot = 11,
    K2OS_Obj_Msg = 12,
    K2OS_Obj_RwLockReq = 13
};

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

typedef BOOL (K2_CALLCONV_CALLERCLEANS *K2OS_pf_CritSecEnter)(K2OS_CRITSEC *apSec);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecEnter(K2OS_CRITSEC *apSec);

typedef BOOL (K2_CALLCONV_CALLERCLEANS *K2OS_pf_CritSecLeave)(K2OS_CRITSEC *apSec);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_CritSecLeave(K2OS_CRITSEC *apSec);

//
//------------------------------------------------------------------------
//

typedef UINT64 (K2_CALLCONV_REGS *K2OS_pf_GetAbsTimeMs)(void);
UINT64 K2_CALLCONV_REGS K2OS_GetAbsTimeMs(void);

//
//------------------------------------------------------------------------
//

typedef void * (K2_CALLCONV_CALLERCLEANS *K2OS_pf_HeapAlloc)(UINT32 aByteCount);
void * K2_CALLCONV_CALLERCLEANS K2OS_HeapAlloc(UINT32 aByteCount);

typedef BOOL   (K2_CALLCONV_CALLERCLEANS *K2OS_pf_HeapFree)(void *aPtr);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_HeapFree(void *aPtr);

//
//------------------------------------------------------------------------
//

#define K2OS_NAME_MAX_LEN   47

BOOL K2_CALLCONV_CALLERCLEANS K2OS_NameDefine(char const *apName, K2OS_TOKEN *apRetNameToken);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_NameString(K2OS_TOKEN aNameToken, char *apRetName, UINT32 aBufferBytes);
BOOL K2_CALLCONV_CALLERCLEANS K2OS_NameAcquire(K2OS_TOKEN aObjectToken, K2OS_TOKEN *apRetNameToken);

//
//------------------------------------------------------------------------
//

typedef enum _K2OS_ThreadState K2OS_ThreadState;
enum _K2OS_ThreadState
{
    K2OS_Thread_Running = 0,
    K2OS_Thread_Ready,
    K2OS_Thread_Exited,
    K2OS_Thread_Killed,
    K2OS_Thread_Paused,
    K2OS_Thread_Blocked
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
    UINT32              mExitCode;
    K2OS_ThreadState    mState;
    UINT32              mPauseCount;
    UINT32              mTotalRunTimeMs;
    K2OS_THREADCREATE   CreateInfo;
};

UINT32 K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnId(void);

K2STAT K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetStatus(void);
void   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetStatus(K2STAT aStatus);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadAllocLocal(UINT32 *apRetSlotId);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadFreeLocal(UINT32 aSlotId);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetLocal(UINT32 aSlotId, UINT32 aValue);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetLocal(UINT32 aSlotId, UINT32 *apRetValue);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadCreate(K2OS_THREADCREATE const *apCreate, K2OS_TOKEN *apRetThreadToken);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetInfo(K2OS_TOKEN aThreadToken, K2OS_THREADINFO *apRetInfo);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnInfo(K2OS_THREADINFO *apRetInfo);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadAcquire(UINT32 aThreadId, K2OS_TOKEN *apRetThreadToken);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadPause(K2OS_TOKEN aThreadToken, UINT32 *apRetPauseCount, UINT32 aTimeoutMs);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadResume(K2OS_TOKEN aThreadToken, UINT32 *apRetPauseCount);

void   K2_CALLCONV_CALLERCLEANS K2OS_ThreadExit(UINT32 aExitCode);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadKill(K2OS_TOKEN aThreadToken, UINT32 aForcedExitCode);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetAttr(K2OS_TOKEN aThreadToken, K2OS_THREADATTR const *apAttr);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetAttr(K2OS_TOKEN aThreadToken, K2OS_THREADATTR *apRetAttr);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetOwnAttr(K2OS_THREADATTR const *apInfo);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadGetOwnAttr(K2OS_THREADATTR *apRetInfo);

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSetPauseLock(BOOL aSetLock);

void   K2_CALLCONV_CALLERCLEANS K2OS_ThreadSleep(UINT32 aMilliseconds);

#define K2OS_TIMEOUT_INFINITE   ((UINT32)-1)

#define K2OS_WAIT_MAX_TOKENS    64
#define K2OS_WAIT_TIMEOUT       ((UINT32)-2)
#define K2OS_WAIT_ERROR         ((UINT32)-1)
#define K2OS_WAIT_SIGNALLED_0   0
#define K2OS_WAIT_ABANDONED_0   K2OS_WAIT_MAX_TOKENS
#define K2OS_WAIT_SUCCEEDED(x)  ((K2OS_WAIT_SIGNALLED_0 <= (x)) && ((x) < K2OS_WAIT_ABANDONED_0))

BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitOne(K2OS_TOKEN aToken, UINT32 aTimeoutMs);
UINT32 K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitAny(UINT32 aTokenCount, K2OS_TOKEN *apTokenArray, UINT32 aTimeoutMs);
BOOL   K2_CALLCONV_CALLERCLEANS K2OS_ThreadWaitAll(UINT32 aTokenCount, K2OS_TOKEN *apTokenArray, UINT32 aTimeoutMs);

//
//------------------------------------------------------------------------
//

#define K2OS_VIRTALLOCFLAG_ALSO_COMMIT  0x00000001
#define K2OS_VIRTALLOCFLAG_TOP_DOWN     0x80000000

#define K2OS_VIRTCOMMIT_NOACCESS        0
#define K2OS_VIRTCOMMIT_EXEC            K2OS_ACCESS_ATTR_X
#define K2OS_VIRTCOMMIT_READ            K2OS_ACCESS_ATTR_R
#define K2OS_VIRTCOMMIT_READWRITE       K2OS_ACCESS_ATTR_W
#define K2OS_VIRTCOMMIT_NOCACHE         0x80000000
#define K2OS_VIRTCOMMIT_WRITECOMBINE    0x40000000
#define K2OS_VIRTCOMMIT_GUARD           0x20000000

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

#endif // __K2OS_H