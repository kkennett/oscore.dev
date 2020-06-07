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
#ifndef __K2OSEXEC_H
#define __K2OSEXEC_H

#include "k2oskern.h"
#include <spec/k2pci.h>

#if __cplusplus
extern "C" {
#endif

//
//------------------------------------------------------------------------
//

extern K2_GUID128 const gK2OSEXEC_MailslotGuid;
extern char const *     gpK2OSEXEC_MailslotGuidStr;

extern K2_GUID128 const gK2OSEXEC_DriverStoreInterfaceGuid;
extern char const *     gpK2OSEXEC_DriverStoreInterfaceGuidStr;

extern K2_GUID128 const gK2OSEXEC_FsProvInterfaceGuid;
extern char const *     gpK2OSEXEC_FsProvInterfaceGuidStr;

extern K2_GUID128 const gK2OSEXEC_FileSysInterfaceGuid;
extern char const *     gpK2OSEXEC_FileSysInterfaceGuidStr;

//
//------------------------------------------------------------------------
//

typedef struct _K2OSEXEC_PCI_ID K2OSEXEC_PCI_ID;
struct _K2OSEXEC_PCI_ID
{
    UINT16  mSegment;
    UINT16  mBus;
    UINT16  mDevice;
    UINT16  mFunction;
};

typedef struct _K2OSEXEC_PCI_DEV_INFO K2OSEXEC_PCI_DEV_INFO;
struct _K2OSEXEC_PCI_DEV_INFO
{
    K2OSEXEC_PCI_ID Id;
    UINT32          mBarsFoundCount;
    UINT32          mBarSize[6];
    PCICFG          Cfg;
};

K2OSEXEC_PCI_DEV_INFO const * K2OSEXEC_DevPci_GetInfo(UINT32 aDevInstanceId);

//
//------------------------------------------------------------------------
//

#define K2OS_INTERFACE_ID_DRIVERSTORE    \
    { 0x612a5d97, 0xc492, 0x44b3, { 0x94, 0xa1, 0xac, 0xe4, 0x75, 0xeb, 0x93, 0xe } };

//
// driver store service calls
//
#define DRVSTORE_CALL_OPCODE_HIGH           0x700C0000
#define DRVSTORE_CALL_OPCODE_HIGH_MASK      0x7FFFF000
#define DRVSTORE_CALL_OPCODE_GET_INFO       (DRVSTORE_CALL_OPCODE_HIGH + 0 + K2OS_MSGOPCODE_HAS_RESPONSE)
#define DRVSTORE_CALL_OPCODE_GET_DIRECT     (DRVSTORE_CALL_OPCODE_HIGH + 1 + K2OS_MSGOPCODE_HAS_RESPONSE)

#define K2OSEXEC_DRVSTOREINFO_NAME_BUF_COUNT  16

typedef struct _K2OSEXEC_DRVSTORE_INFO K2OSEXEC_DRVSTORE_INFO;
struct _K2OSEXEC_DRVSTORE_INFO
{
    K2_GUID128  mStoreId;
    char        StoreName[K2OSEXEC_DRVSTOREINFO_NAME_BUF_COUNT];
    UINT32      mStoreVersion;
};

#define K2OSEXEC_DRVSTORE_ID_HAL \
    { 0xafa9a8c6, 0xd494, 0x4d7b, { 0x94, 0x23, 0x16, 0x9e, 0x41, 0x6c, 0x23, 0x96 } }

#define K2OSEXEC_DRVSTORE_CURRENT_VERSION   0x00010000

typedef K2STAT (*K2OSEXEC_pf_DriverStore_FindDriver)(UINT32 aNumTypeIds, char const **appTypeIds, UINT32 *apRetSelect);
typedef K2STAT (*K2OSEXEC_pf_DriverStore_PrepareDriverInstance)(char const *apTypeId, UINT32 *apRetStoreHandle);
typedef K2STAT (*K2OSEXEC_pf_DriverStore_ActivateDriver)(UINT32 aStoreHandle, UINT32 aDevInstanceId);
typedef K2STAT (*K2OSEXEC_pf_DriverStore_PurgeDriverInstance)(UINT32 aStoreHandle);

typedef struct _K2OSEXEC_DRVSTORE_DIRECT K2OSEXEC_DRVSTORE_DIRECT;
struct _K2OSEXEC_DRVSTORE_DIRECT
{
    UINT32  mVersion;

    K2OSEXEC_pf_DriverStore_FindDriver              FindDriver;
    K2OSEXEC_pf_DriverStore_PrepareDriverInstance   PrepareDriverInstance;
    K2OSEXEC_pf_DriverStore_ActivateDriver          ActivateDriver;
    K2OSEXEC_pf_DriverStore_PurgeDriverInstance     PurgeDriverInstance;
};

//
//------------------------------------------------------------------------
//

#define K2OS_INTERFACE_ID_FSPROV    \
    { 0x440c404, 0xcad6, 0x4333, { 0xa2, 0x3d, 0x24, 0xfd, 0xbf, 0x33, 0x5d, 0xa8 } }

//
// filesys provider service calls
//
#define FSPROV_CALL_OPCODE_HIGH             0x700D0000
#define FSPROV_CALL_OPCODE_HIGH_MASK        0x7FFFF000
#define FSPROV_CALL_OPCODE_GET_INFO         (FSPROV_CALL_OPCODE_HIGH + 0 + K2OS_MSGOPCODE_HAS_RESPONSE)

#define K2OS_INTERFACE_ID_FILESYS   \
    { 0x71e000d8, 0x2a94, 0x476a, { 0xb6, 0xc7, 0xa8, 0x57, 0x5c, 0x66, 0x18, 0xce } }

#define K2OS_FSPROV_ID_HAL \
    { 0x138de9b5, 0x2549, 0x4376, { 0xbd, 0xe7, 0x2c, 0x3a, 0xf4, 0x1b, 0x21, 0x80 } }

//
//------------------------------------------------------------------------
//

#if __cplusplus
}
#endif

#endif // __K2OSKERN_H