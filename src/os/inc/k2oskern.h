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
#ifndef __K2OSKERN_H
#define __K2OSKERN_H

#include "k2os.h"

#if __cplusplus
extern "C" {
#endif

//
//------------------------------------------------------------------------
//

struct _K2OSKERN_SEQLOCK
{
    UINT32 volatile mSeqIn;
    UINT32 volatile mSeqOut;
};
typedef struct _K2OSKERN_SEQLOCK K2OSKERN_SEQLOCK;

//
//------------------------------------------------------------------------
//

typedef
UINT32
(*K2OSKERN_pf_Debug)(
    char const *apFormat,
    ...
);

UINT32
K2OSKERN_Debug(
    char const *apFormat,
    ...
);

//
//------------------------------------------------------------------------
//

typedef
void
(*K2OSKERN_pf_Panic)(
    char const *apFormat,
    ...
);

void
K2OSKERN_Panic(
    char const *apFormat,
    ...
);

//
//------------------------------------------------------------------------
//

typedef
void
(K2_CALLCONV_REGS *K2OSKERN_pf_MicroStall)(
    UINT32      aMicroseconds
    );

void
K2_CALLCONV_REGS
K2OSKERN_MicroStall(
    UINT32      aMicroseconds
);

//
//------------------------------------------------------------------------
//

typedef
BOOL
(K2_CALLCONV_REGS *K2OSKERN_pf_SetIntr)(
    BOOL aEnable
);

BOOL
K2_CALLCONV_REGS
K2OSKERN_SetIntr(
    BOOL aEnable
);

typedef
BOOL
(K2_CALLCONV_REGS *K2OSKERN_pf_GetIntr)(
    void
    );

BOOL
K2_CALLCONV_REGS
K2OSKERN_GetIntr(
    void
    );

//
//------------------------------------------------------------------------
//

typedef
void
(K2_CALLCONV_REGS *K2OSKERN_pf_SeqIntrInit)(
    K2OSKERN_SEQLOCK *  apLock
);

void
K2_CALLCONV_REGS
K2OSKERN_SeqIntrInit(
    K2OSKERN_SEQLOCK *  apLock
);

//
//------------------------------------------------------------------------
//

typedef 
BOOL
(K2_CALLCONV_REGS *K2OSKERN_pf_SeqIntrLock)(
    K2OSKERN_SEQLOCK *  apLock
);

BOOL
K2_CALLCONV_REGS
K2OSKERN_SeqIntrLock(
    K2OSKERN_SEQLOCK *  apLock
);

//
//------------------------------------------------------------------------
//

typedef 
void
(K2_CALLCONV_REGS *K2OSKERN_pf_SeqIntrUnlock)(
    K2OSKERN_SEQLOCK *  apLock,
    BOOL                aDisp
);

void
K2_CALLCONV_REGS
K2OSKERN_SeqIntrUnlock(
    K2OSKERN_SEQLOCK *  apLock,
    BOOL                aDisp
);

//
//------------------------------------------------------------------------
//

typedef 
UINT32
(K2_CALLCONV_REGS *K2OSKERN_pf_GetCpuIndex)(
    void
);

UINT32
K2_CALLCONV_REGS
K2OSKERN_GetCpuIndex(
    void
);

//
//------------------------------------------------------------------------
//

typedef
K2STAT
(*K2OSKERN_pf_MapDevice)(
    UINT32      aPhysDeviceAddr,
    UINT32      aPageCount,
    UINT32 *    apRetVirtAddr
    );

K2STAT
K2OSKERN_MapDevice(
    UINT32      aPhysDeviceAddr,
    UINT32      aPageCount,
    UINT32 *    apRetVirtAddr
);

typedef
K2STAT
(*K2OSKERN_pf_UnmapDevice)(
    UINT32      aVirtDeviceAddr
    );

K2STAT
K2OSKERN_UnmapDevice(
    UINT32      aVirtDeviceAddr
);

//
//------------------------------------------------------------------------
//

struct _K2OSKERN_IRQ_LINE_CONFIG
{
    BOOL    mIsEdgeTriggered;
    BOOL    mIsActiveLow;
    BOOL    mShareConfig;
    BOOL    mWakeConfig;
};
typedef struct _K2OSKERN_IRQ_LINE_CONFIG K2OSKERN_IRQ_LINE_CONFIG;

struct _K2OSKERN_IRQ_CONFIG
{
    UINT32                      mSourceIrq;
    UINT32                      mDestCoreIx;
    K2OSKERN_IRQ_LINE_CONFIG    Line;
};
typedef struct _K2OSKERN_IRQ_CONFIG K2OSKERN_IRQ_CONFIG;

//
// calling convention must match ACPI_SYSTEM_XFACE
//
typedef UINT32 (*K2OSKERN_pf_IntrHandler)(void *apContext);

typedef
K2STAT
(*K2OSKERN_pf_InstallIntrHandler)(
    K2OSKERN_IRQ_CONFIG const * apConfig,
    K2OSKERN_pf_IntrHandler     aHandler,
    void *                      apContext,
    K2OS_TOKEN *                apRetTokIntr
    );

K2STAT
K2OSKERN_InstallIntrHandler(
    K2OSKERN_IRQ_CONFIG const * apConfig,
    K2OSKERN_pf_IntrHandler     aHandler,
    void *                      apContext,
    K2OS_TOKEN *                apRetTokIntr
);

typedef 
K2STAT
(*K2OSKERN_pf_SetIntrMask)(
    K2OS_TOKEN  aTokIntr,
    BOOL        aMask
);

K2STAT
K2OSKERN_SetIntrMask(
    K2OS_TOKEN  aTokIntr,
    BOOL        aMask
);

//
//------------------------------------------------------------------------
//

typedef
K2STAT
(*K2OSKERN_pf_Esc)(
    UINT32  aEscCode,
    void *  apParam
    );

K2STAT
K2OSKERN_Esc(
    UINT32  aEscCode,
    void *  apParam
);

#define K2OSKERN_ESC_GET_DEBUGPAGE  1

//
//------------------------------------------------------------------------
//

extern K2_CACHEINFO const * gpK2OSKERN_CacheInfo;

//
//------------------------------------------------------------------------
//

#define K2OS_SYSPROP_ID_KERN_FWTABLES_PHYS_ADDR     0x80000000
#define K2OS_SYSPROP_ID_KERN_FWTABLES_VIRT_ADDR     0x80000001
#define K2OS_SYSPROP_ID_KERN_FWTABLES_BYTES         0x80000002
#define K2OS_SYSPROP_ID_KERN_FWFACS_PHYS_ADDR       0x80000003
#define K2OS_SYSPROP_ID_KERN_XFWFACS_PHYS_ADDR      0x80000004

//
//------------------------------------------------------------------------
//

#if K2_TARGET_ARCH_IS_INTEL

#define X32_DEVIRQ_LVT_BASE     80 
#define X32_DEVIRQ_LVT_CMCI     (X32_DEVIRQ_LVT_BASE + 0)
#define X32_DEVIRQ_LVT_TIMER    (X32_DEVIRQ_LVT_BASE + 1)
#define X32_DEVIRQ_LVT_THERM    (X32_DEVIRQ_LVT_BASE + 2)
#define X32_DEVIRQ_LVT_PERF     (X32_DEVIRQ_LVT_BASE + 3)
#define X32_DEVIRQ_LVT_LINT0    (X32_DEVIRQ_LVT_BASE + 4)
#define X32_DEVIRQ_LVT_LINT1    (X32_DEVIRQ_LVT_BASE + 5)
#define X32_DEVIRQ_LVT_ERROR    (X32_DEVIRQ_LVT_BASE + 6)
#define X32_DEVIRQ_LVT_LAST     X32_DEVIRQ_LVT_ERROR
#define X32_DEVIRQ_MAX_COUNT    (X32_DEVIRQ_LVT_LAST + 1)

#endif

//
//------------------------------------------------------------------------
//

typedef
void
(*K2OSKERN_pf_ReflectNotify)(
    UINT32          aOpCode,
    UINT32 const *  apParam
    );

void
K2OSKERN_ReflectNotify(
    UINT32          aOpCode,
    UINT32 const *  apParam
);

#define SYSMSG_OPCODE_HIGH          0xFEED0000
#define SYSMSG_OPCODE_HIGH_MASK     0xFFFFF000
#define SYSMSG_OPCODE_THREAD_EXIT   (SYSMSG_OPCODE_HIGH + 0)
#define SYSMSG_OPCODE_SVC_CALL      (SYSMSG_OPCODE_HIGH + 1)

//
//------------------------------------------------------------------------
//

K2OS_TOKEN
K2OSKERN_CreateNotify(
    K2OS_TOKEN          aTokMailslot,
    UINT32              aMsgsCount,
    K2OS_TOKEN const *  apTokMsgs
);

K2OS_TOKEN
K2OSKERN_NotifySubscribe(
    K2OS_TOKEN          aTokNotify,
    K2_GUID128 const *  apInterfaceId,
    void *              apContext
);

BOOL
K2OSKERN_NotifyEnum(
    K2OS_TOKEN          aTokNotify,
    UINT32 *            apIoCount,
    K2_GUID128 *        apRetInterfaceIds
);

BOOL
K2OSKERN_NotifyAddMsgs(
    K2OS_TOKEN          aTokNotify,
    UINT32              aMsgsCount,
    K2OS_TOKEN const *  apTokMsgs
);

BOOL
K2OSKERN_NotifyAbortMsgs(
    K2OS_TOKEN          aTokNotify
);

typedef struct _K2OSKERN_NOTIFY_MSGIO K2OSKERN_NOTIFY_MSGIO;
struct _K2OSKERN_NOTIFY_MSGIO
{
    K2STAT      mStatus;
    void *      mpSubscribeContext;
    K2_GUID128  InterfaceId;
    UINT32      mServiceInstanceId;
    UINT32      mUnused;
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_NOTIFY_MSGIO) == sizeof(K2OS_MSGIO));

//
//------------------------------------------------------------------------
//

K2OS_TOKEN
K2OSKERN_ServiceCreate(
    K2OS_TOKEN  aTokMailslot,
    void *      apContext,
    UINT32 *    apRetInstanceId
);

UINT32
K2OSKERN_ServiceGetInstanceId(
    K2OS_TOKEN  aTokService
);

K2OS_TOKEN
K2OSKERN_ServicePublish(
    K2OS_TOKEN          aTokService,
    K2_GUID128 const *  apInterfaceId,
    void *              apContext,
    UINT32 *            apRetInstanceId
);
// destroy token to unpublish

UINT32
K2OSKERN_PublishGetInstanceId(
    K2OS_TOKEN  aTokPublish
);

K2STAT
K2OSKERN_ServiceCall(
    UINT32          aInterfaceInstanceId,
    UINT32          aCallCmd,
    void const *    apInBuf,
    UINT32          aInBufBytes,
    void *          apOutBuf,
    UINT32          aOutBufBytes,
    UINT32 *        apRetActualOut
);

typedef struct _K2OSKERN_SVC_MSGIO K2OSKERN_SVC_MSGIO;
struct _K2OSKERN_SVC_MSGIO
{
    K2STAT          mSvcOpCode;
    void *          mpServiceContext;
    void *          mpPublishContext;
    UINT32          mCallCmd;
    void const *    mpInBuf;
    UINT32          mInBufBytes;
    void *          mpOutBuf;
    UINT32          mOutBufBytes;
};
K2_STATIC_ASSERT(sizeof(K2OSKERN_SVC_MSGIO) == sizeof(K2OS_MSGIO));

//
// enumerate services that publish a specific interface
//
BOOL
K2OSKERN_ServiceEnum(
    K2_GUID128 const *  apInterfaceId,
    UINT32 *            apIoCount,
    UINT32 *            apRetServiceInstanceIds
);

//
// enumerate interfaces published by a specific service
//
BOOL
K2OSKERN_ServiceInterfaceEnum(
    UINT32          aServiceInstanceId,
    UINT32 *        apIoCount,
    K2_GUID128 *    apRetInterfaceIds
);

//
// enumerate all published instances of a particular interface
//
typedef struct K2OSKERN_SVC_IFINST K2OSKERN_SVC_IFINST;
struct K2OSKERN_SVC_IFINST
{
    UINT32  mServiceInstanceId;
    UINT32  mInterfaceInstanceId;
};

BOOL
K2OSKERN_InterfaceInstanceEnum(
    K2_GUID128 const *      apInterfaceId,
    UINT32 *                apIoCount,
    K2OSKERN_SVC_IFINST *   apRetInst
);

//
//------------------------------------------------------------------------
//

#if __cplusplus
}
#endif


#endif // __K2OSKERN_H