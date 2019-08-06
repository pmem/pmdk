/*
 * Copyright 2018-2019, Intel Corporation
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
 * os_auto_flush_linux.c -- Linux abstraction layer for auto flush detection
 */

#define _GNU_SOURCE

#include <inttypes.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include "out.h"
#include "os.h"
#include "fs.h"
#include "os_auto_flush.h"

#define BUS_DEVICE_PATH "/sys/bus/nd/devices"
#define PERSISTENCE_DOMAIN "persistence_domain"
#define DOMAIN_VALUE_LEN 32

/*
 * check_cpu_cache -- (internal) check if file contains "cpu_cache" entry
 */
static int
check_cpu_cache(const char *domain_path)
{
	LOG(3, "domain_path: %s", domain_path);

	char domain_value[DOMAIN_VALUE_LEN];
	int domain_fd;
	int cpu_cache = 0;

	if ((domain_fd = os_open(domain_path, O_RDONLY)) < 0) {
		LOG(1, "!open(\"%s\", O_RDONLY)", domain_path);
			goto end;
	}
	ssize_t len = read(domain_fd, domain_value,
			DOMAIN_VALUE_LEN);

	if (len < 0) {
		ERR("!read(%d, %p, %d)", domain_fd,
			domain_value, DOMAIN_VALUE_LEN);
		cpu_cache = -1;
		goto end;
	} else if (len == 0) {
		errno = EIO;
		ERR("read(%d, %p, %d) empty string",
			domain_fd, domain_value,
			DOMAIN_VALUE_LEN);
		cpu_cache = -1;
		goto end;
	} else if (domain_value[len - 1] != '\n') {
		ERR("!read(%d, %p, %d) invalid format",
			domain_fd, domain_value,
			DOMAIN_VALUE_LEN);
		cpu_cache = -1;
		goto end;
	}

	domain_value[len - 1] = '\0';
	LOG(15, "detected persistent_domain: %s", domain_value);
	if (strcmp(domain_value, "cpu_cache") == 0) {
		LOG(15, "cpu_cache in persistent_domain: %s", domain_path);
		cpu_cache = 1;
	} else {
		LOG(15, "cpu_cache not in persistent_domain: %s", domain_path);
		cpu_cache = 0;
	}

end:
	if (domain_fd >= 0)
		os_close(domain_fd);
	return cpu_cache;
}

/*
 * check_domain_in_region -- (internal) check if region
 * contains persistence_domain file
 */
static int
check_domain_in_region(const char *region_path)
{
	LOG(3, "region_path: %s", region_path);

	struct fs *reg = NULL;
	struct fs_entry *reg_entry;
	char domain_path[PATH_MAX];
	int cpu_cache = 0;

	reg = fs_new(region_path);
	if (reg == NULL) {
		ERR("!fs_new: \"%s\"", region_path);
		cpu_cache = -1;
		goto end;
	}

	while ((reg_entry = fs_read(reg)) != NULL) {
		/*
		 * persistence_domain has to be a file type entry
		 * and it has to be first level child for region;
		 * there is no need to run into deeper levels
		 */
		if (reg_entry->type != FS_ENTRY_FILE ||
				strcmp(reg_entry->name,
				PERSISTENCE_DOMAIN) != 0 ||
				reg_entry->level != 1)
			continue;

		int ret = snprintf(domain_path, PATH_MAX,
			"%s/"PERSISTENCE_DOMAIN,
			region_path);
		if (ret < 0) {
			ERR("snprintf(%p, %d,"
				"%s/"PERSISTENCE_DOMAIN", %s): %d",
				domain_path, PATH_MAX,
				region_path, region_path, ret);
			cpu_cache = -1;
			goto end;
		}
		cpu_cache = check_cpu_cache(domain_path);
	}

end:
	if (reg)
		fs_delete(reg);
	return cpu_cache;
}


/*
 * os_auto_flush -- check if platform supports auto flush for all regions
 *
 * Traverse "/sys/bus/nd/devices" path to find all the nvdimm regions,
 * then for each region checks if "persistence_domain" file exists and
 * contains "cpu_cache" string.
 * If for any region "persistence_domain" entry does not exists, or its
 * context is not as expected, assume eADR is not available on this platform.
 */
int
os_auto_flush(void)
{
	LOG(15, NULL);

	char *device_path;
	int cpu_cache = 0;

	device_path = BUS_DEVICE_PATH;

	os_stat_t sdev;
	if (os_stat(device_path, &sdev) != 0 ||
		S_ISDIR(sdev.st_mode) == 0) {
		LOG(3, "eADR not supported");
		return cpu_cache;
	}

	struct fs *dev = fs_new(device_path);
	if (dev == NULL) {
		ERR("!fs_new: \"%s\"", device_path);
		return -1;
	}

	struct fs_entry *dev_entry;

	while ((dev_entry = fs_read(dev)) != NULL) {
		/*
		 * Skip if not a symlink, because we expect that
		 * region on sysfs path is a symlink.
		 * Skip if depth is different than 1, bacause region
		 * we are interested in should be the first level
		 * child for device.
		 */
		if ((dev_entry->type != FS_ENTRY_SYMLINK) ||
				!strstr(dev_entry->name, "region") ||
				dev_entry->level != 1)
			continue;

		LOG(15, "Start traversing region: %s", dev_entry->path);
		cpu_cache = check_domain_in_region(dev_entry->path);
		if (cpu_cache != 1)
			goto end;
	}

end:
	fs_delete(dev);
	return cpu_cache;
}
