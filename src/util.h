/*
 * Copyright (c) 2014, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * util.h -- internal definitions for util module
 */

/*
 * overridable names for malloc & friends used by this library
 */
typedef void *(*Malloc_func)(size_t size);
typedef void (*Free_func)(void *ptr);
typedef void *(*Realloc_func)(void *ptr, size_t size);
typedef char *(*Strdup_func)(const char *s);

Malloc_func Malloc;
Free_func Free;
Realloc_func Realloc;
Strdup_func Strdup;

void util_set_alloc_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s));
void *util_map(int fd, size_t len, int cow);
int util_unmap(void *addr, size_t len);

/*
 * header used at the beginning of all types of memory pools
 *
 * for pools build on persistent memory, the integer types
 * below are stored in little-endian byte order.
 */
#define	POOL_HDR_SIG_LEN 8
#define	POOL_HDR_UUID_LEN 16
struct pool_hdr {
	char signature[POOL_HDR_SIG_LEN];
	uint32_t major;			/* format major version number */
	uint32_t compat_features;	/* mask: compatible "may" features */
	uint32_t incompat_features;	/* mask: "must support" features */
	uint32_t ro_compat_features;	/* mask: force RO if unsupported */
	unsigned char uuid[POOL_HDR_UUID_LEN];
	uint64_t crtime;		/* when created (seconds since epoch) */
	unsigned char unused[4040];	/* must be zero */
	uint64_t checksum;		/* checksum of above fields */
};

int util_checksum(void *addr, size_t len, uint64_t *csump, int insert);
int util_convert_hdr(struct pool_hdr *hdrp);

/*
 * macros for micromanaging range protections for the debug version
 */
#ifdef	DEBUG

#define	RANGE_RO(addr, len) ASSERT(util_range_ro(addr, len) >= 0)
#define	RANGE_RW(addr, len) ASSERT(util_range_rw(addr, len) >= 0)

#else

/* nondebug version */
#define	RANGE_RO(addr, len)
#define	RANGE_RW(addr, len)

#endif	/* DEBUG */

void util_init(void);

int util_range_ro(void *addr, size_t len);
int util_range_rw(void *addr, size_t len);
int util_range_none(void *addr, size_t len);

int util_feature_check(struct pool_hdr *hdrp, uint32_t incompat,
				uint32_t ro_compat, uint32_t compat);
