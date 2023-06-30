// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2023, Intel Corporation */

/*
 * pool.c -- pool processing functions
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <endian.h>

#include <sys/ioctl.h>
#include <linux/fs.h>

#include "libpmem.h"
#include "libpmempool.h"

#include "out.h"
#include "pmempool.h"
#include "pool.h"
#include "lane.h"
#include "obj.h"
#include "file.h"
#include "os.h"
#include "set.h"
#include "check_util.h"
#include "util_pmem.h"
#include "mmap.h"

/*
 * pool_set_read_header -- (internal) read a header of a pool set
 */
static int
pool_set_read_header(const char *fname, struct pool_hdr *hdr)
{
	struct pool_set *set;
	int ret = 0;

	if (util_poolset_read(&set, fname)) {
		return -1;
	}
	/* open the first part set file to read the pool header values */
	const struct pool_set_part *part = PART(REP(set, 0), 0);
	int fdp = util_file_open(part->path, NULL, 0, O_RDONLY);
	if (fdp < 0) {
		ERR("cannot open poolset part file");
		ret = -1;
		goto err_pool_set;
	}

	/* read the pool header from first pool set file */
	if (pread(fdp, hdr, sizeof(*hdr), 0) != sizeof(*hdr)) {
		ERR("cannot read pool header from poolset");
		ret = -1;
		goto err_close_part;
	}

err_close_part:
	os_close(fdp);

err_pool_set:
	util_poolset_free(set);
	return ret;
}

/*
 * pool_set_map -- (internal) map poolset
 */
static int
pool_set_map(const char *fname, struct pool_set **poolset, unsigned flags)
{
	ASSERTeq(util_is_poolset_file(fname), 1);

	struct pool_hdr hdr;
	if (pool_set_read_header(fname, &hdr))
		return -1;

	util_convert2h_hdr_nocheck(&hdr);

	/* parse pool type from first pool set file */
	enum pool_type type = pool_hdr_get_type(&hdr);
	if (type == POOL_TYPE_UNKNOWN) {
		ERR("cannot determine pool type from poolset");
		return -1;
	}

	/*
	 * Open the poolset, the values passed to util_pool_open are read
	 * from the first poolset file, these values are then compared with
	 * the values from all headers of poolset files.
	 */
	struct pool_attr attr;
	util_pool_hdr2attr(&attr, &hdr);
	if (util_pool_open(poolset, fname, 0 /* minpartsize */, &attr,
				NULL, NULL, flags | POOL_OPEN_IGNORE_SDS |
						POOL_OPEN_IGNORE_BAD_BLOCKS)) {
		ERR("opening poolset failed");
		return -1;
	}

	return 0;
}

/*
 * pool_params_from_header -- parse pool params from pool header
 */
void
pool_params_from_header(struct pool_params *params, const struct pool_hdr *hdr)
{
	memcpy(params->signature, hdr->signature, sizeof(params->signature));
	memcpy(&params->features, &hdr->features, sizeof(params->features));

	/*
	 * Check if file is a part of pool set by comparing the UUID with the
	 * next part UUID. If it is the same it means the pool consist of a
	 * single file.
	 */
	int uuid_eq_next = uuidcmp(hdr->uuid, hdr->next_part_uuid);
	int uuid_eq_prev = uuidcmp(hdr->uuid, hdr->prev_part_uuid);
	params->is_part = !params->is_poolset && (uuid_eq_next || uuid_eq_prev);

	params->type = pool_hdr_get_type(hdr);
}

/*
 * pool_check_type_to_pool_type -- (internal) convert check pool type to
 *	internal pool type value
 */
static enum pool_type
pool_check_type_to_pool_type(enum pmempool_pool_type check_pool_type)
{
	switch (check_pool_type) {
	case PMEMPOOL_POOL_TYPE_OBJ:
		return POOL_TYPE_OBJ;
	default:
		ERR("can not convert pmempool_pool_type %u to pool_type",
			check_pool_type);
		return POOL_TYPE_UNKNOWN;
	}
}

/*
 * pool_parse_params -- parse pool type, file size and block size
 */
static int
pool_params_parse(const PMEMpoolcheck *ppc, struct pool_params *params,
	int check)
{
	LOG(3, NULL);

	params->type = POOL_TYPE_UNKNOWN;
	params->is_poolset = util_is_poolset_file(ppc->path) == 1;

	int fd = util_file_open(ppc->path, NULL, 0, O_RDONLY);
	if (fd < 0)
		return -1;

	int ret = 0;

	os_stat_t stat_buf;
	ret = os_fstat(fd, &stat_buf);
	if (ret)
		goto out_close;

	ASSERT(stat_buf.st_size >= 0);

	params->mode = stat_buf.st_mode;

	struct pool_set *set;
	void *addr;
	if (params->is_poolset) {
		/*
		 * Need to close the poolset because it will be opened with
		 * flock in the following instructions.
		 */
		os_close(fd);
		fd = -1;

		if (check) {
			if (pool_set_map(ppc->path, &set, 0))
				return -1;
		} else {
			ret = util_poolset_create_set(&set, ppc->path,
				0, 0, true);
			if (ret < 0) {
				LOG(2, "cannot open pool set -- '%s'",
					ppc->path);
				return -1;
			}
			if (util_pool_open_nocheck(set,
						POOL_OPEN_IGNORE_BAD_BLOCKS))
				return -1;
		}

		params->size = set->poolsize;
		addr = set->replica[0]->part[0].addr;

		/*
		 * XXX mprotect for device dax with length not aligned to its
		 * page granularity causes SIGBUS on the next page fault.
		 * The length argument of this call should be changed to
		 * set->poolsize once the kernel issue is solved.
		 */
		if (mprotect(addr, set->replica[0]->repsize,
			PROT_READ) < 0) {
			ERR("!mprotect");
			goto out_unmap;
		}
		params->is_dev_dax = set->replica[0]->part[0].is_dev_dax;
		params->is_pmem = set->replica[0]->is_pmem;
	} else {
		enum file_type type = util_file_get_type(ppc->path);
		if (type < 0) {
			ret = -1;
			goto out_close;
		}

		ssize_t s = util_file_get_size(ppc->path);
		if (s < 0) {
			ret = -1;
			goto out_close;
		}
		params->size = (size_t)s;
		int map_sync;
		addr = util_map(fd, 0, params->size, MAP_SHARED, 1, 0,
			&map_sync);
		if (addr == NULL) {
			ret = -1;
			goto out_close;
		}
		params->is_dev_dax = type == TYPE_DEVDAX;
		params->is_pmem = params->is_dev_dax || map_sync ||
			pmem_is_pmem(addr, params->size);
	}

	struct pool_hdr hdr;
	memcpy(&hdr, addr, sizeof(hdr));
	util_convert2h_hdr_nocheck(&hdr);
	pool_params_from_header(params, &hdr);

	if (ppc->args.pool_type != PMEMPOOL_POOL_TYPE_DETECT) {
		enum pool_type declared_type =
			pool_check_type_to_pool_type(ppc->args.pool_type);
		if ((params->type & ~declared_type) != 0) {
			ERR("declared pool type does not match");
			errno = EINVAL;
			ret = 1;
			goto out_unmap;
		}
	}

	if (params->type == POOL_TYPE_OBJ) {
		struct pmemobjpool *pop = addr;
		memcpy(params->obj.layout, pop->layout,
			PMEMOBJ_MAX_LAYOUT);
	}

out_unmap:
	if (params->is_poolset) {
		ASSERTeq(fd, -1);
		ASSERTne(addr, NULL);
		util_poolset_close(set, DO_NOT_DELETE_PARTS);
	} else {
		ASSERTne(fd, -1);
		ASSERTne(addr, NULL);
		munmap(addr, params->size);
	}
out_close:
	if (fd != -1)
		os_close(fd);
	return ret;
}

/*
 * pool_set_file_open -- (internal) opens pool set file or regular file
 */
static struct pool_set_file *
pool_set_file_open(const char *fname, int rdonly)
{
	LOG(3, NULL);

	struct pool_set_file *file = calloc(1, sizeof(*file));
	if (!file)
		return NULL;

	file->fname = strdup(fname);
	if (!file->fname)
		goto err;

	const char *path = file->fname;

	int ret = util_poolset_create_set(&file->poolset, path,
		0, 0, true);
	if (ret < 0) {
		LOG(2, "cannot open pool set -- '%s'", path);
		goto err_free_fname;
	}
	unsigned flags = (rdonly ? POOL_OPEN_COW : 0) |
				POOL_OPEN_IGNORE_BAD_BLOCKS;
	if (util_pool_open_nocheck(file->poolset, flags))
		goto err_free_fname;

	file->size = file->poolset->poolsize;

	/* get modification time from the first part of first replica */
	path = file->poolset->replica[0]->part[0].path;
	file->addr = file->poolset->replica[0]->part[0].addr;

	os_stat_t buf;
	if (os_stat(path, &buf)) {
		ERR("%s", path);
		goto err_close_poolset;
	}

	file->mtime = buf.st_mtime;
	file->mode = buf.st_mode;
	return file;

err_close_poolset:
	util_poolset_close(file->poolset, DO_NOT_DELETE_PARTS);
err_free_fname:
	free(file->fname);
err:
	free(file);
	return NULL;
}

/*
 * pool_set_parse -- parse poolset file
 */
int
pool_set_parse(struct pool_set **setp, const char *path)
{
	LOG(3, "setp %p path %s", setp, path);

	int fd = os_open(path, O_RDONLY);
	int ret = 0;

	if (fd < 0)
		return 1;

	if (util_poolset_parse(setp, path, fd)) {
		ret = 1;
		goto err_close;
	}

err_close:
	os_close(fd);
	return ret;
}

/*
 * pool_data_alloc -- allocate pool data and open set_file
 */
struct pool_data *
pool_data_alloc(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	struct pool_data *pool = calloc(1, sizeof(*pool));
	if (!pool) {
		ERR("!calloc");
		return NULL;
	}

	if (pool_params_parse(ppc, &pool->params, 0))
		goto error;

	int rdonly = CHECK_IS_NOT(ppc, REPAIR);
	int prv = CHECK_IS(ppc, DRY_RUN);

	if (prv && pool->params.is_dev_dax) {
		errno = ENOTSUP;
		ERR("!cannot perform a dry run on dax device");
		goto error;
	}

	pool->set_file = pool_set_file_open(ppc->path, prv);
	if (pool->set_file == NULL)
		goto error;

	/*
	 * XXX mprotect for device dax with length not aligned to its
	 * page granularity causes SIGBUS on the next page fault.
	 * The length argument of this call should be changed to
	 * pool->set_file->poolsize once the kernel issue is solved.
	 */
	if (rdonly && mprotect(pool->set_file->addr,
		pool->set_file->poolset->replica[0]->repsize,
		PROT_READ) < 0)
		goto error;

	if (pool_set_file_map_headers(pool->set_file, rdonly, prv))
		goto error;

	return pool;

error:
	pool_data_free(pool);
	return NULL;
}

/*
 * pool_set_file_close -- (internal) closes pool set file or regular file
 */
static void
pool_set_file_close(struct pool_set_file *file)
{
	LOG(3, NULL);

	if (file->poolset)
		util_poolset_close(file->poolset, DO_NOT_DELETE_PARTS);
	else if (file->addr) {
		munmap(file->addr, file->size);
		os_close(file->fd);
	} else if (file->fd)
		os_close(file->fd);

	free(file->fname);
	free(file);
}

/*
 * pool_data_free -- close set_file and release pool data
 */
void
pool_data_free(struct pool_data *pool)
{
	LOG(3, NULL);

	if (pool->set_file) {
		pool_set_file_unmap_headers(pool->set_file);
		pool_set_file_close(pool->set_file);
	}

	free(pool);
}

/*
 * pool_set_file_map -- return mapped address at given offset
 */
void *
pool_set_file_map(struct pool_set_file *file, uint64_t offset)
{
	if (file->addr == MAP_FAILED)
		return NULL;

	return (char *)file->addr + offset;
}

/*
 * pool_read -- read from pool set file or regular file
 *
 * 'buff' has to be a buffer at least 'nbytes' long
 * 'off' is an offset from the beginning of the pool
 */
int
pool_read(struct pool_data *pool, void *buff, size_t nbytes, uint64_t off)
{
	if (off + nbytes > pool->set_file->size)
		return -1;

	memcpy(buff, (char *)pool->set_file->addr + off, nbytes);

	return 0;
}

/*
 * pool_write -- write to pool set file or regular file
 *
 * 'buff' has to be a buffer at least 'nbytes' long
 * 'off' is an offset from the beginning of the pool
 */
int
pool_write(struct pool_data *pool, const void *buff, size_t nbytes,
	uint64_t off)
{
	if (off + nbytes > pool->set_file->size)
		return -1;

	memcpy((char *)pool->set_file->addr + off, buff, nbytes);
	util_persist_auto(pool->params.is_pmem,
			(char *)pool->set_file->addr + off, nbytes);

	return 0;
}

/*
 * pool_copy -- make a copy of the pool
 */
int
pool_copy(struct pool_data *pool, const char *dst_path, int overwrite)
{
	struct pool_set_file *file = pool->set_file;
	int dfd;
	int exists = util_file_exists(dst_path);
	if (exists < 0)
		return -1;

	if (exists) {
		if (!overwrite) {
			errno = EEXIST;
			return -1;
		}
		dfd = util_file_open(dst_path, NULL, 0, O_RDWR);
	} else {
		errno = 0;
		dfd = util_file_create(dst_path, file->size, 0);
	}
	if (dfd < 0)
		return -1;

	int result = 0;
	os_stat_t stat_buf;
	if (os_stat(file->fname, &stat_buf)) {
		result = -1;
		goto out_close;
	}

	if (fchmod(dfd, stat_buf.st_mode)) {
		result = -1;
		goto out_close;
	}

	void *daddr = mmap(NULL, file->size, PROT_READ | PROT_WRITE,
		MAP_SHARED, dfd, 0);
	if (daddr == MAP_FAILED) {
		result = -1;
		goto out_close;
	}

	void *saddr = pool_set_file_map(file, 0);
	memcpy(daddr, saddr, file->size);
	munmap(daddr, file->size);

out_close:
	(void) os_close(dfd);
	return result;
}

/*
 * pool_set_part_copy -- make a copy of the poolset part
 */
int
pool_set_part_copy(struct pool_set_part *dpart, struct pool_set_part *spart,
	int overwrite)
{
	LOG(3, "dpart %p spart %p", dpart, spart);

	int result = 0;

	os_stat_t stat_buf;
	if (os_fstat(spart->fd, &stat_buf)) {
		ERR("!util_stat");
		return -1;
	}

	size_t smapped = 0;
	void *saddr = pmem_map_file(spart->path, 0, 0, S_IREAD, &smapped, NULL);
	if (!saddr)
		return -1;

	size_t dmapped = 0;
	int is_pmem;
	void *daddr;

	int exists = util_file_exists(dpart->path);
	if (exists < 0) {
		result = -1;
		goto out_sunmap;
	}

	if (exists) {
		if (!overwrite) {
			errno = EEXIST;
			result = -1;
			goto out_sunmap;
		}

		daddr = pmem_map_file(dpart->path, 0, 0, S_IWRITE, &dmapped,
			&is_pmem);
	} else {
		errno = 0;
		daddr = pmem_map_file(dpart->path, dpart->filesize,
				PMEM_FILE_CREATE | PMEM_FILE_EXCL,
				stat_buf.st_mode, &dmapped, &is_pmem);
	}
	if (!daddr) {
		result = -1;
		goto out_sunmap;
	}

#ifdef DEBUG
	/* provide extra logging in case of wrong dmapped/smapped value */
	if (dmapped < smapped) {
		LOG(1, "dmapped < smapped: dmapped = %lu, smapped = %lu",
			dmapped, smapped);
		ASSERT(0);
	}
#endif

	if (is_pmem) {
		pmem_memcpy_persist(daddr, saddr, smapped);
	} else {
		memcpy(daddr, saddr, smapped);
		pmem_msync(daddr, smapped);
	}

	pmem_unmap(daddr, dmapped);
out_sunmap:
	pmem_unmap(saddr, smapped);
	return result;
}

/*
 * pool_set_files_count -- get total number of parts of all replicas
 */
unsigned
pool_set_files_count(struct pool_set_file *file)
{
	unsigned ret = 0;
	unsigned nreplicas = file->poolset->nreplicas;
	for (unsigned r = 0; r < nreplicas; r++) {
		struct pool_replica *rep = file->poolset->replica[r];
		ret += rep->nparts;
	}

	return ret;
}

/*
 * pool_set_file_map_headers -- map headers of each pool set part file
 */
int
pool_set_file_map_headers(struct pool_set_file *file, int rdonly, int prv)
{
	if (!file->poolset)
		return -1;

	for (unsigned r = 0; r < file->poolset->nreplicas; r++) {
		struct pool_replica *rep = file->poolset->replica[r];
		for (unsigned p = 0; p < rep->nparts; p++) {
			struct pool_set_part *part = &rep->part[p];
			if (util_map_hdr(part,
				prv ? MAP_PRIVATE : MAP_SHARED, rdonly)) {
				part->hdr = NULL;
				goto err;
			}
		}
	}

	return 0;
err:
	pool_set_file_unmap_headers(file);
	return -1;
}

/*
 * pool_set_file_unmap_headers -- unmap headers of each pool set part file
 */
void
pool_set_file_unmap_headers(struct pool_set_file *file)
{
	if (!file->poolset)
		return;
	for (unsigned r = 0; r < file->poolset->nreplicas; r++) {
		struct pool_replica *rep = file->poolset->replica[r];
		for (unsigned p = 0; p < rep->nparts; p++) {
			struct pool_set_part *part = &rep->part[p];
			util_unmap_hdr(part);
		}
	}
}

/*
 * pool_get_signature -- (internal) return signature of specified pool type
 */
static const char *
pool_get_signature(enum pool_type type)
{
	switch (type) {
	case POOL_TYPE_OBJ:
		return OBJ_HDR_SIG;
	default:
		return NULL;
	}
}

/*
 * pool_hdr_default -- return default pool header values
 */
void
pool_hdr_default(enum pool_type type, struct pool_hdr *hdrp)
{
	memset(hdrp, 0, sizeof(*hdrp));
	const char *sig = pool_get_signature(type);
	ASSERTne(sig, NULL);

	memcpy(hdrp->signature, sig, POOL_HDR_SIG_LEN);

	switch (type) {
	case POOL_TYPE_OBJ:
		hdrp->major = OBJ_FORMAT_MAJOR;
		hdrp->features = obj_format_feat_default;
		break;
	default:
		break;
	}
}

/*
 * pool_hdr_get_type -- return pool type based on pool header data
 */
enum pool_type
pool_hdr_get_type(const struct pool_hdr *hdrp)
{
	if (memcmp(hdrp->signature, OBJ_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return POOL_TYPE_OBJ;
	else
		return POOL_TYPE_UNKNOWN;
}

/*
 * pool_get_pool_type_str -- return human-readable pool type string
 */
const char *
pool_get_pool_type_str(enum pool_type type)
{
	switch (type) {
	case POOL_TYPE_OBJ:
		return "pmemobj";
	default:
		return "unknown";
	}
}

/*
 * pool_set_type -- get pool type of a poolset
 */
enum pool_type
pool_set_type(struct pool_set *set)
{
	struct pool_hdr hdr;

	/* open the first part file to read the pool header values */
	const struct pool_set_part *part = PART(REP(set, 0), 0);

	if (util_file_pread(part->path, &hdr, sizeof(hdr), 0) !=
			sizeof(hdr)) {
		ERR("cannot read pool header from poolset");
		return POOL_TYPE_UNKNOWN;
	}

	util_convert2h_hdr_nocheck(&hdr);
	enum pool_type type = pool_hdr_get_type(&hdr);
	return type;
}

/*
 * pool_get_min_size -- return the minimum pool size of a pool of a given type
 */
size_t
pool_get_min_size(enum pool_type type)
{
	switch (type) {
	case POOL_TYPE_OBJ:
		return PMEMOBJ_MIN_POOL;
	default:
		ERR("unknown type of a pool");
		return SIZE_MAX;
	}
}

#if FAULT_INJECTION
void
pmempool_inject_fault_at(enum pmem_allocation_type type, int nth,
							const char *at)
{
	core_inject_fault_at(type, nth, at);
}

int
pmempool_fault_injection_enabled(void)
{
	return core_fault_injection_enabled();
}
#endif
