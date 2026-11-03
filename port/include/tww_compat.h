/* Force-included (-include) into every Wind Waker decomp file in the port
 * build: the Sunshine port's stand-ins for MWCC and MSL (port_compat.h),
 * plus what the Wind Waker decomp needs on top of them. */
#ifndef TWW_PORT_COMPAT_H
#define TWW_PORT_COMPAT_H

#include "port_compat.h"

/* The Wind Waker decomp declares the Gekko intrinsics as extern functions
 * (global.h); port_compat.h has inline versions of the ones the game uses. */
#ifdef __cplusplus
extern "C++" {
#endif
static inline int __rlwimi(int a, int b, int sh, int mb, int me)
{
	unsigned r = sh ? (((unsigned)b << sh) | ((unsigned)b >> (32 - sh))) : (unsigned)b;
	unsigned m = (mb <= me) ? ((0xFFFFFFFFu >> mb) & (0xFFFFFFFFu << (31 - me)))
	                        : ((0xFFFFFFFFu >> mb) | (0xFFFFFFFFu << (31 - me)));
	return (int)((r & m) | ((unsigned)a & ~m));
}
static inline void __dcbz(void* p, int off) { memset((char*)p + off, 0, 32); }
static inline void __dcbf(void*, int) {}
static inline void __dcbst(void*, int) {}
static inline void __dcbi(void*, int) {}
static inline void __dcbt(void*, int) {}
#ifdef __cplusplus
}
#endif
#define __fabs(x) fabs(x)
#define __fabsf(x) fabsf(x)
#define __fnabs(x) (-fabs(x))
#define __fnabsf(x) (-fabsf(x))

#ifdef __cplusplus
/* MSL's va_list is a struct, std::__tag_va_List, whose .list the game
 * passes on; the host's va_list goes inside the same shape. */
namespace std {
struct __tag_va_List {
	va_list list;
};
}
#endif

#endif
