#ifndef NEON_SSE_H
#define NEON_SSE_H

#include <arm_neon.h>
#include <stdlib.h>

typedef int32x4_t	__m128i;
typedef float32x4_t	__m128;

static __inline void *
__mm_malloc(size_t size, size_t alignment)
{
	void *ptr;

	if (posix_memalign(&ptr, alignment, size) == 0)
		return ptr;
	else
		return NULL;
}

static __inline void
__mm_free(void *ptr)
{
	free(ptr);
}

static __inline void
_mm_clflush(void const *p __attribute__((unused)))
{
	/* Cannot use __builtin__clear_cache */
	/* Currenly not implemented */
}

static __inline void
_mm_sfence(void)
{
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static __inline __m128i
_mm_loadu_si128(__m128i *p)
{
	return (__m128i)(vld1q_s16((int16_t *)(p)));
}

static inline void
_mm_stream_si128(__m128i *p, __m128i a)
{
	__builtin_memcpy(p, &a, sizeof(a));
}

static inline void
_mm_stream_si32(int *p, int a)
{
	__builtin_memcpy(p, &a, sizeof(a));
}

static inline __m128i
_mm_set1_epi8(char b)
{
	int32_t value = (int32_t)b;

	return vdupq_n_s32(value);
}

static inline int
_mm_cvtsi128_si32(__m128i a)
{
	return vgetq_lane_s32(a, 0);
}

static inline void
_mm_clwb(const void *addr)
{
	/* Do nothing currently */
}

static inline void
_mm_clflushopt(char *uptr)
{
	/* Flush a cache line of 64 bytes */
	__builtin___clear_cache(uptr, uptr + 64);
}
#endif
