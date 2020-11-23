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

#include <lib/k2rofshelp.h>

#define K2_STATIC static

#define DESC_BUFFER_BYTES   (((sizeof(K2EFI_MEMORY_DESCRIPTOR) * 2) + 4) & ~3)

K2_STATIC
K2ROFS const *
sMapBuiltIn(void)
{
    K2STAT                      stat;
    UINT32                      entCount;
    UINT8                       descBuffer[DESC_BUFFER_BYTES];
    K2EFI_MEMORY_DESCRIPTOR *   pDesc = (K2EFI_MEMORY_DESCRIPTOR *)descBuffer;
    UINT8 *                     pScan;
    UINT32                      ix;
    UINT32                      virtAddr;

    //
    // find the builtin region in the efi memory map
    //
    entCount = gData.mpShared->LoadInfo.mEfiMapSize / gData.mpShared->LoadInfo.mEfiMemDescSize;
    pScan = (UINT8 *)K2OS_KVA_EFIMAP_BASE;

    for (ix = 0; ix < entCount; ix++)
    {
        K2MEM_Copy(descBuffer, pScan, sizeof(K2EFI_MEMORY_DESCRIPTOR));
        pScan += gData.mpShared->LoadInfo.mEfiMemDescSize;
        if (pDesc->Type == K2OS_EFIMEMTYPE_BUILTIN)
            break;
    }
    K2_ASSERT(ix != entCount);

    stat = KernMem_MapContigPhys(
        (UINT32)pDesc->PhysicalStart,
        (UINT32)pDesc->NumberOfPages,
        K2OSKERN_SEG_ATTR_TYPE_BUILTIN | K2OS_MAPTYPE_KERN_READ,
        &virtAddr);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    return (K2ROFS const *)virtAddr;
}

void KernAcpi_Init(void)
{
    DLX_pf_ENTRYPOINT   acpiEntryPoint;
    K2STAT              stat;

    stat = K2DLXSUPP_GetInfo(
        gData.mpDlxAcpi,
        NULL,
        &acpiEntryPoint,
        NULL,
        NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    stat = acpiEntryPoint(gData.mpDlxAcpi, DLX_ENTRY_REASON_LOAD);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));
}

K2_STATIC
K2OSEXEC_pf_Run 
sExec_Init(
    K2OSHAL_pf_OnSystemReady * apfSysReady
)
{
    DLX_pf_ENTRYPOINT   execEntryPoint;
    K2STAT              stat;
    K2OSEXEC_INIT_INFO  initInfo;
    K2OSEXEC_pf_Init    fExec_Init;
    K2OSEXEC_pf_Run     fExec_Run;

    stat = DLX_FindExport(
        gData.mpDlxExec,
        DlxSeg_Text,
        "K2OSEXEC_Init",
        (UINT32 *)&fExec_Init);
    if (K2STAT_IS_ERROR(stat))
        K2OSKERN_Panic("*** Required K2OSEXEC export \"K2OSEXEC_Init\" missing\n");

    stat = DLX_FindExport(
        gData.mpDlxExec,
        DlxSeg_Text,
        "K2OSEXEC_Run",
        (UINT32 *)&fExec_Run);
    if (K2STAT_IS_ERROR(stat))
        K2OSKERN_Panic("*** Required K2OSEXEC export \"K2OSEXEC_Run\" missing\n");

    stat = K2DLXSUPP_GetInfo(
        gData.mpDlxExec,
        NULL,
        &execEntryPoint,
        NULL,
        NULL);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    stat = execEntryPoint(gData.mpDlxExec, DLX_ENTRY_REASON_LOAD);
    K2_ASSERT(!K2STAT_IS_ERROR(stat));

    K2MEM_Zero(&initInfo,sizeof(initInfo));
    initInfo.mEfiMapSize = gData.mpShared->LoadInfo.mEfiMapSize;
    initInfo.mEfiMemDescSize = gData.mpShared->LoadInfo.mEfiMemDescSize;
    initInfo.mEfiMemDescVer = gData.mpShared->LoadInfo.mEfiMemDescVer;

    //
    // map in the built in file system
    //
    initInfo.mpBuiltinRofs = sMapBuiltIn();

    //
    // off you go
    //
    fExec_Init(&initInfo);

    //
    // confirm outputs from init are good
    //
#if !K2_TARGET_ARCH_IS_ARM
    K2_ASSERT(initInfo.SysTickDevIrqConfig.mSourceIrq <= X32_DEVIRQ_LVT_ERROR);
#endif

    stat = DLX_FindExport(
        gData.mpDlxHal,
        DlxSeg_Text,
        "K2OSHAL_OnSystemReady",
        (UINT32 *)apfSysReady);
    if (K2STAT_IS_ERROR(stat))
        K2OSKERN_Debug("*** HAL has no OnSystemReady()\n");

#if TRACING_ON
    gData.mTraceStarted = TRUE;
#endif

    KernSched_StartSysTick(&initInfo.SysTickDevIrqConfig);

    return fExec_Run;
}

UINT32 K2_CALLCONV_REGS K2OSKERN_Thread0(void *apArg)
{
    KernInitStage               initStage;
    K2OSHAL_pf_OnSystemReady    fSysReady;

    //
    // run stages up to multithreaded
    //
    for (initStage = KernInitStage_Threaded; initStage < KernInitStage_MultiThreaded; initStage++)
    {
        KernInit_Stage(initStage);
    }

    //
    // kernel init finished.  init core stuff that needs threads
    //
    gData.mpShared->FuncTab.CoreThreadedPostInit();

    //
    // "load" ACPI
    //
    KernAcpi_Init();

    //
    // load the exec and run it
    //
    fSysReady = NULL;
    (sExec_Init(&fSysReady))(fSysReady);

    K2OSKERN_Panic("Executive returned");

    return 0xAABBCCDD;
}

