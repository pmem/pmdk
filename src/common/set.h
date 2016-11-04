/*
 * Copyright 2014-2016, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * set.h -- internal definitions for set module
 */

#ifndef NVML_SET_H
#define NVML_SET_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "pool_hdr.h"
#include "librpmem.h"
#include "pmem_provider.h"

/*
 * pool sets & replicas
 */
#define POOLSET_HDR_SIG "PMEMPOOLSET"
#define POOLSET_HDR_SIG_LEN 11	/* does NOT include '\0' */

#define POOLSET_REPLICA_SIG "REPLICA"
#define POOLSET_REPLICA_SIG_LEN 7	/* does NOT include '\0' */

#define POOL_LOCAL 0
#define POOL_REMOTE 1

#define REPLICAS_DISABLED 0
#define REPLICAS_ENABLED 1

struct pool_set_part {
	/* populated by a pool set file parser */
	const char *path;
	size_t filesize;	/* aligned to page size */
	int fd;
	struct pmem_provider provider;
	int created;		/* indicates newly created (zeroed) file */

	/* util_poolset_open/create */
	void *remote_hdr;	/* allocated header for remote replica */
	void *hdr;		/* base address of header */
	size_t hdrsize;		/* size of the header mapping */
	void *addr;		/* base address of the mapping */
	size_t size;		/* size of the mapping - page aligned */
	int rdonly;
	uuid_t uuid;
};

struct remote_replica {
	void *rpp;		/* RPMEMpool opaque handle */
	char *node_addr;	/* address of a remote node */
	/* poolset descriptor is a pool set file name on a remote node */
	char *pool_desc;	/* descriptor of a poolset */
};

struct pool_replica {
	unsigned nparts;
	size_t repsize;		/* total size of all the parts (mappings) */
	int is_pmem;		/* true if all the parts are in PMEM */
	struct remote_replica *remote;	/* not NULL if the replica */
					/* is a remote one */
	struct pool_set_part part[];
};

struct pool_set {
	unsigned nreplicas;
	uuid_t uuid;
	int rdonly;
	int zeroed;		/* true if all the parts are new files */
	size_t poolsize;	/* the smallest replica size */
	int remote;		/* true if contains a remote replica */
	struct pool_replica *replica[];
};

struct part_file {
	int is_remote;
	const char *path;	/* not-NULL only for a local part file */
	const char *node_addr;	/* address of a remote node */
	/* poolset descriptor is a pool set file name on a remote node */
	const char *pool_desc;	/* descriptor of a poolset */
};

struct pool_attr {
	const unsigned char *poolset_uuid;	/* pool uuid */
	const unsigned char *first_part_uuid;	/* first part uuid */
	const unsigned char *prev_repl_uuid;	/* prev replica uuid */
	const unsigned char *next_repl_uuid;	/* next replica uuid */
	const unsigned char *user_flags;	/* user flags */
};

#define REP(set, r)\
	((set)->replica[((set)->nreplicas + (r)) % (set)->nreplicas])

#define PART(rep, p)\
	((rep)->part[((rep)->nparts + (p)) % (rep)->nparts])

#define HDR(rep, p)\
	((struct pool_hdr *)(PART(rep, p).hdr))

int util_poolset_parse(struct pool_set **setp, const char *path, int fd);
int util_poolset_read(struct pool_set **setp, const char *path);
int util_poolset_create_set(struct pool_set **setp, const char *path,
	size_t poolsize, size_t minsize);
int util_poolset_open(struct pool_set *set);
void util_poolset_close(struct pool_set *set, int del);
void util_poolset_free(struct pool_set *set);
int util_poolset_chmod(struct pool_set *set, mode_t mode);
void util_poolset_fdclose(struct pool_set *set);
int util_is_poolset_file(const char *path);
int util_poolset_foreach_part(const char *path,
	int (*cb)(struct part_file *pf, void *arg), void *arg);
size_t util_poolset_size(const char *path);

int util_pool_create(struct pool_set **setp, const char *path, size_t poolsize,
	size_t minsize, const char *sig,
	uint32_t major, uint32_t compat, uint32_t incompat, uint32_t ro_compat,
	unsigned *nlanes, int can_have_rep);
int util_pool_create_uuids(struct pool_set **setp, const char *path,
	size_t poolsize, size_t minsize, const char *sig,
	uint32_t major, uint32_t compat, uint32_t incompat, uint32_t ro_compat,
	unsigned *nlanes, int can_have_rep,
	int remote, struct pool_attr *poolattr);

int util_part_open(struct pool_set_part *part, size_t minsize, int create);
void util_part_fdclose(struct pool_set_part *part);
int util_replica_open(struct pool_set *set, unsigned repidx, int flags);
int util_replica_close(struct pool_set *set, unsigned repidx);
int util_map_part(struct pool_set_part *part, void *addr, size_t size,
	size_t offset, int flags);
int util_unmap_part(struct pool_set_part *part);
int util_unmap_parts(struct pool_replica *rep, unsigned start_index,
	unsigned end_index);
int util_header_create(struct pool_set *set, unsigned repidx, unsigned partidx,
	const char *sig, uint32_t major, uint32_t compat, uint32_t incompat,
	uint32_t ro_compat, const unsigned char *prev_repl_uuid,
	const unsigned char *next_repl_uuid, const unsigned char *arch_flags);

int util_map_hdr(struct pool_set_part *part, int flags);
int util_unmap_hdr(struct pool_set_part *part);

int util_pool_open_nocheck(struct pool_set *set, int rdonly);
int util_pool_open(struct pool_set **setp, const char *path, int rdonly,
	size_t minsize, const char *sig, uint32_t major, uint32_t compat,
	uint32_t incompat, uint32_t ro_compat, unsigned *nlanes);
int util_pool_open_remote(struct pool_set **setp, const char *path, int rdonly,
	size_t minsize, char *sig, uint32_t *major,
	uint32_t *compat, uint32_t *incompat, uint32_t *ro_compat,
	unsigned char *poolset_uuid, unsigned char *first_part_uuid,
	unsigned char *prev_repl_uuid, unsigned char *next_repl_uuid,
	unsigned char *arch_flags);

void util_remote_init(void);
void util_remote_fini(void);

void util_remote_init_lock(void);
void util_remote_destroy_lock(void);
int util_pool_close_remote(RPMEMpool *rpp);
void util_remote_unload(void);
void util_replica_fdclose(struct pool_replica *rep);
int util_poolset_remote_open(struct pool_replica *rep, unsigned repidx,
			size_t minsize, int create, void *pool_addr,
			size_t pool_size, unsigned *nlanes);
int util_remote_load(void);
int util_replica_open_remote(struct pool_set *set, unsigned repidx, int flags);
int util_poolset_remote_replica_open(struct pool_set *set, unsigned repidx,
	size_t minsize, int create, unsigned *nlanes);

extern int (*Rpmem_persist)(RPMEMpool *rpp, size_t offset, size_t length,
								unsigned lane);
extern int (*Rpmem_read)(RPMEMpool *rpp, void *buff, size_t offset,
							size_t length);
extern int (*Rpmem_close)(RPMEMpool *rpp);

#endif
