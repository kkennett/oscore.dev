#ifndef __K2ASMA32_INC
#define __K2ASMA32_INC

/* --------------------------------------------------------------------------------- */

#include "k2basedefs.inc"

#define A32_DATA \
        .section ".data","aw"

#define A32_TEXT \
        .section ".text","ax"

#define A32_READ \
        .section ".text","a"

#define A32_STARTUPTEXT \
        .section ".text.startup","ax"

#define BEGIN_A32_PROC(symbol) \
        .global symbol; \
        .type   symbol,"function"; \
symbol:

#define BEGIN_A32_STATIC_PROC(symbol) \
        .type   symbol,"function"; \
symbol:

#define END_A32_PROC(symbol) \
        .size   symbol,.-symbol;

/* --------------------------------------------------------------------------------- */

#endif // __K2ASMA32_INC

