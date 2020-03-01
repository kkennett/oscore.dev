#ifndef __KERNSHARED_H
#define __KERNSHARED_H

#include <k2oshal.h>
#include <lib/k2dlxsupp.h>

/* --------------------------------------------------------------------------------- */

typedef
void
(*K2OSKERN_pf_CrtThreadedPostInit)(
    void
    );

typedef 
void 
(*K2OSKERN_pf_Exec)(
    K2OSKERN_pf_CrtThreadedPostInit afThreadedPostInit,
    K2OSAPI const **                apRetOsApiPtr
    );

typedef struct _K2OSKERN_FUNCTAB K2OSKERN_FUNCTAB;
struct _K2OSKERN_FUNCTAB
{
    K2DLXSUPP_pf_GetInfo        GetDlxInfo;

    K2OSKERN_pf_Exec            Exec;

    K2OSKERN_pf_Debug           Debug;
    K2OSKERN_pf_Panic           Panic;
    K2OSKERN_pf_MicroStall      MicroStall;
    K2OSKERN_pf_GetAbsTimeMs    GetAbsTimeMs;
    K2OSKERN_pf_SetIntr         SetIntr;
    K2OSKERN_pf_SeqIntrInit     SeqIntrInit;
    K2OSKERN_pf_SeqIntrLock     SeqIntrLock;
    K2OSKERN_pf_SeqIntrUnlock   SeqIntrUnlock;
    K2OSKERN_pf_GetCpuIndex     GetCpuIndex;

    K2OSHAL_pf_DebugOut         DebugOut;
    K2OSHAL_pf_DebugIn          DebugIn;

    K2_pf_ASSERT                Assert;
    K2_pf_EXTRAP_MOUNT          ExTrap_Mount;
    K2_pf_EXTRAP_DISMOUNT       ExTrap_Dismount;
    K2_pf_RAISE_EXCEPTION       RaiseException;

    K2DLXSUPP_HOST              DlxHost;
};

typedef struct _K2OSKERN_SHARED K2OSKERN_SHARED;
struct _K2OSKERN_SHARED
{
    K2OS_UEFI_LOADINFO      LoadInfo;
    K2_CACHEINFO const *    mpCacheInfo;
    DLX *                   mpDlxKern;
    DLX *                   mpDlxHal;
    K2OSKERN_FUNCTAB        FuncTab;
};

/* --------------------------------------------------------------------------------- */


#endif // __KERNSHARED_H