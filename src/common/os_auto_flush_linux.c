/*
 * Copyright 2018, Intel Corporation
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
#include "out.h"
#include "os.h"
#include "fs.h"
#include "os_auto_flush.h"

#define BUS_DEVICE_PATH "/sys/bus/nd/devices"
#define PERSISTENCE_DOMAIN "persistence_domain"
#define DOMAIN_VALUE_LEN 32

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
	char region_path[PATH_MAX];
	char domain_path[PATH_MAX];
	char domain_value[DOMAIN_VALUE_LEN];
	int domain_fd = -1;
	int CPU_cache = 0;
	struct fs *reg = NULL;

	device_path = BUS_DEVICE_PATH;

	os_stat_t sdev;
	if (os_stat(device_path, &sdev) != 0 ||
		S_ISDIR(sdev.st_mode) == 0) {
		LOG(3, "eADR not supported");
		return CPU_cache;
	}

	struct fs *dev = fs_new(device_path);
	if (dev == NULL) {
		ERR("!fs_new: \"%s\"", device_path);
		return -1;
	}

	struct fs_entry *dev_entry;
	struct fs_entry *reg_entry;

	while ((dev_entry = fs_read(dev)) !=  NULL) {
		/*
		 * skip if not a symlink,
		 * does not contain region in name,
		 * or depth different than 1
		 */
		if ((dev_entry->type != FS_ENTRY_SYMLINK) ||
				!strstr(dev_entry->name, "region") ||
				dev_entry->level != 1)
			continue;

		LOG(15, "start %s traversing", dev_entry->name);
		int ret = snprintf(region_path, PATH_MAX, "%s/%s",
					device_path, dev_entry->name);
		if (ret < 0) {
			ERR("!snprintf(%p, %d, ""%s/%s"", %s)",
				region_path, PATH_MAX, device_path,
				dev_entry->name, dev_entry->name);
			CPU_cache = -1;
			goto end_dev;
		}
		reg = fs_new(region_path);
		if (reg == NULL) {
			ERR("!fs_new: \"%s\"", region_path);
			CPU_cache = -1;
			goto end_dev;
		}
		while ((reg_entry = fs_read(reg)) != NULL) {
			/*
			 * skip if not a file,
			 * name does not equal persistence_domain,
			 * or depth different than 1
			 */
			if (reg_entry->type != FS_ENTRY_FILE ||
					strcmp(reg_entry->name,
					PERSISTENCE_DOMAIN) != 0 ||
					reg_entry->level != 1)
				continue;

			ret = snprintf(domain_path, PATH_MAX,
				"%s/"PERSISTENCE_DOMAIN,
				region_path);
			if (ret < 0) {
				ERR("!snprintf(%p, %d,"
					"%s/"PERSISTENCE_DOMAIN", %s)",
					domain_path, PATH_MAX,
					region_path, region_path);
				CPU_cache = -1;
				goto end_reg;
			}

			if ((domain_fd = os_open(domain_path, O_RDONLY)) < 0) {
				LOG(1, "!open(\"%s\", O_RDONLY)", domain_path);
					CPU_cache = 0;
					goto end_reg;
			}
			ssize_t len = read(domain_fd, domain_value,
					DOMAIN_VALUE_LEN);

			if (len == -1) {
				ERR("!read(%d, %p, %d)", domain_fd,
					domain_value, DOMAIN_VALUE_LEN);
				CPU_cache =  -1;
				goto end;
			} else if (domain_value[len - 1] != '\n') {
				ERR("!read(%d, %p, %d) invalid format",
					domain_fd, domain_value,
					DOMAIN_VALUE_LEN);
				CPU_cache = -1;
				goto end;
			}
			os_close(domain_fd);

			LOG(3, "detected persistent_domain: %s", domain_value);
			if (strncmp(domain_value, "cpu_cache",
					strlen("cpu_cache")) == 0) {
				CPU_cache = 1;
				continue;
			} else {
				LOG(3, "cpu_cache not in persistent_domain");
				CPU_cache = 0;
				goto end;
			}
		}
		fs_delete(reg);
		reg = NULL;
	}

end:
	if (domain_fd >= 0)
		os_close(domain_fd);
end_reg:
	if (reg)
		fs_delete(reg);
end_dev:
	fs_delete(dev);
	return CPU_cache;

}
