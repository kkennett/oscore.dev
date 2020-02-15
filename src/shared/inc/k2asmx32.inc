#ifndef __K2ASMX32_INC
#define __K2ASMX32_INC

/* --------------------------------------------------------------------------------- */

.intel_syntax
.code32

#include "k2basedefs.inc"

#define X32_DATA \
        .section ".data","aw"

#define X32_TEXT \
        .section ".text","ax"

#define X32_READ \
        .section ".text","a"

#define X32_STARTUPTEXT \
        .section ".text.startup","x","progbits"

#define BEGIN_X32_PROC(symbol) \
        .text; \
        .balign 4; \
        .global symbol; \
        .type   symbol,"function"; \
symbol:

#define BEGIN_X32_STATIC_PROC(symbol) \
        .text; \
        .balign 4; \
        .type   symbol,"function"; \
symbol:

#define END_X32_PROC(symbol) \
        .size   symbol,.-symbol;

/* --------------------------------------------------------------------------------- */

#endif // __K2ASMX32_INC

