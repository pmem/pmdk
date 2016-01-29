/*
 * Copyright 2016, Intel Corporation
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
 * pool.c -- pool processing functions
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "libpmemlog.h"
#include "libpmemblk.h"
#include "libpmempool.h"

#include "out.h"
#include "pmempool.h"
#include "pool.h"
#include "lane.h"
#include "obj.h"
#include "btt.h"
#include "check_util.h"

/*
 * pool_btt_lseek -- (internal) perform lseek in BTT file mode
 */
static inline off_t
pool_btt_lseek(struct pool_data *pool, off_t offset, int whence)
{
	off_t result;
	if ((result = lseek(pool->set_file->fd, offset, whence)) == -1)
		ERR("!lseek");

	return result;
}

/*
 * pool_btt_read -- (internal) perform read in BTT file mode
 */
static inline ssize_t
pool_btt_read(struct pool_data *pool, void *dst, size_t count)
{
	size_t total = 0;
	ssize_t nread;
	while (count > total &&
		(nread = read(pool->set_file->fd, dst, count - total))) {
		if (nread == -1) {
			ERR("!read");
			return total ? (ssize_t)total : -1;
		}

		dst = (void *)((ssize_t)dst + nread);
		total += (size_t)nread;
	}

	return (ssize_t)total;
}

/*
 * pool_btt_write -- (internal) perform write in BTT file mode
 */
static inline ssize_t
pool_btt_write(struct pool_data *pool, const void *src, size_t count)
{
	ssize_t nwrite = 0;
	size_t total = 0;
	while (count > total &&
		(nwrite = write(pool->set_file->fd, src, count - total))) {
		if (nwrite == -1) {
			ERR("!write");
			return total ? (ssize_t)total : -1;
		}

		src = (void *)((ssize_t)src + nwrite);
		total += (size_t)nwrite;
	}

	return (ssize_t)total;
}

/*
 * pool_get_min_size -- (internal) return minimum size of pool for specified
 *	type
 */
static uint64_t
pool_get_min_size(enum pool_type type)
{
	switch (type) {
	case POOL_TYPE_LOG:
		return PMEMLOG_MIN_POOL;
	case POOL_TYPE_BLK:
		return PMEMBLK_MIN_POOL;
	case POOL_TYPE_OBJ:
		return PMEMOBJ_MIN_POOL;
	default:
		break;
	}

	return 0;
}

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
	const struct pool_set_part *part = &PART(REP(set, 0), 0);
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
	close(fdp);

err_pool_set:
	util_poolset_free(set);
	return ret;
}

/*
 * pool_set_map -- (internal) map poolset
 */
static int
pool_set_map(const char *fname, struct pool_set **poolset, int rdonly)
{
	if (util_is_poolset(fname) != 1)
		return util_pool_open_nocheck(poolset, fname, rdonly);

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

	/* get minimum size based on pool type for util_pool_open */
	size_t minsize = pool_get_min_size(type);

	/*
	 * Open the poolset, the values passed to util_pool_open are read
	 * from the first poolset file, these values are then compared with
	 * the values from all headers of poolset files.
	 */
	if (util_pool_open(poolset, fname, rdonly, minsize, hdr.signature,
			hdr.major, hdr.compat_features, hdr.incompat_features,
			hdr.ro_compat_features)) {
		ERR("openning poolset failed");
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

	/*
	 * Check if file is a part of pool set by comparing the UUID with the
	 * next part UUID. If it is the same it means the pool consist of a
	 * single file.
	 */
	int uuid_eq_next = memcmp(hdr->uuid, hdr->next_part_uuid,
		POOL_HDR_UUID_LEN);
	int uuid_eq_prev = memcmp(hdr->uuid, hdr->prev_part_uuid,
		POOL_HDR_UUID_LEN);
	params->is_part = !params->is_poolset && (uuid_eq_next || uuid_eq_prev);

	params->type = pool_hdr_get_type(hdr);
}

/*
 * pmempool_check_type_to_pool_type -- (internal) convert check pool type to
 *	internal pool type value
 */
static enum pool_type
pmempool_check_type_to_pool_type(enum pmempool_pool_type check_pool_type)
{
	switch (check_pool_type) {
	case PMEMPOOL_POOL_TYPE_LOG:
		return POOL_TYPE_LOG;
	case PMEMPOOL_POOL_TYPE_BLK:
		return POOL_TYPE_BLK;
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

	int is_btt = ppc->args.pool_type == PMEMPOOL_POOL_TYPE_BTT;
	struct stat stat_buf;
	int ret = 0;

	params->type = POOL_TYPE_UNKNOWN;
	params->is_poolset = is_btt ? false : util_is_poolset(ppc->path) == 1;
	int fd = util_file_open(ppc->path, NULL, 0, O_RDONLY);
	if (fd < 0)
		return -1;

	if (!params->is_poolset) {
		/* get file size and mode */
		if (fstat(fd, &stat_buf)) {
			ret = -1;
			goto out_close;
		}

		ASSERT(stat_buf.st_size >= 0);
		params->size = (uint64_t)stat_buf.st_size;
		params->mode = stat_buf.st_mode;
	}

	void *addr = NULL;
	struct pool_set *set = NULL;
	if (params->is_poolset) {
		/* close the file */
		close(fd);
		fd = -1;

		if (check) {
			if (pool_set_map(ppc->path, &set, 1))
				return -1;
		} else {
			if (util_pool_open_nocheck(&set, ppc->path, 1))
				return -1;
		}

		params->size = set->poolsize;
		addr = set->replica[0]->part[0].addr;
	} else if (!is_btt) {
		addr = mmap(NULL, (uint64_t)stat_buf.st_size, PROT_READ,
			MAP_PRIVATE, fd, 0);
		if (addr == MAP_FAILED) {
			ret = -1;
			goto out_close;
		}
	} else {
		params->type = POOL_TYPE_BTT;
		params->is_part = false;
		goto out_close;
	}

	struct pool_hdr hdr;
	memcpy(&hdr, addr, sizeof(hdr));
	util_convert2h_hdr_nocheck(&hdr);
	pool_params_from_header(params, &hdr);

	if (ppc->args.pool_type != PMEMPOOL_POOL_TYPE_DETECT) {
		enum pool_type declared_type =
			pmempool_check_type_to_pool_type(ppc->args.pool_type);
		if ((params->type & ~declared_type) != 0) {
			ERR("declared pool type does not match");
			ret = 1;
			goto out_unmap;
		}
	}

	if (params->type == POOL_TYPE_BLK) {
		struct pmemblk pbp;
		memcpy(&pbp, addr, sizeof(pbp));
		params->blk.bsize = le32toh(pbp.bsize);
	} else if (params->type == POOL_TYPE_OBJ) {
		struct pmemobjpool pop;
		memcpy(&pop, addr, sizeof(pop));
		memcpy(params->obj.layout, pop.layout,
			PMEMOBJ_MAX_LAYOUT);
	}

out_unmap:
	if (params->is_poolset)
		util_poolset_close(set, 0);
	else if (!is_btt)
		munmap(addr, (uint64_t)stat_buf.st_size);
out_close:
	if (fd >= 0)
		(void) close(fd);
	return ret;
}

/*
 * pool_set_file_open -- (internal) opens pool set file or regular file
 */
static struct pool_set_file *
pool_set_file_open(const char *fname, struct pool_params *params, int rdonly)
{
	LOG(3, NULL);

	struct pool_set_file *file = calloc(1, sizeof(*file));
	if (!file)
		return NULL;

	file->fname = strdup(fname);
	if (!file->fname)
		goto err;

	const char *path = file->fname;

	if (params->type != POOL_TYPE_BTT) {
		if (util_pool_open_nocheck(&file->poolset, file->fname, rdonly))
			goto err_free_fname;

		file->size = file->poolset->poolsize;

		/* get modification time from the first part of first replica */
		path = file->poolset->replica[0]->part[0].path;
		file->addr = file->poolset->replica[0]->part[0].addr;
	} else {
		int oflag = rdonly ? O_RDONLY : O_RDWR;
		file->fd = util_file_open(fname, NULL, 0, oflag);
		file->size = params->size;
	}

	struct stat buf;
	if (stat(path, &buf)) {
		ERR("%s", path);
		goto err_close_poolset;
	}

	file->mtime = buf.st_mtime;
	file->mode = buf.st_mode;
	return file;

err_close_poolset:
	if (params->type != POOL_TYPE_BTT)
		util_poolset_close(file->poolset, 0);
	else
		close(file->fd);
err_free_fname:
	free(file->fname);
err:
	free(file);
	return NULL;
}

/*
 * pool_data_alloc -- allocate pool data and open set_file
 */
struct pool_data *
pool_data_alloc(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	struct pool_data *pool = malloc(sizeof(*pool));
	if (!pool) {
		ERR("!malloc");
		return NULL;
	}

	TAILQ_INIT(&pool->arenas);
	pool->narenas = 0;
	pool->blk_no_layout = 0;
	pool->uuid_op = UUID_NOP;
	pool->set_file = NULL;
	pool->bttc.valid = false;

	if (pool_params_parse(ppc, &pool->params, 0))
		goto error;

	int rdonly = CHECK_WITHOUT_FIXING(ppc);
	pool->set_file = pool_set_file_open(ppc->path, &pool->params, rdonly);
	if (!pool->set_file)
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
		util_poolset_close(file->poolset, 0);
	else if (file->addr) {
		munmap(file->addr, file->size);
		close(file->fd);
	} else if (file->fd)
		close(file->fd);

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

	if (pool->set_file)
		pool_set_file_close(pool->set_file);

	while (!TAILQ_EMPTY(&pool->arenas)) {
		struct arena *arenap = TAILQ_FIRST(&pool->arenas);
		if (arenap->map)
			free(arenap->map);
		if (arenap->flog)
			free(arenap->flog);

		TAILQ_REMOVE(&pool->arenas, arenap, next);
		free(arenap);
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
 */
int
pool_read(struct pool_data *pool, void *buff, size_t nbytes, uint64_t off)
{
	if (off + nbytes > pool->set_file->size)
		return -1;

	if (pool->params.type != POOL_TYPE_BTT)
		memcpy(buff, (char *)pool->set_file->addr + off, nbytes);
	else {
		if (pool_btt_lseek(pool, (off_t)off, SEEK_SET) == -1)
			return -1;
		if ((size_t)pool_btt_read(pool, buff, nbytes) != nbytes)
			return -1;
	}

	return 0;
}

/*
 * pool_write -- write to pool set file or regular file
 */
int
pool_write(struct pool_data *pool, const void *buff, size_t nbytes,
	uint64_t off)
{
	if (off + nbytes > pool->set_file->size)
		return -1;

	if (pool->params.type != POOL_TYPE_BTT)
		memcpy((char *)pool->set_file->addr + off, buff, nbytes);
	else {
		if (pool_btt_lseek(pool, (off_t)off, SEEK_SET) == -1)
			return -1;
		if ((size_t)pool_btt_write(pool, buff, nbytes) != nbytes)
			return -1;
	}

	return 0;
}

/* arbitrary size of a buffer used to read and write to / from files */
#define READ_WRITE_BUFFER_SIZE (128 * 1024 * 1024)

/*
 * pool_copy -- make a copy of the pool
 */
int
pool_copy(struct pool_data *pool, const char *dst_path)
{
	struct pool_set_file *file = pool->set_file;
	int dfd = util_file_create(dst_path, file->size, 0);
	if (dfd < 0)
		return -1;

	int result = 0;
	struct stat stat_buf;
	if (stat(file->fname, &stat_buf)) {
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

	if (pool->params.type != POOL_TYPE_BTT) {
		void *saddr = pool_set_file_map(file, 0);
		memcpy(daddr, saddr, file->size);
		goto out_unmap;
	}

	void *buf = malloc(READ_WRITE_BUFFER_SIZE);
	if (buf == NULL) {
		ERR("!malloc");
		result = -1;
		goto out_unmap;
	}

	pool_btt_lseek(pool, 0, SEEK_SET);
	ssize_t buf_read = 0;
	void *dst = daddr;
	while ((buf_read = pool_btt_read(pool, buf, READ_WRITE_BUFFER_SIZE))) {
		if (buf_read == -1)
			break;

		memcpy(dst, buf, (size_t)buf_read);
		dst  = (void *)((ssize_t)dst + buf_read);
	}

	free(buf);
out_unmap:
	munmap(daddr, file->size);
out_close:
	if (dfd >= 0)
		close(dfd);
	return result;
}

/*
 * pool_memset -- memset pool part described by off and count
 */
int
pool_memset(struct pool_data *pool, uint64_t off, int c, size_t count)
{
	int result = 0;

	if (pool->params.type != POOL_TYPE_BTT)
		memset((char *)off, 0, count);
	else {
		pool_btt_lseek(pool, (off_t)off, SEEK_SET);
		size_t zero_size = min(count, READ_WRITE_BUFFER_SIZE);
		void *buf = malloc(zero_size);
		if (!buf) {
			ERR("!malloc");
			return -1;
		}
		memset(buf, c, zero_size);
		ssize_t nwrite = 0;
		do {
			zero_size = min(zero_size, count);
			nwrite = pool_btt_write(pool, buf, zero_size);
			if (nwrite < 0) {
				result = -1;
				break;
			}
			count -= (size_t)nwrite;
		} while (count > 0);

		free(buf);
	}

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
pool_set_file_map_headers(struct pool_set_file *file, int rdonly)
{
	if (!file->poolset)
		return -1;

	int flags = rdonly ? MAP_PRIVATE : MAP_SHARED;
	for (unsigned r = 0; r < file->poolset->nreplicas; r++) {
		struct pool_replica *rep = file->poolset->replica[r];
		for (unsigned p = 0; p < rep->nparts; p++) {
			struct pool_set_part *part = &rep->part[p];
			if (util_map_hdr(part, flags)) {
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
	case POOL_TYPE_LOG:
		return LOG_HDR_SIG;
	case POOL_TYPE_BLK:
		return BLK_HDR_SIG;
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
	case POOL_TYPE_LOG:
		hdrp->major = LOG_FORMAT_MAJOR;
		hdrp->compat_features = LOG_FORMAT_COMPAT;
		hdrp->incompat_features = LOG_FORMAT_INCOMPAT;
		hdrp->ro_compat_features = LOG_FORMAT_RO_COMPAT;
		break;
	case POOL_TYPE_BLK:
		hdrp->major = BLK_FORMAT_MAJOR;
		hdrp->compat_features = BLK_FORMAT_COMPAT;
		hdrp->incompat_features = BLK_FORMAT_INCOMPAT;
		hdrp->ro_compat_features = BLK_FORMAT_RO_COMPAT;
		break;
	case POOL_TYPE_OBJ:
		hdrp->major = OBJ_FORMAT_MAJOR;
		hdrp->compat_features = OBJ_FORMAT_COMPAT;
		hdrp->incompat_features = OBJ_FORMAT_INCOMPAT;
		hdrp->ro_compat_features = OBJ_FORMAT_RO_COMPAT;
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
	if (memcmp(hdrp->signature, LOG_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return POOL_TYPE_LOG;
	else if (memcmp(hdrp->signature, BLK_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return POOL_TYPE_BLK;
	else if (memcmp(hdrp->signature, OBJ_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return POOL_TYPE_OBJ;
	else
		return POOL_TYPE_UNKNOWN;
}

/*
 * pool_btt_info_valid -- check consistency of BTT Info header
 */
int
pool_btt_info_valid(struct btt_info *infop)
{
	if (memcmp(infop->sig, BTTINFO_SIG, BTTINFO_SIG_LEN) != 0)
		return 0;

	return util_checksum(infop, sizeof(*infop), &infop->checksum, 0);
}

/*
 * pool_blk_get_first_valid_arena -- get first valid BTT Info in arena
 */
int
pool_blk_get_first_valid_arena(struct pool_data *pool, struct arena *arenap)
{
	arenap->zeroed = true;
	uint64_t offset = pool_get_first_valid_btt(pool, &arenap->btt_info,
		2 * BTT_ALIGNMENT, &arenap->zeroed);

	if (offset != 0) {
		arenap->offset = offset;
		arenap->valid = true;
		return 1;
	}

	return 0;
}

/*
 * pool_next_arena_offset --  get offset of next arena
 *
 * Calculated offset is theoretical. Function does not check if such arena can
 * exist.
 */
uint64_t
pool_next_arena_offset(struct pool_data *pool, uint64_t offset)
{
	uint64_t lastoff = (pool->set_file->size & ~(BTT_ALIGNMENT - 1));
	uint64_t nextoff = min(offset + BTT_MAX_ARENA, lastoff);
	return nextoff;
}

/*
 * pool_get_first_valid_btt -- return offset to first valid BTT Info
 *
 * - Return offset to valid BTT Info header in pool file.
 * - Start looking from given offset.
 * - Convert BTT Info header to host endianness.
 * - Return the BTT Info header by pointer.
 * - If zeroed pointer provided would check if all checked BTT Info are zeroed
 *	which is useful for BLK pools
 */
uint64_t
pool_get_first_valid_btt(struct pool_data *pool, struct btt_info *infop,
	uint64_t offset, bool *zeroed)
{
	/* if we have valid arena get BTT Info header from it */
	if (pool->narenas != 0) {
		struct arena *arenap = TAILQ_FIRST(&pool->arenas);
		memcpy(infop, &arenap->btt_info, sizeof(*infop));
		return arenap->offset;
	}

	const size_t info_size = sizeof(*infop);

	/* theoretical offsets to BTT Info header and backup */
	uint64_t offsets[2] = {offset, 0};

	while (offsets[0] < pool->set_file->size) {
		/* calculate backup offset */
		offsets[1] = pool_next_arena_offset(pool, offsets[0]) -
			info_size;

		/* check both offsets: header and backup */
		for (int i = 0; i < 2; ++i) {
			if (pool_read(pool, infop, info_size, offsets[i]))
				continue;

			/* check if all possible BTT Info are zeroed */
			if (zeroed)
				*zeroed &= util_is_zeroed((const void *)infop,
					info_size);

			/* check if read BTT Info is valid */
			if (pool_btt_info_valid(infop)) {
				btt_info_convert2h(infop);
				return offsets[i];
			}
		}

		/* jump to next arena */
		offsets[0] += BTT_MAX_ARENA;
	}

	return 0;
}
