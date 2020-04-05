//
//    Copyright <C> 2017, Kurt Kennett (K2)
//
//    All rights reserved.
// 
#ifndef __K2OSACPI_H
#define __K2OSACPI_H

#include <k2oskern.h>
#include <acenv.h>
#include <actypes.h>
#include <actbl.h>
#include <acpiosxf.h>
#include <acrestyp.h>
#include <acpixf.h>
#include <acexcep.h>

struct _K2OSACPI_INTR
{
    UINT32              InterruptNumber;
    ACPI_OSD_HANDLER    ServiceRoutine;
    K2OS_TOKEN          mToken;
    K2LIST_LINK         ListLink;
};
typedef struct _K2OSACPI_INTR K2OSACPI_INTR;

extern K2LIST_ANCHOR gK2OSACPI_IntrList;
extern K2LIST_ANCHOR gK2OSACPI_IntrFreeList;

#endif // __K2OSACPI_H