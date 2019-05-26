/*
 * Copyright 2019, IBM Corporation
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

#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <libpmem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/limits.h>

#include "out.h"
#include "pmem.h"
#include "ppc64_ops.h"
#include "cpuinfo.h"

#define CPU_DT_PATH "/proc/device-tree/cpus"
#define DCACHE_BLOCK_SIZE "d-cache-block-size"
#define DCACHE_SIZE "d-cache-size"

/* Pointer to glocal cpu context */
const struct cpu_info *ppc_cpuinfo = NULL;

/*
 * Read upto 'size' bytes from file located at 'path' and store
 * it into location pointed to by 'buffer'
 */

static int read_file_contents(const char *path, void *buffer, size_t size)
{
	int fd, ret = -1;
	ssize_t rd;

	LOG(2, NULL);

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -1;

	/* read the 'size' bytes from the file */
	rd = read(fd, buffer, size);
	if (rd == (ssize_t)size) {
		ret = 0;
	}

	close(fd);

	return ret;
}

/*
 * Read and return the L1 d-cache block size from device tree.
 * In case of an error errno is set appropriatly and '-1' is returned.
 */
static int read_cache_info(struct cpu_info *cpuinfo)
{
	char path[PATH_MAX];
	struct dirent *dent;
	int err = 0;
	uint32_t size = 0;
	DIR *d;

	LOG(3, NULL);

	cpuinfo->page_size = (size_t)sysconf(_SC_PAGESIZE);

	/*
	 * All shipping POWER8 machines have a firmware bug that
	 * puts incorrect information in the device-tree. This will
	 * be (hopefully) fixed for future chips but for now hard
	 * code the values if we are running on one of these
	 */
	if (PVR_VER(cpuinfo->pvr) == PVR_POWER8 ||
	    PVR_VER(cpuinfo->pvr) == PVR_POWER8E ||
	    PVR_VER(cpuinfo->pvr) == PVR_POWER8NVL) {
		cpuinfo->d_cache_block_size = 128;
		cpuinfo->d_cache_size = 0x10000;
		cpuinfo->blocks_per_page = cpuinfo->page_size /
			cpuinfo->d_cache_block_size;
		return 0;
	}

	/* Else read the cache info from device tree */
	d = opendir(CPU_DT_PATH);
	if (d == NULL)
		return -errno;

	errno = ENOENT;
	while ((dent = readdir(d)) != NULL) {

		/*
		 * Check if dentry is a directory and possible cpu device node
		 */
		if (dent->d_type != DT_DIR ||
		    strstr(dent->d_name, "PowerPC,") != dent->d_name)
			continue;
		/*
		 * Form a path for the device tree attribute file and try
		 * opening the file to read its contents.
		 */
		snprintf(path, sizeof(path),
			    "%s/%s/%s", CPU_DT_PATH, dent->d_name,
			    DCACHE_BLOCK_SIZE);
		path[sizeof(path) - 1] = '\0';
		LOG(3, "Reading cache block size from '%s'", path);

		/* Read d-cache block size first */
		err = read_file_contents(path, &size, sizeof(size));
		if (!err) {
			cpuinfo->d_cache_block_size =
				be32toh(size);
		} else if (errno == ENOENT) {
			/* In case file doesnt exist then move on */
			continue;
		} else {
			ERR("Unable to read d-cache block-size, Err=%d",
			    errno);
			break;
		}

		/* Now read cache size */
		snprintf(path, sizeof(path),
			    "%s/%s/%s", CPU_DT_PATH, dent->d_name,
			    DCACHE_SIZE);
		path[sizeof(path) - 1] = '\0';
		LOG(3, "Reading cache block size from '%s'", path);

		err = read_file_contents(path, &size, sizeof(size));
		if (!err) {
			cpuinfo->d_cache_size =
				be32toh(size);
		} else if (errno == ENOENT) {
			/* In case file doesnt exist then move on */
			continue;
		} else {
			ERR("Unable to read d-cache size, Err=%d",
			    errno);
			break;
		}

		cpuinfo->blocks_per_page = cpuinfo->page_size /
			cpuinfo->d_cache_block_size;

		/* Mark success and break the loop */
		errno = 0;
		break;
	}

	closedir(d);
	return errno ? -1 : 0;
}

/*
 * Populate 'cpuinfo' with cache details of the cpu.
 * These value are fetched from device tree and special purporse
 * registers.
 */
void
ppc_populate_cpu_info(struct cpu_info *cpuinfo)
{
	LOG(3, NULL);

	/* Set the cpuinfo PVR first */
	cpuinfo->pvr = mfpvr();
	LOG(2, "PVR = 0x%016lX", cpuinfo->pvr);

	/* Read the processor cache info */
	if (read_cache_info(cpuinfo)) {
		FATAL("Unable to read cpu cache info. Error=%d", errno);
	}
}
