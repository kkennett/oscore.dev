//
//    Copyright <C> 2017, Kurt Kennett (K2)
//
//    All rights reserved.
// 
#ifndef __K2SYSTYPE_H
#define __K2SYSTYPE_H

#include <spec/dlx.h>

//
//------------------------------------------------------------------------
//

#ifdef __cplusplus
extern "C" {
#endif

//
// Raw Opaque Token
//
typedef void * K2_RAW_TOKEN;


//
// Assert
//
typedef 
void
(K2_CALLCONV_REGS *K2_pf_ASSERT)(
    char const *    apFile,
    int             aLineNum,
    char const *    apCondition
    );

extern K2_pf_ASSERT     K2_Assert;
#ifdef K2_DEBUG
#define K2_ASSERT(x)    if (!(x)) K2_Assert(__FILE__, __LINE__, K2_STRINGIZE(x))
#else
#define K2_ASSERT(x)    
#endif


//
// Raw Atomics
//
UINT32
K2_CALLCONV_REGS
K2ATOMIC_CompareExchange32(
    UINT32 volatile *apMem,
    UINT32 aNewVal,
    UINT32 aComparand
);

UINT64
K2_CALLCONV_REGS
K2ATOMIC_CompareExchange64(
    UINT64 volatile *apMem,
    UINT64 aNewVal,
    UINT64 aComparand
);



//
// Exception Trap
//

K2_PACKED_PUSH
typedef struct _K2_EXCEPTION_TRAP K2_EXCEPTION_TRAP;
struct _K2_EXCEPTION_TRAP
{
    K2_EXCEPTION_TRAP * mpNextTrap;
    K2STAT              mResult;
    K2_TRAP_CONTEXT     SavedContext;
} K2_PACKED_ATTRIB;
K2_PACKED_POP

typedef 
BOOL       
(K2_CALLCONV_REGS *K2_pf_EXTRAP_MOUNT)(
    K2_EXCEPTION_TRAP *apTrap
    );

typedef 
K2STAT 
(K2_CALLCONV_REGS *K2_pf_EXTRAP_DISMOUNT)(
    K2_EXCEPTION_TRAP *apTrap
    );

typedef 
void       
(K2_CALLCONV_REGS *K2_pf_RAISE_EXCEPTION)(
    K2STAT aExceptionCode
    );

extern K2_pf_EXTRAP_MOUNT      K2_ExTrap_Mount;
extern K2_pf_EXTRAP_DISMOUNT   K2_ExTrap_Dismount;
extern K2_pf_RAISE_EXCEPTION   K2_RaiseException;

#define K2_EXTRAP(pTrap, Func)  (K2_ExTrap_Mount(pTrap) ? \
        (pTrap)->mResult : \
        ((pTrap)->mResult = (Func), K2_ExTrap_Dismount(pTrap)))

#ifdef __cplusplus
}
#endif

//
//------------------------------------------------------------------------
//

#endif  // __K2SYSTYPE_H
