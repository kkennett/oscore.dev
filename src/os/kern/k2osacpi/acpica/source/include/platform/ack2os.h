#ifndef __ACK2OS_H__
#define __ACK2OS_H__

#include <k2oskern.h>

#define ACPI_MACHINE_WIDTH      32

#define ACPI_CPU_FLAGS          BOOL

#define ACPI_SPINLOCK           K2OSKERN_SEQLOCK *

#define ACPI_SEMAPHORE          K2OS_TOKEN

#endif /* __ACK2OS_H__ */
