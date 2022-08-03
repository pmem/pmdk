/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */
/*
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

#ifndef PMDK_SET_H
#define PMDK_SET_H 1

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <sys/types.h>

#include "out.h"
#include "vec.h"
#include "pool_hdr.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * pool sets & replicas
 */
#define POOLSET_HDR_SIG "PMEMPOOLSET"
#define POOLSET_HDR_SIG_LEN 11	/* does NOT include '\0' */

#define POOLSET_REPLICA_SIG "REPLICA"
#define POOLSET_REPLICA_SIG_LEN 7	/* does NOT include '\0' */

#define POOLSET_OPTION_SIG "OPTION"
#define POOLSET_OPTION_SIG_LEN 6	/* does NOT include '\0' */

/* pool set option flags */
enum pool_set_option_flag {
	OPTION_UNKNOWN = 0x0,
	OPTION_SINGLEHDR = 0x1,	/* pool headers only in the first part */
	OPTION_NOHDRS = 0x2,	/* no pool headers, remote replicas only */
};

struct pool_set_option {
	const char *name;
	enum pool_set_option_flag flag;
};

#define POOL_LOCAL 0

#define REPLICAS_DISABLED 0
#define REPLICAS_ENABLED 1

/*  util_pool_open flags */
#define POOL_OPEN_COW			1	/* copy-on-write mode */
#define POOL_OPEN_IGNORE_SDS		2	/* ignore shutdown state */
#define POOL_OPEN_IGNORE_BAD_BLOCKS	4	/* ignore bad blocks */
#define POOL_OPEN_CHECK_BAD_BLOCKS	8	/* check bad blocks */

enum del_parts_mode {
	DO_NOT_DELETE_PARTS,	/* do not delete part files */
	DELETE_CREATED_PARTS,	/* delete only newly created parts files */
	DELETE_ALL_PARTS	/* force delete all parts files */
};

struct pool_set_part {
	/* populated by a pool set file parser */
	const char *path;
	size_t filesize;	/* aligned to page size */
	int fd;
	int flags;		/* stores flags used when opening the file */
				/* valid only if fd >= 0 */
	int is_dev_dax;		/* indicates if the part is on device dax */
	size_t alignment;	/* internal alignment (Device DAX only) */
	int created;		/* indicates newly created (zeroed) file */

	/* util_poolset_open/create */
	void *hdr;		/* base address of header */
	size_t hdrsize;		/* size of the header mapping */
	int hdr_map_sync;	/* header mapped with MAP_SYNC */
	void *addr;		/* base address of the mapping */
	size_t size;		/* size of the mapping - page aligned */
	int map_sync;		/* part has been mapped with MAP_SYNC flag */
	int rdonly;		/* is set based on compat features, affects */
				/* the whole poolset */
	uuid_t uuid;
	int has_bad_blocks;	/* part file contains bad blocks */
	int sds_dirty_modified;	/* sds dirty flag was set */
};

struct pool_set_directory {
	const char *path;
	size_t resvsize; /* size of the address space reservation */

};

struct pool_replica {
	unsigned nparts;
	unsigned nallocated;
	unsigned nhdrs;		/* should be 0, 1 or nparts */
	size_t repsize;		/* total size of all the parts (mappings) */
	size_t resvsize;	/* min size of the address space reservation */
	int is_pmem;		/* true if all the parts are in PMEM */

	VEC(, struct pool_set_directory) directory;
	struct pool_set_part part[];
};

struct pool_set {
	char *path;		/* path of the poolset file */
	unsigned nreplicas;
	uuid_t uuid;
	int rdonly;
	int zeroed;		/* true if all the parts are new files */
	size_t poolsize;	/* the smallest replica size */
	int has_bad_blocks;	/* pool set contains bad blocks */
	unsigned options;	/* enabled pool set options */

	int directory_based;
	size_t resvsize;

	unsigned next_id;
	unsigned next_directory_id;

	int ignore_sds;		/* don't use shutdown state */
	struct pool_replica *replica[];
};

struct part_file {
	struct pool_set_part *part;
};

struct pool_attr {
	char signature[POOL_HDR_SIG_LEN]; /* pool signature */
	uint32_t major; /* format major version number */
	features_t features; /* features flags */
	unsigned char poolset_uuid[POOL_HDR_UUID_LEN];		/* pool uuid */
	unsigned char first_part_uuid[POOL_HDR_UUID_LEN]; /* first part uuid */
	unsigned char prev_repl_uuid[POOL_HDR_UUID_LEN]; /* prev replica uuid */
	unsigned char next_repl_uuid[POOL_HDR_UUID_LEN]; /* next replica uuid */
	unsigned char arch_flags[POOL_HDR_ARCH_LEN];		/* arch flags */
};

/* get index of the (r)th replica */
static inline unsigned
REPidx(const struct pool_set *set, unsigned r)
{
	ASSERTne(set->nreplicas, 0);
	return r % set->nreplicas;
}

/* get index of the (r + 1)th replica */
static inline unsigned
REPNidx(const struct pool_set *set, unsigned r)
{
	ASSERTne(set->nreplicas, 0);
	return (r + 1) % set->nreplicas;
}

/* get index of the (r - 1)th replica */
static inline unsigned
REPPidx(const struct pool_set *set, unsigned r)
{
	ASSERTne(set->nreplicas, 0);
	return (set->nreplicas + r - 1) % set->nreplicas;
}

/* get index of the (r)th part */
static inline unsigned
PARTidx(const struct pool_replica *rep, unsigned p)
{
	ASSERTne(rep->nparts, 0);
	return p % rep->nparts;
}

/* get index of the (r + 1)th part */
static inline unsigned
PARTNidx(const struct pool_replica *rep, unsigned p)
{
	ASSERTne(rep->nparts, 0);
	return (p + 1) % rep->nparts;
}

/* get index of the (r - 1)th part */
static inline unsigned
PARTPidx(const struct pool_replica *rep, unsigned p)
{
	ASSERTne(rep->nparts, 0);
	return (rep->nparts + p - 1) % rep->nparts;
}

/* get index of the (r)th part */
static inline unsigned
HDRidx(const struct pool_replica *rep, unsigned p)
{
	ASSERTne(rep->nhdrs, 0);
	return p % rep->nhdrs;
}

/* get index of the (r + 1)th part */
static inline unsigned
HDRNidx(const struct pool_replica *rep, unsigned p)
{
	ASSERTne(rep->nhdrs, 0);
	return (p + 1) % rep->nhdrs;
}

/* get index of the (r - 1)th part */
static inline unsigned
HDRPidx(const struct pool_replica *rep, unsigned p)
{
	ASSERTne(rep->nhdrs, 0);
	return (rep->nhdrs + p - 1) % rep->nhdrs;
}

/* get (r)th replica */
static inline struct pool_replica *
REP(const struct pool_set *set, unsigned r)
{
	return set->replica[REPidx(set, r)];
}

/* get (r + 1)th replica */
static inline struct pool_replica *
REPN(const struct pool_set *set, unsigned r)
{
	return set->replica[REPNidx(set, r)];
}

/* get (r - 1)th replica */
static inline struct pool_replica *
REPP(const struct pool_set *set, unsigned r)
{
	return set->replica[REPPidx(set, r)];
}

/* get (p)th part */
static inline struct pool_set_part *
PART(struct pool_replica *rep, unsigned p)
{
	return &rep->part[PARTidx(rep, p)];
}

/* get (p + 1)th part */
static inline struct pool_set_part *
PARTN(struct pool_replica *rep, unsigned p)
{
	return &rep->part[PARTNidx(rep, p)];
}

/* get (p - 1)th part */
static inline struct pool_set_part *
PARTP(struct pool_replica *rep, unsigned p)
{
	return &rep->part[PARTPidx(rep, p)];
}

/* get (p)th header */
static inline struct pool_hdr *
HDR(struct pool_replica *rep, unsigned p)
{
	return (struct pool_hdr *)(rep->part[HDRidx(rep, p)].hdr);
}

/* get (p + 1)th header */
static inline struct pool_hdr *
HDRN(struct pool_replica *rep, unsigned p)
{
	return (struct pool_hdr *)(rep->part[HDRNidx(rep, p)].hdr);
}

/* get (p - 1)th header */
static inline struct pool_hdr *
HDRP(struct pool_replica *rep, unsigned p)
{
	return (struct pool_hdr *)(rep->part[HDRPidx(rep, p)].hdr);
}

extern int Prefault_at_open;
extern int Prefault_at_create;
extern int SDS_at_create;
extern int Fallocate_at_create;
extern int COW_at_open;

int util_poolset_parse(struct pool_set **setp, const char *path, int fd);
int util_poolset_read(struct pool_set **setp, const char *path);
int util_poolset_create_set(struct pool_set **setp, const char *path,
	size_t poolsize, size_t minsize, int ignore_sds);
int util_poolset_open(struct pool_set *set);
void util_poolset_close(struct pool_set *set, enum del_parts_mode del);
void util_poolset_free(struct pool_set *set);
int util_poolset_chmod(struct pool_set *set, mode_t mode);
void util_poolset_fdclose(struct pool_set *set);
void util_poolset_fdclose_always(struct pool_set *set);
int util_is_poolset_file(const char *path);
int util_poolset_foreach_part_struct(struct pool_set *set,
	int (*cb)(struct part_file *pf, void *arg), void *arg);
int util_poolset_foreach_part(const char *path,
	int (*cb)(struct part_file *pf, void *arg), void *arg);
size_t util_poolset_size(const char *path);

int util_replica_deep_common(const void *addr, size_t len,
	struct pool_set *set, unsigned replica_id, int flush);
int util_replica_deep_persist(const void *addr, size_t len,
	struct pool_set *set, unsigned replica_id);
int util_replica_deep_drain(const void *addr, size_t len,
	struct pool_set *set, unsigned replica_id);

int util_pool_create(struct pool_set **setp, const char *path, size_t poolsize,
	size_t minsize, size_t minpartsize, const struct pool_attr *attr,
	unsigned *nlanes, int can_have_rep);
int util_pool_create_uuids(struct pool_set **setp, const char *path,
	size_t poolsize, size_t minsize, size_t minpartsize,
	const struct pool_attr *attr, unsigned *nlanes, int can_have_rep);

int util_part_open(struct pool_set_part *part, size_t minsize, int create_part);
void util_part_fdclose(struct pool_set_part *part);
int util_replica_open(struct pool_set *set, unsigned repidx, int flags);
void util_pool_hdr2attr(struct pool_attr *attr, struct pool_hdr *hdr);
void util_pool_attr2hdr(struct pool_hdr *hdr,
		const struct pool_attr *attr);
int util_replica_close(struct pool_set *set, unsigned repidx);
int util_map_part(struct pool_set_part *part, void *addr, size_t size,
	size_t offset, int flags, int rdonly);
int util_unmap_part(struct pool_set_part *part);
int util_unmap_parts(struct pool_replica *rep, unsigned start_index,
	unsigned end_index);
int util_header_create(struct pool_set *set, unsigned repidx, unsigned partidx,
	const struct pool_attr *attr, int overwrite);

int util_map_hdr(struct pool_set_part *part, int flags, int rdonly);
void util_unmap_hdr(struct pool_set_part *part);

int util_pool_has_device_dax(struct pool_set *set);

int util_pool_open_nocheck(struct pool_set *set, unsigned flags);
int util_pool_open(struct pool_set **setp, const char *path, size_t minpartsize,
	const struct pool_attr *attr, unsigned *nlanes, void *addr,
	unsigned flags);

void *util_pool_extend(struct pool_set *set, size_t *size, size_t minpartsize);

void util_replica_fdclose(struct pool_replica *rep);
int util_replica_close_local(struct pool_replica *rep, unsigned repn,
		enum del_parts_mode del);

#ifdef __cplusplus
}
#endif

#endif
