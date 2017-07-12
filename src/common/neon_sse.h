#ifndef NEON_SSE_H
#define NEON_SSE_H

#include <stdlib.h>

static __inline void
arm_clean_va_to_poc(void const *p __attribute__((unused)))
{
	asm volatile("dc cvac, %0" : : "r" (p) : "memory");
}

static __inline void
arm_data_memory_barrier(void)
{
	asm volatile("dmb ish" : : : "memory");
}

static inline void
arm_invalidate_va_to_poc(const void *addr)
{
	asm volatile("dc civac, %0" : : "r" (addr) : "memory");
}
#endif
