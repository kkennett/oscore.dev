#ifndef __USERAPI_H
#define __USERAPI_H

#include <k2systype.h>

#define SYSCALL_ID_THREAD_EXIT      1
#define SYSCALL_ID_SIGNAL_NOTIFY    2
#define SYSCALL_ID_WAIT_FOR_NOTIFY  3
#define SYSCALL_ID_SLEEP            4

K2STAT SK_SystemCall(UINT_PTR aCallId, UINT_PTR aArg1, UINT_PTR aArg2, UINT_PTR *apResultValue);

#endif // __USERAPI_H