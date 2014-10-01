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
 * trn.c -- transactional memory pool entry points for libpmem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <uuid/uuid.h>
#include <endian.h>
#include <libpmem.h>
#include "pmem.h"
#include "util.h"
#include "out.h"
#include "trn.h"

/*
 * trn_init -- load-time initialization for trn
 *
 * Called automatically by the run-time loader.
 */
__attribute__((constructor))
static void
trn_init(void)
{
	out_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR);
	LOG(3, NULL);
	util_init();
}

/*
 * pmemtrn_map -- map a transactional memory pool
 */
PMEMtrn *
pmemtrn_map(int fd)
{
	LOG(3, "fd %d", fd);

	struct stat stbuf;

	if (fstat(fd, &stbuf) < 0) {
		LOG(1, "!fstat");
		return NULL;
	}

	if (stbuf.st_size < PMEMTRN_MIN_POOL) {
		LOG(1, "size %zu smaller than %zu",
				stbuf.st_size, PMEMTRN_MIN_POOL);
		errno = EINVAL;
		return NULL;
	}

	void *addr;
	if ((addr = util_map(fd, stbuf.st_size, 0)) == NULL)
		return NULL;	/* util_map() set errno, called LOG */

	/* check if the mapped region is located in persistent memory */
	int is_pmem = pmem_is_pmem(addr, stbuf.st_size);

	/* opaque info lives at the beginning of mapped memory pool */
	struct pmemtrn *ptp = addr;

	struct pool_hdr hdr;
	memcpy(&hdr, &ptp->hdr, sizeof (hdr));

	if (util_convert_hdr(&hdr)) {
		/*
		 * valid header found
		 */
		if (strncmp(hdr.signature, TRN_HDR_SIG, POOL_HDR_SIG_LEN)) {
			LOG(1, "wrong pool type: \"%s\"", hdr.signature);

			errno = EINVAL;
			goto err;
		}

		if (hdr.major != TRN_FORMAT_MAJOR) {
			LOG(1, "trn pool version %d (library expects %d)",
				hdr.major, TRN_FORMAT_MAJOR);

			errno = EINVAL;
			goto err;
		}

		int retval = util_feature_check(&hdr, TRN_FORMAT_INCOMPAT,
							TRN_FORMAT_RO_COMPAT,
							TRN_FORMAT_COMPAT);
		if (retval < 0)
		    goto err;
		else if (retval == 0) {
			/* XXX switch to read-only mode */
		}
	} else {
		/*
		 * no valid header was found
		 */
		LOG(3, "creating new trn memory pool");

		struct pool_hdr *hdrp = &ptp->hdr;

		memset(hdrp, '\0', sizeof (*hdrp));
		strncpy(hdrp->signature, TRN_HDR_SIG, POOL_HDR_SIG_LEN);
		hdrp->major = htole32(TRN_FORMAT_MAJOR);
		hdrp->compat_features = htole32(TRN_FORMAT_COMPAT);
		hdrp->incompat_features = htole32(TRN_FORMAT_INCOMPAT);
		hdrp->ro_compat_features = htole32(TRN_FORMAT_RO_COMPAT);
		uuid_generate(hdrp->uuid);
		hdrp->crtime = htole64((uint64_t)time(NULL));
		util_checksum(hdrp, sizeof (*hdrp), &hdrp->checksum, 1);
		hdrp->checksum = htole64(hdrp->checksum);

		/* store pool's header */
		libpmem_persist(is_pmem, hdrp, sizeof (*hdrp));

		/* XXX create rest of required metadata */
	}

	/* use some of the memory pool area for run-time info */
	ptp->addr = addr;
	ptp->size = stbuf.st_size;

	/*
	 * If possible, turn off all permissions on the pool header page.
	 *
	 * The prototype PMFS doesn't allow this when large pages are in
	 * use. It is not considered an error if this fails.
	 */
	util_range_none(addr, sizeof (struct pool_hdr));

	/* the rest should be kept read-only for debug version */
	RANGE_RO(addr + sizeof (struct pool_hdr),
			stbuf.st_size - sizeof (struct pool_hdr));

	LOG(3, "ptp %p", ptp);
	return ptp;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	util_unmap(addr, stbuf.st_size);
	errno = oerrno;
	return NULL;
}

/*
 * pmemtrn_unmap -- unmap a transactional memory pool
 */
void
pmemtrn_unmap(PMEMtrn *ptp)
{
	LOG(3, "ptp %p", ptp);

	util_unmap(ptp->addr, ptp->size);
}

/*
 * pmemtrn_check -- transactional memory pool consistency check
 */
int
pmemtrn_check(const char *path)
{
	LOG(3, "path \"%s\"", path);

	/* XXX stub */
	return 0;
}
