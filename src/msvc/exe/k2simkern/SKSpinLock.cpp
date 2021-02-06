#include "SimKern.h"

void SKSpinLock_Init(SKSpinLock *apLock)
{
    InitializeCriticalSection(&apLock->mWin32Sec);
}

void SKSpinLock_Lock(SKSpinLock *apLock)
{
    EnterCriticalSection(&apLock->mWin32Sec);
}

void SKSpinLock_Unlock(SKSpinLock *apLock)
{
    LeaveCriticalSection(&apLock->mWin32Sec);
}

