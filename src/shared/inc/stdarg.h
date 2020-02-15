/*
 * ISO C Standard:  7.15  Variable arguments  <stdarg.h>
 */

#ifndef _STDARG_H
#ifndef _ANSI_STDARG_H_

#include "k2basetype.h"

#if !K2_TOOLCHAIN_IS_MS

typedef VALIST va_list;

#define va_start(v,l)	K2_VASTART(v,l)
#define va_end(v)	    K2_VAEND(v)
#define va_arg(v,l)	    K2_VAARG(v,l)

#endif /* not K2_TOOLCHAIN_IS_MS */

#endif /* not _ANSI_STDARG_H_ */
#endif /* not _STDARG_H */