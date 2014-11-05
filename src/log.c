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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * log.c -- log memory pool entry points for libpmem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <endian.h>
#include <libpmem.h>
#include <libpmemlog.h>
#include "util.h"
#include "out.h"
#include "log.h"

/*
 * pmemlog_pool_open_common -- (internal) open a log memory pool
 *
 * This routine does all the work, but takes a rdonly flag so internal
 * calls can map a read-only pool if required.
 */
static PMEMlogpool *
pmemlog_pool_open_common(const char *path, int rdonly)
{
	LOG(3, "path %s rdonly %d", path, rdonly);

	struct stat stbuf;
	if (stat(path, &stbuf) < 0) {
		LOG(1, "!stat %s", path);
		return NULL;
	}

	if (stbuf.st_size < PMEMLOG_MIN_POOL) {
		LOG(1, "size %lld smaller than %zu",
				(long long)stbuf.st_size, PMEMLOG_MIN_POOL);
		errno = EINVAL;
		return NULL;
	}

	int fd;
	if ((fd = open(path, O_RDWR)) < 0) {
		LOG(1, "!open %s", path);
		return NULL;
	}

	void *addr;
	if ((addr = util_map(fd, stbuf.st_size, rdonly)) == NULL) {
		close(fd);
		return NULL;	/* util_map() set errno, called LOG */
	}

	close(fd);

	/* check if the mapped region is located in persistent memory */
	int is_pmem = pmem_is_pmem(addr, stbuf.st_size);

	/* opaque info lives at the beginning of mapped memory pool */
	struct pmemlog *plp = addr;
	struct pool_hdr hdr;

	memcpy(&hdr, &plp->hdr, sizeof (hdr));

	if (util_convert_hdr(&hdr)) {
		/*
		 * valid header found
		 */
		if (strncmp(hdr.signature, LOG_HDR_SIG, POOL_HDR_SIG_LEN)) {
			LOG(1, "wrong pool type: \"%s\"", hdr.signature);
			errno = EINVAL;
			goto err;
		}

		if (hdr.major != LOG_FORMAT_MAJOR) {
			LOG(1, "log pool version %d (library expects %d)",
				hdr.major, LOG_FORMAT_MAJOR);
			errno = EINVAL;
			goto err;
		}

		uint64_t hdr_start = le64toh(plp->start_offset);
		uint64_t hdr_end = le64toh(plp->end_offset);
		uint64_t hdr_write = le64toh(plp->write_offset);

		if ((hdr_start != roundup(sizeof (*plp),
					LOG_FORMAT_DATA_ALIGN)) ||
			(hdr_end != stbuf.st_size) || (hdr_start > hdr_end)) {
			LOG(1, "wrong start/end offsets (start: %ju end: %ju), "
				"pool size %lld",
				hdr_start, hdr_end, (long long)stbuf.st_size);
			errno = EINVAL;
			goto err;
		}

		if ((hdr_write > hdr_end) || (hdr_write < hdr_start)) {
			LOG(1, "wrong write offset "
				"(start: %ju end: %ju write: %ju)",
				hdr_start, hdr_end, hdr_write);
			errno = EINVAL;
			goto err;
		}

		LOG(3, "start: %ju, end: %ju, write: %ju",
			hdr_start, hdr_end, hdr_write);

		int retval = util_feature_check(&hdr, LOG_FORMAT_INCOMPAT,
							LOG_FORMAT_RO_COMPAT,
							LOG_FORMAT_COMPAT);
		if (retval < 0)
		    goto err;
		else if (retval == 0)
		    rdonly = 1;
	} else {
		/*
		 * no valid header was found
		 */
		if (rdonly) {
			LOG(1, "read-only and no header found");
			errno = EROFS;
			goto err;
		}
		LOG(3, "creating new log memory pool");

		struct pool_hdr *hdrp = &plp->hdr;

		memset(hdrp, '\0', sizeof (*hdrp));
		strncpy(hdrp->signature, LOG_HDR_SIG, POOL_HDR_SIG_LEN);
		hdrp->major = htole32(LOG_FORMAT_MAJOR);
		hdrp->compat_features = htole32(LOG_FORMAT_COMPAT);
		hdrp->incompat_features = htole32(LOG_FORMAT_INCOMPAT);
		hdrp->ro_compat_features = htole32(LOG_FORMAT_RO_COMPAT);
		uuid_generate(hdrp->uuid);
		hdrp->crtime = htole64((uint64_t)time(NULL));
		util_checksum(hdrp, sizeof (*hdrp), &hdrp->checksum, 1);
		hdrp->checksum = htole64(hdrp->checksum);

		/* store pool's header */
		pmem_msync(hdrp, sizeof (*hdrp));

		/* create rest of required metadata */
		plp->start_offset = htole64(roundup(sizeof (*plp),
						LOG_FORMAT_DATA_ALIGN));
		plp->end_offset = htole64(stbuf.st_size);
		plp->write_offset = plp->start_offset;

		/* store non-volatile part of pool's descriptor */
		pmem_msync(&plp->start_offset, 3 * sizeof (uint64_t));
	}

	/*
	 * Use some of the memory pool area for run-time info.  This
	 * run-time state is never loaded from the file, it is always
	 * created here, so no need to worry about byte-order.
	 */
	plp->addr = addr;
	plp->size = stbuf.st_size;
	plp->rdonly = rdonly;
	plp->is_pmem = is_pmem;

	if ((plp->rwlockp = Malloc(sizeof (*plp->rwlockp))) == NULL) {
		LOG(1, "!Malloc for a RW lock");
		goto err;
	}

	if ((errno = pthread_rwlock_init(plp->rwlockp, NULL))) {
		LOG(1, "!pthread_rwlock_init");
		goto err_free;
	}

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	util_range_none(addr, sizeof (struct pool_hdr));

	/* the rest should be kept read-only (debug version only) */
	RANGE_RO(addr + sizeof (struct pool_hdr),
			stbuf.st_size - sizeof (struct pool_hdr));

	LOG(3, "plp %p", plp);
	return plp;

err_free:
	Free((void *)plp->rwlockp);
err:
	LOG(4, "error clean up");
	int oerrno = errno;
	util_unmap(addr, stbuf.st_size);
	errno = oerrno;
	return NULL;
}

/*
 * pmemlog_pool_open -- open a log memory pool
 */
PMEMlogpool *
pmemlog_pool_open(const char *path)
{
	LOG(3, "path %s", path);

	return pmemlog_pool_open_common(path, 0);
}

/*
 * pmemlog_pool_close -- close a log memory pool
 */
void
pmemlog_pool_close(PMEMlogpool *plp)
{
	LOG(3, "plp %p", plp);

	if ((errno = pthread_rwlock_destroy(plp->rwlockp)))
		LOG(1, "!pthread_rwlock_destroy");
	Free((void *)plp->rwlockp);
	util_unmap(plp->addr, plp->size);
}

/*
 * pmemlog_nbyte -- return usable size of a log memory pool
 */
size_t
pmemlog_nbyte(PMEMlogpool *plp)
{
	LOG(3, "plp %p", plp);

	if ((errno = pthread_rwlock_rdlock(plp->rwlockp))) {
		LOG(1, "!pthread_rwlock_rdlock");
		return (size_t)-1;
	}

	size_t size = le64toh(plp->end_offset) - le64toh(plp->start_offset);
	LOG(4, "plp %p nbyte %zu", plp, size);

	if ((errno = pthread_rwlock_unlock(plp->rwlockp)))
		LOG(1, "!pthread_rwlock_unlock");

	return size;
}

/*
 * pmemlog_persist -- (internal) persist data, then metadata
 *
 * On entry, the write lock should be held.
 */
static void
pmemlog_persist(PMEMlogpool *plp, uint64_t new_write_offset)
{
	uint64_t old_write_offset = le64toh(plp->write_offset);
	size_t length = new_write_offset - old_write_offset;

	/* unprotect the log space range (debug version only) */
	RANGE_RW(plp->addr + old_write_offset, length);

	/* persist the data */
	if (plp->is_pmem)
		pmem_persist(plp->addr + old_write_offset, length);
	else
		pmem_msync(plp->addr + old_write_offset, length);

	/* protect the log space range (debug version only) */
	RANGE_RO(plp->addr + old_write_offset, length);

	/* unprotect the pool descriptor (debug version only) */
	RANGE_RW(plp->addr + sizeof (struct pool_hdr), LOG_FORMAT_DATA_ALIGN);

	/* write the metadata */
	plp->write_offset = htole64(new_write_offset);

	/* persist the metadata */
	if (plp->is_pmem)
		pmem_persist(&plp->write_offset, sizeof (plp->write_offset));
	else
		pmem_msync(&plp->write_offset, sizeof (plp->write_offset));

	/* set the write-protection again (debug version only) */
	RANGE_RO(plp->addr + sizeof (struct pool_hdr), LOG_FORMAT_DATA_ALIGN);
}

/*
 * pmemlog_append -- add data to a log memory pool
 */
int
pmemlog_append(PMEMlogpool *plp, const void *buf, size_t count)
{
	int ret = 0;

	LOG(3, "plp %p buf %p count %zu", plp, buf, count);

	if (plp->rdonly) {
		LOG(1, "can't append to read-only log");
		errno = EROFS;
		return -1;
	}

	if ((errno = pthread_rwlock_wrlock(plp->rwlockp))) {
		LOG(1, "!pthread_rwlock_wrlock");
		return -1;
	}

	/* get the current values */
	uint64_t end_offset = le64toh(plp->end_offset);
	uint64_t write_offset = le64toh(plp->write_offset);

	if (write_offset >= end_offset) {
		/* no space left */
		errno = ENOSPC;
		ret = -1;
	} else {
		/* make sure we don't write past the available space */
		if (count > (end_offset - write_offset)) {
			errno = ENOSPC;
			ret = -1;
		} else {
			char *data = plp->addr;

			/*
			 * unprotect the log space range,
			 * where the new data will be stored
			 * (debug version only)
			 */
			RANGE_RW(&data[write_offset], count);

			memcpy(&data[write_offset], buf, count);

			/* protect the log space range (debug version only) */
			RANGE_RO(&data[write_offset], count);

			write_offset += count;
		}
	}

	/* persist the data and the metadata only if there was no error */
	if (ret == 0)
		pmemlog_persist(plp, write_offset);

	int oerrno = errno;
	if ((errno = pthread_rwlock_unlock(plp->rwlockp)))
		LOG(1, "!pthread_rwlock_unlock");
	errno = oerrno;

	return ret;
}

/*
 * pmemlog_appendv -- add gathered data to a log memory pool
 */
int
pmemlog_appendv(PMEMlogpool *plp, const struct iovec *iov, int iovcnt)
{
	LOG(3, "plp %p iovec %p iovcnt %d", plp, iov, iovcnt);

	int ret = 0; // success
	int i;

	if (plp->rdonly) {
		LOG(1, "can't append to read-only log");
		errno = EROFS;
		return -1;
	}

	if ((errno = pthread_rwlock_wrlock(plp->rwlockp))) {
		LOG(1, "!pthread_rwlock_wrlock");
		return -1;
	}

	/* get the current values */
	uint64_t end_offset = le64toh(plp->end_offset);
	uint64_t write_offset = le64toh(plp->write_offset);

	if (write_offset >= end_offset) {
		/* no space left */
		errno = ENOSPC;
		ret = -1;
	} else {
		char *data = plp->addr;
		uint64_t count = 0;
		char *buf;

		/* calculate required space */
		for (i = 0; i < iovcnt; ++i)
			count += iov[i].iov_len;

		/* check if there is enough free space */
		if (count > (end_offset - write_offset)) {
			errno = ENOSPC;
			ret = -1;
		} else {
			/* append the data */
			for (i = 0; i < iovcnt; ++i) {
				buf = iov[i].iov_base;
				count = iov[i].iov_len;

				/*
				 * unprotect the log space range,
				 * where the new data will be stored
				 * (debug version only)
				 */
				RANGE_RW(&data[write_offset], count);

				memcpy(&data[write_offset], buf, count);

				/*
				 * protect the log space range
				 * (debug version only)
				 */
				RANGE_RO(&data[write_offset], count);

				write_offset += count;
			}
		}
	}

	/* persist the data and the metadata only if there was no error */
	if (ret == 0)
		pmemlog_persist(plp, write_offset);

	int oerrno = errno;
	if ((errno = pthread_rwlock_unlock(plp->rwlockp)))
		LOG(1, "!pthread_rwlock_unlock");
	errno = oerrno;

	return ret;
}

/*
 * pmemlog_tell -- return current write point in a log memory pool
 */
off_t
pmemlog_tell(PMEMlogpool *plp)
{
	LOG(3, "plp %p", plp);

	if ((errno = pthread_rwlock_rdlock(plp->rwlockp))) {
		LOG(1, "!pthread_rwlock_rdlock");
		return (off_t)-1;
	}

	off_t wp = le64toh(plp->write_offset) - le64toh(plp->start_offset);
	LOG(4, "write offset %lld", (long long)wp);

	if ((errno = pthread_rwlock_unlock(plp->rwlockp)))
		LOG(1, "!pthread_rwlock_unlock");

	return wp;
}

/*
 * pmemlog_rewind -- discard all data, resetting a log memory pool to empty
 */
void
pmemlog_rewind(PMEMlogpool *plp)
{
	LOG(3, "plp %p", plp);

	if (plp->rdonly) {
		LOG(1, "can't rewind read-only log");
		errno = EROFS;
		return;
	}

	if ((errno = pthread_rwlock_wrlock(plp->rwlockp))) {
		LOG(1, "!pthread_rwlock_wrlock");
		return;
	}

	/* unprotect the pool descriptor (debug version only) */
	RANGE_RW(plp->addr + sizeof (struct pool_hdr), LOG_FORMAT_DATA_ALIGN);

	plp->write_offset = plp->start_offset;
	if (plp->is_pmem)
		pmem_persist(&plp->write_offset, sizeof (uint64_t));
	else
		pmem_msync(&plp->write_offset, sizeof (uint64_t));

	/* set the write-protection again (debug version only) */
	RANGE_RO(plp->addr + sizeof (struct pool_hdr), LOG_FORMAT_DATA_ALIGN);

	if ((errno = pthread_rwlock_unlock(plp->rwlockp)))
		LOG(1, "!pthread_rwlock_unlock");
}

/*
 * pmemlog_walk -- walk through all data in a log memory pool
 *
 * chunksize of 0 means process_chunk gets called once for all data
 * as a single chunk.
 */
void
pmemlog_walk(PMEMlogpool *plp, size_t chunksize,
	int (*process_chunk)(const void *buf, size_t len, void *arg), void *arg)
{
	LOG(3, "plp %p chunksize %zu", plp, chunksize);

	/*
	 * We are assuming that the walker doesn't change the data it's reading
	 * in place. We prevent everyone from changing the data behind our back
	 * until we are done with processing it.
	 */
	if ((errno = pthread_rwlock_rdlock(plp->rwlockp))) {
		LOG(1, "!pthread_rwlock_rdlock");
		return;
	}

	char *data = plp->addr;
	uint64_t write_offset = le64toh(plp->write_offset);
	uint64_t data_offset = le64toh(plp->start_offset);
	size_t len;

	if (chunksize == 0) {
		/* most common case: process everything at once */
		len = write_offset - data_offset;
		LOG(3, "length %zu", len);
		(*process_chunk)(&data[data_offset], len, arg);
	} else {
		/*
		 * Walk through the complete record, chunk by chunk.
		 * The callback returns 0 to terminate the walk.
		 */
		while (data_offset < write_offset) {
			len = MIN(chunksize, write_offset - data_offset);
			if (!(*process_chunk)(&data[data_offset], len, arg))
				break;
			data_offset += chunksize;
		}
	}

	if ((errno = pthread_rwlock_unlock(plp->rwlockp)))
		LOG(1, "!pthread_rwlock_unlock");
}

/*
 * pmemlog_pool_check -- log memory pool consistency check
 *
 * Returns true if consistent, zero if inconsistent, -1/error if checking
 * cannot happen due to other errors.
 */
int
pmemlog_pool_check(const char *path)
{
	LOG(3, "path \"%s\"", path);

	/* open the pool read-only */
	PMEMlogpool *plp = pmemlog_pool_open_common(path, 1);

	if (plp == NULL)
		return -1;	/* errno set by pmemlog_pool_open_common() */

	int consistent = 1;

	/* validate pool descriptor */
	uint64_t hdr_start = le64toh(plp->start_offset);
	uint64_t hdr_end = le64toh(plp->end_offset);
	uint64_t hdr_write = le64toh(plp->write_offset);

	if (hdr_start != roundup(sizeof (*plp), LOG_FORMAT_DATA_ALIGN)) {
		LOG(1, "wrong value of start_offset");
		consistent = 0;
	}

	if (hdr_end != plp->size) {
		LOG(1, "wrong value of end_offset");
		consistent = 0;
	}

	if (hdr_start > hdr_end) {
		LOG(1, "start_offset greater than end_offset");
		consistent = 0;
	}

	if (hdr_start > hdr_write) {
		LOG(1, "start_offset greater than write_offset");
		consistent = 0;
	}

	if (hdr_write > hdr_end) {
		LOG(1, "write_offset greater than end_offset");
		consistent = 0;
	}

	pmemlog_pool_close(plp);

	if (consistent)
		LOG(4, "pool consistency check OK");

	return consistent;
}
