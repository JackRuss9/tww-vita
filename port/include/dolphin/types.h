/* Port override of tww/include/dolphin/types.h (same include guard, so the
 * decomp's copy is never read). The integer types, AT_ADDRESS and PTR32 come
 * from the Sunshine port's override (sms-port/src/port_include/dolphin/
 * types.h), so the game and the platform layer agree on every type; the
 * rest is what the Wind Waker decomp's types.h adds. */
#ifndef TYPES_H_
#define TYPES_H_

#include "../../../sms-port/src/port_include/dolphin/types.h"

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;
typedef volatile f32 vf32;
typedef volatile f64 vf64;

typedef unsigned int uint;

#define READU32_BE(ptr, offset)                                                                    \
    (((u32)ptr[offset] << 24) | ((u32)ptr[offset + 1] << 16) | ((u32)ptr[offset + 2] << 8) |       \
     (u32)ptr[offset + 3]);

#ifndef DECL_WEAK
#define DECL_WEAK __attribute__((weak))
#endif

#define __REGISTER

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>

#endif
