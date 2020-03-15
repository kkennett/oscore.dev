#include "k2osacpi.h"

K2STAT 
K2_CALLCONV_REGS 
dlx_entry(
    DLX *   apDlx,
    UINT32  aReason
    )
{
    K2OS_DebugPrint("k2osacpi dlx_entry\n");

    return 0;
}

