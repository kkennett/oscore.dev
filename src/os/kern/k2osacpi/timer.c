#include "k2osacpi.h"

UINT64
AcpiOsGetTimer(
    void)
{
    //
    // return current value of timer in 100 ns units
    //
    return K2OS_SysUpTimeMs() * 10000;
}

