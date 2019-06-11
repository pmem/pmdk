/*
 * Copyright 2016-2019, Intel Corporation
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
 *     * Neither the name of the copyright holder nor the names of its
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
 * librpmem.h -- definitions of librpmem entry points (EXPERIMENTAL)
 *
 * This library provides low-level support for remote access to persistent
 * memory utilizing RDMA-capable RNICs.
 *
 * See librpmem(3) for details.
 */

#ifndef LIBRPMEM_H
#define LIBRPMEM_H 1

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rpmem_pool RPMEMpool;

#define RPMEM_POOL_HDR_SIG_LEN	8
#define RPMEM_POOL_HDR_UUID_LEN	16 /* uuid byte length */
#define RPMEM_POOL_USER_FLAGS_LEN 16

struct rpmem_pool_attr {
	char signature[RPMEM_POOL_HDR_SIG_LEN]; /* pool signature */
	uint32_t major; /* format major version number */
	uint32_t compat_features; /* mask: compatible "may" features */
	uint32_t incompat_features; /* mask: "must support" features */
	uint32_t ro_compat_features; /* mask: force RO if unsupported */
	unsigned char poolset_uuid[RPMEM_POOL_HDR_UUID_LEN]; /* pool uuid */
	unsigned char uuid[RPMEM_POOL_HDR_UUID_LEN]; /* first part uuid */
	unsigned char next_uuid[RPMEM_POOL_HDR_UUID_LEN]; /* next pool uuid */
	unsigned char prev_uuid[RPMEM_POOL_HDR_UUID_LEN]; /* prev pool uuid */
	unsigned char user_flags[RPMEM_POOL_USER_FLAGS_LEN]; /* user flags */
};

RPMEMpool *rpmem_create(const char *target, const char *pool_set_name,
		void *pool_addr, size_t pool_size, unsigned *nlanes,
		const struct rpmem_pool_attr *create_attr);

RPMEMpool *rpmem_open(const char *target, const char *pool_set_name,
		void *pool_addr, size_t pool_size, unsigned *nlanes,
		struct rpmem_pool_attr *open_attr);

int rpmem_set_attr(RPMEMpool *rpp, const struct rpmem_pool_attr *attr);

int rpmem_close(RPMEMpool *rpp);

#define RPMEM_PERSIST_RELAXED	(1U << 0)
#define RPMEM_FLUSH_RELAXED	(1U << 0)

int rpmem_flush(RPMEMpool *rpp, size_t offset, size_t length, unsigned lane,
		unsigned flags);
int rpmem_drain(RPMEMpool *rpp, unsigned lane, unsigned flags);

int rpmem_persist(RPMEMpool *rpp, size_t offset, size_t length,
		unsigned lane, unsigned flags);
int rpmem_read(RPMEMpool *rpp, void *buff, size_t offset, size_t length,
		unsigned lane);
int rpmem_deep_persist(RPMEMpool *rpp, size_t offset, size_t length,
		unsigned lane);

#define RPMEM_REMOVE_FORCE 0x1
#define RPMEM_REMOVE_POOL_SET 0x2

int rpmem_remove(const char *target, const char *pool_set, int flags);

/*
 * RPMEM_MAJOR_VERSION and RPMEM_MINOR_VERSION provide the current version of
 * the librpmem API as provided by this header file.  Applications can verify
 * that the version available at run-time is compatible with the version used
 * at compile-time by passing these defines to rpmem_check_version().
 */
#define RPMEM_MAJOR_VERSION 1
#define RPMEM_MINOR_VERSION 3
const char *rpmem_check_version(unsigned major_required,
		unsigned minor_required);

const char *rpmem_errormsg(void);

/* minimum size of a pool */
#define RPMEM_MIN_POOL ((size_t)(1024 * 8)) /* 8 KB */

/*
 * This limit is set arbitrary to incorporate a pool header and required
 * alignment plus supply.
 */
#define RPMEM_MIN_PART ((size_t)(1024 * 1024 * 2)) /* 2 MiB */

#ifdef __cplusplus
}
#endif
#endif	/* librpmem.h */
