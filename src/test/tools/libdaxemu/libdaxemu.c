/*
 * Copyright 2016-2017, Intel Corporation
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
 * libdaxemu.c -- Device DAX emulation library
 *
 * The library allows to run tests that require access to Device DAX
 * devices on a system that does not have DAX support, or if user has
 * no privileges to create/open Device DAX.
 *
 * The emulation is based on interposing each access to the files like:
 *   /dev/daxX.Y
 *   /sys/dev/char/M.N/device/align
 *   /sys/dev/char/M.N/size
 *   /sys/dev/char/M.N/subsystem
 *
 * Access to /dev/daxX.Y is redirected to the fake file specified
 * in a config file, so the device can be open, mapped to memory, etc.,
 * and its content is preserved between opens.
 * The library simulates the behavior of selected file I/O routines
 * when used on Device DAX.
 *
 * Files on sysfs are created on demand (temp files) and populated
 * with the appropriate data (i.e. device size or alignment).
 *
 * TBD:
 *  - mprotect/mmap - Device DAX behavior/limitations
 *  - support for various kernel versions (default = installed kernel ver)
 *      alignment format
 *      misaligned mmap/mprotect behavior
 *      USC
 *      bad blocks
 *      deep flush
 *      ...
 *  - tracking fd/addr => device mapping (tree/hash table)
 *  - mode / access permissions
 *  - unit tests simulating various kernels' behavior
 *  - unlimited number of file desctiptors for one Device DAX
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "os.h"
#include "util.h"
#include "out.h"
#include "file.h"
#include "queue.h"

/* need to intercept those for nondebug binaries */
int __open_2(const char *path, int flags);
char *__realpath_chk(const char *path, char *rpath, size_t rpathlen);

#define MAX_LINE 4096
#define MAX_FD 16	/* XXX: assume each device can be open max. 16 times */

#define DAXEMU_LOG_PREFIX "libdaxemu"
#define DAXEMU_LOG_LEVEL_VAR "DAXEMU_LOG_LEVEL"
#define DAXEMU_LOG_FILE_VAR "DAXEMU_LOG_FILE"
#define DAXEMU_CFG_FILE_VAR "DAXEMU_CFG_FILE"

#define DAXEMU_SYS_PATH "/sys/dev/char/%d:%d"

#define DEVICE_CHAR_PREFIX "/sys/dev/char"
#define DEVICE_DAX_PREFIX "/sys/class/dax"

struct devdax {
	char path[PATH_MAX];
	char fake_path[PATH_MAX];
	char sys_path[PATH_MAX]; /* /sys/dev/char/%d:%d */
	size_t length;
	size_t alignment;
	unsigned major;
	unsigned minor;
	int fd[MAX_FD];
};

static unsigned Version;
static int Ndev;
static struct devdax *Devices;

static int (*Creat_func)(const char *path, mode_t mode);
static int (*Open_func)(const char *path, int flags, mode_t mode);
static int (*Close_func)(int fd);
static void *(*Mmap_func)(void *addr, size_t length, int prot, int flags,
	int fd, off_t offset);
static int (*Munmap_func)(void *addr, size_t length);
static int (*Msync_func)(void *addr, size_t length, int flags);
static int (*Mprotect_func)(void *addr, size_t length, int prot);
static int (*Xstat_func)(int ver, const char *path, struct stat *st);
static int (*Fxstat_func)(int ver, int fd, struct stat *st);
static char *(*Realpath_func)(const char *path, char *rpath);
static int (*Access_func)(const char *path, int mode);
static ssize_t (*Read_func)(int fd, void *buf, size_t count);
static ssize_t (*Write_func)(int fd, const void *buf, size_t count);
static ssize_t (*Pread_func)(int fd, void *buf, size_t count, off_t offset);
static ssize_t (*Pwrite_func)(int fd, const void *buf, size_t count,
	off_t offset);
static off_t (*Lseek_func)(int fd, off_t offset, int whence);
static int (*Fsync_func)(int fd);
static int (*Ftruncate_func)(int fd, off_t length);
static int (*Posix_fallocate_func)(int fd, off_t offset, off_t len);

/*
 * this structure tracks the file mappings outstanding per file handle
 */
struct map_tracker {
	SORTEDQ_ENTRY(map_tracker) entry;
	const void *base_addr;
	const void *end_addr;
};

static SORTEDQ_HEAD(map_list_head, map_tracker) Mmap_list =
		SORTEDQ_HEAD_INITIALIZER(Mmap_list);


/*
 * util_range_comparer -- (internal) compares the two mapping trackers
 */
static intptr_t
util_range_comparer(struct map_tracker *a, struct map_tracker *b)
{
	return ((intptr_t)a->base_addr - (intptr_t)b->base_addr);
}

/*
 * util_range_find -- find the map tracker for given address range
 *
 * Returns the first entry at least partially overlapping given range.
 * It's up to the caller to check whether the entry exactly matches the range,
 * or if the range spans multiple entries.
 * The caller is also responsible for acquiring/releasing a lock on
 * the map tracking list.
 */
static struct map_tracker *
util_range_find(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	void *end = (char *)addr + len;

	struct map_tracker *mt;
	SORTEDQ_FOREACH(mt, &Mmap_list, entry) {
		if (addr < mt->end_addr &&
		    (addr >= mt->base_addr || end > mt->base_addr))
			return mt;

		/* break if there is no chance to find matching entry */
		if (addr < mt->base_addr)
			break;
	}

	return NULL;
}

/*
 * util_range_register -- add a memory range into a map tracking list
 */
static int
util_range_register(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	int ret = 0;

	/* check if not tracked already */
	struct map_tracker *mt = util_range_find(addr, len);
	ASSERTeq(mt, NULL);

	mt = malloc(sizeof(struct map_tracker));
	if (mt == NULL) {
		ERR("!Malloc");
		ret = -1;
		goto err;
	}

	mt->base_addr = addr;
	mt->end_addr = (void *)((char *)addr + len);

	SORTEDQ_INSERT(&Mmap_list, mt, entry, struct map_tracker,
			util_range_comparer);

err:
	return ret;
}

/*
 * util_range_split -- (internal) remove or split a map tracking entry
 */
static int
util_range_split(struct map_tracker *mt, const void *addr, const void *end)
{
	LOG(3, "begin %p end %p", addr, end);

	ASSERTne(mt, NULL);

	struct map_tracker *mtb = NULL;
	struct map_tracker *mte = NULL;

	/*
	 * 1)    b    e           b     e
	 *    xxxxxxxxxxxxx => xxx.......xxxx  -  mtb+mte
	 * 2)       b     e           b     e
	 *    xxxxxxxxxxxxx => xxxxxxx.......  -  mtb
	 * 3) b     e          b      e
	 *    xxxxxxxxxxxxx => ........xxxxxx  -  mte
	 * 4) b           e    b            e
	 *    xxxxxxxxxxxxx => ..............  -  <none>
	 */

	if (addr > mt->base_addr) {
		/* case #1/2 */
		/* new mapping at the beginning */
		mtb = malloc(sizeof(struct map_tracker));
		if (mtb == NULL) {
			ERR("!malloc");
			goto err;
		}

		mtb->base_addr = mt->base_addr;
		mtb->end_addr = addr;
	}

	if (end < mt->end_addr) {
		/* case #1/3 */
		/* new mapping at the end */
		mte = malloc(sizeof(struct map_tracker));
		if (mte == NULL) {
			ERR("!malloc");
			goto err;
		}

		mte->base_addr = end;
		mte->end_addr = mt->end_addr;
	}

	SORTEDQ_REMOVE(&Mmap_list, mt, entry);

	if (mtb) {
		SORTEDQ_INSERT(&Mmap_list, mtb, entry,
				struct map_tracker, util_range_comparer);
	}

	if (mte) {
		SORTEDQ_INSERT(&Mmap_list, mte, entry,
				struct map_tracker, util_range_comparer);
	}

	/* free entry for the original mapping */
	free(mt);
	return 0;

err:
	free(mtb);
	free(mte);
	return -1;
}

/*
 * util_range_unregister -- remove a memory range from map tracking list
 *
 * Remove the region between [begin,end].  If it's in a middle of the existing
 * mapping, it results in two new map trackers.
 */
static int
util_range_unregister(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	int ret = 0;

	void *end = (char *)addr + len;

	/* XXX optimize the loop */
	struct map_tracker *mt;
	while ((mt = util_range_find(addr, len)) != NULL) {
		if (util_range_split(mt, addr, end) != 0) {
			ret = -1;
			break;
		}
	}

	return ret;
}

/*
 * util_range_is_pmem -- return true if entire range is persistent memory
 */
static int
util_range_is_pmem(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	int retval = 1;

	do {
		struct map_tracker *mt = util_range_find(addr, len);
		if (mt == NULL) {
			LOG(4, "address not found %p", addr);
			retval = 0;
			break;
		}

		LOG(10, "range found - begin %p end %p",
				mt->base_addr, mt->end_addr);

		if (mt->base_addr > addr) {
			LOG(10, "base address doesn't match: %p > %p",
					mt->base_addr, addr);
			retval = 0;
			break;
		}

		uintptr_t map_len = ((uintptr_t)mt->end_addr - (uintptr_t)addr);
		if (map_len > len)
			map_len = len;
		len -= map_len;
		addr = (char *)addr + map_len;
	} while (len > 0);

	LOG(4, "returning %d", retval);
	return retval;
}

/*
 * is_dev_dax_path -- (internal) detects if path is emulated Device DAX
 *
 * Returns device index for Device DAX or -1 otherwise.
 */
static int
is_dev_dax_path(const char *path)
{
	LOG(3, "path %s", path);
	int ret = -1;

	for (int i = 0; i < Ndev; i++)
		if (strcmp(path, Devices[i].path) == 0) {
			ret = i;
			break;
		}

	LOG(4, "returning %d", ret);
	return ret;
}

/*
 * is_dev_dax_sys_path -- (internal) detects if path is /sys/dev/...
 *                        associated with emulated Device DAX
 *
 * Returns device index for Device DAX or -1 otherwise.
 */
static int
is_dev_dax_sys_path(const char *path)
{
	LOG(3, "path %s", path);
	int ret = -1;

	for (int i = 0; i < Ndev; i++)
		if (strncmp(path, Devices[i].sys_path,
				strlen(Devices[i].sys_path)) == 0) {
			ret = i;
			break;
		}

	LOG(4, "returning %d", ret);
	return ret;
}

/*
 * is_dev_dax_fd -- (internal) detects if fd is associated with
 *                   emulated Device DAX
 *
 * Returns device index for Device DAX or -1 otherwise.
 */
static int
is_dev_dax_fd(int fd)
{
	LOG(3, "fd %d", fd);
	int ret = -1;

	for (int i = 0; i < Ndev; i++)
		for (int j = 0; j < MAX_FD; j++)
			if (fd == Devices[i].fd[j]) {
				ret = i;
				break;
			}

	LOG(4, "returning %d", ret);
	return ret;
}

/*
 * is_dev_dax_addr -- (internal) detects if addr points to
 *                    memory region mapped from emulated Device DAX
 *
 * Returns 1 for Device DAX or -1 otherwise.
 */
static int
is_dev_dax_addr(void *addr)
{
	return util_range_is_pmem(addr, 1) ? 1 : -1;
}

/*
 * register_fd -- (internal) associate fd with emulated Device DAX
 */
static void
register_fd(int idx, int fd)
{
	for (int i = 0; i < MAX_FD; i++)
		if (Devices[idx].fd[i] == -1) {
			Devices[idx].fd[i] = fd;
			return;
		}

	ASSERT(0);
}

/*
 * unregister_fd -- (internal) disassociate fd with emulated Device DAX
 */
static void
unregister_fd(int idx, int fd)
{
	for (int i = 0; i < MAX_FD; i++)
		if (Devices[idx].fd[i] == fd) {
			Devices[idx].fd[i] = -1;
			return;
		}

	ASSERT(0);
}

/*
 * open_sys -- (internal) emulates opening a /sys/dev/... entry
 *             associated with emulated Device DAX
 */
static int
open_sys(const char *path, int idx)
{
	LOG(3, "path %s idx %d", path, idx);

	int fd = util_tmpfile("/tmp", "/daxemu.XXXXXXXX");
	if (fd < 0)
		FATAL("!mktemp");

	int fd2 = dup(fd);
	if (fd2 < 0) {
		ERR("!dup");
		return -1;
	}

	/* associate a stream with the file descriptor */
	FILE *fs = os_fdopen(fd2, "w");
	if (fs == NULL) {
		ERR("!fdopen %d", fd2);
		(Close_func)(fd2);
		return -1; /* FATAL */
	}

	if (strstr(path, "/device/align") != NULL) {
		LOG(3, "alignment %zu", Devices[idx].alignment);
		fprintf(fs, "%zu\n", Devices[idx].alignment);
		fclose(fs);
	}
	if (strstr(path, "/size") != NULL) {
		LOG(3, "size %zu", Devices[idx].length);
		fprintf(fs, "%zu\n", Devices[idx].length);
		fclose(fs);
	}
	if (strstr(path, "/subsystem") != NULL) {
		LOG(3, "subsystem %s", DEVICE_DAX_PREFIX);
		fprintf(fs, "%s\n", DEVICE_DAX_PREFIX);
		fclose(fs);
	}

	os_lseek(fd, 0, SEEK_SET);

	return fd;
}

/*
 * open -- XXX
 */
int
open(const char *path, int flags, ...)
{
	va_list ap;
	va_start(ap, flags);
	mode_t mode = va_arg(ap, mode_t);
	va_end(ap);

	LOG(3, "path %s flags %x mode %o", path, flags, mode);

	int idx = is_dev_dax_sys_path(path);
	if (idx != -1)
		return open_sys(path, idx);

	int fd;
	idx = is_dev_dax_path(path);
	if (idx != -1) {
		LOG(4, "open: fake path %s flags %x mode %o",
				Devices[idx].fake_path, flags, 0644);
		fd = (Open_func)(Devices[idx].fake_path,
				O_CREAT|O_RDWR, 0644); /* XXX */
		if (fd < 0) {
			ERR("!open: %s", Devices[idx].fake_path);
			return -1;
		}
		LOG(4, "posix_fallocate: fd %d off %ju len %zu",
				fd, (off_t)0, Devices[idx].length);
		int err = (Posix_fallocate_func)(fd, 0,
				(off_t)Devices[idx].length);
		if (err != 0) {
			errno = err;
			ERR("!posix_fallocate: %s", Devices[idx].fake_path);
			(void) (Close_func)(fd);
			return -1;
		}
		(void) (Close_func)(fd);

		flags &= ~(O_EXCL); /* XXX */
		LOG(4, "open: fake path %s flags %x mode %o",
				Devices[idx].fake_path, flags, mode);
		fd = (Open_func)(Devices[idx].fake_path, flags, mode);
		register_fd(idx, fd);

		LOG(4, "open: returning fd %d", fd);
		return fd;
	}

	LOG(4, "Open: path %s flags %x mode %o", path, flags, mode);
	fd = (Open_func)(path, flags, mode);
	LOG(4, "Open: returning fd %d", fd);
	return fd;
}

/*
 * __open_2 -- XXX
 */
int
__open_2(const char *path, int flags)
{
	return open(path, flags, 0);
}

/*
 * close -- XXX
 */
int
close(int fd)
{
	LOG(3, "fd %d", fd);

	int ret = (Close_func)(fd);

	int idx = is_dev_dax_fd(fd);
	if (idx != -1)
		unregister_fd(idx, fd);

	return ret;
}

/*
 * mmap -- XXX
 */
void *
mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	LOG(3, "addr %p len %zu prot %x flags %x fd %d offset %ju",
		addr, length, prot, flags, fd, offset);

	void *ret;
	int idx = -1;

	if (fd == -1)
		goto map;

	idx = is_dev_dax_fd(fd);
	if (idx == -1)
		goto map;

	/* mapping length must be aligned to internal page size */
	size_t len_aligned = roundup(length, Pagesize);
	if (len_aligned % Devices[idx].alignment != 0) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	/* MAP_PRIVATE is not supported */
	if (flags & MAP_PRIVATE) {
		errno = EINVAL;
		return MAP_FAILED;
	}

map:
	ret = (Mmap_func)(addr, length, prot, flags, fd, offset);
	if (idx != -1 && ret != MAP_FAILED && ret != NULL) {
		util_range_unregister(ret, length); /* XXX */
		util_range_register(ret, length);
	}

	return ret;
}

/*
 * munmap -- XXX
 */
int
munmap(void *addr, size_t length)
{
	LOG(3, "addr %p len %zu", addr, length);

	int idx = is_dev_dax_addr(addr);

	int ret = (Munmap_func)(addr, length);

	if (idx != -1 && length != 0)
		util_range_unregister(addr, length);

	return ret;
}

/*
 * msync -- XXX
 */
int
msync(void *addr, size_t length, int flags)
{
	LOG(3, "addr %p len %zu flags %x", addr, length, flags);

	/* XXX: length */
	int idx = is_dev_dax_addr(addr);
	if (idx != -1 && length > 0) {
		errno = EINVAL;
		return -1;
	}

	return (Msync_func)(addr, length, flags);
}

/*
 * stat -- XXX
 */
int
__xstat(int ver, const char *path, struct stat *st)
{
	LOG(3, "ver %d path %s st %p", ver, path, st);

	int idx = is_dev_dax_path(path);
	if (idx == -1)
		return (Xstat_func)(ver, path, st);

	(Xstat_func)(_STAT_VER, Devices[idx].fake_path, st);
	st->st_mode = S_IRUSR|S_IWUSR;
	st->st_mode |= S_IFCHR;
	st->st_rdev = makedev(Devices[idx].major, Devices[idx].minor);

	return 0;
}

/*
 * fstat -- XXX
 */
int
__fxstat(int ver, int fd, struct stat *st)
{
	LOG(3, "ver %d fd %d st %p", ver, fd, st);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1)
		return (Fxstat_func)(ver, fd, st);

	(Fxstat_func)(_STAT_VER, fd, st);
	st->st_mode = S_IRUSR|S_IWUSR;
	st->st_mode |= S_IFCHR;
	st->st_rdev = makedev(Devices[idx].major, Devices[idx].minor);

	return 0;
}

/*
 * realpath -- XXX
 */
char *
realpath(const char *path, char *rpath)
{
	LOG(3, "path %s rpath %p", path, rpath);

	char *rp;
	char *ret;

	if (strncmp(path, DEVICE_CHAR_PREFIX,
			strlen(DEVICE_CHAR_PREFIX)) == 0) {
		rp = DEVICE_DAX_PREFIX;
		goto out;
	}

	int idx = is_dev_dax_path(path);
	if (idx != -1) {
		rp = (char *)path;
		goto out;
	}

	return (Realpath_func)(path, rpath);

out:
	if (rpath != NULL)
		ret = strcpy(rpath, rp);
	else
		ret = strdup(rp);

	LOG(3, "rpath %s", ret);
	return ret;
}

/*
 * __realpath_chk -- XXX
 */
char *
__realpath_chk(const char *path, char *rpath, size_t rpathlen)
{
	return realpath(path, rpath); /* XXX */
}

/*
 * access -- XXX
 */
int
access(const char *path, int mode)
{
	LOG(3, "path %s mode %o", path, mode);

	int idx = is_dev_dax_path(path);
	if (idx == -1)
		return (Access_func)(path, mode);

	if (mode == F_OK)
		return 0;	/* XXX */
	if (mode & X_OK)
		return -1;
	if (mode & (R_OK | W_OK))
		return 0;

	return 0;
}

/*
 * read -- XXX
 */
ssize_t
read(int fd, void *buf, size_t count)
{
	LOG(3, "fd %d buf %p count %zu", fd, buf, count);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1) {
		return (Read_func)(fd, buf, count);
	} else {
		errno = EINVAL;
		return -1;
	}
}

/*
 * write -- XXX
 */
ssize_t
write(int fd, const void *buf, size_t count)
{
	LOG(3, "fd %d buf %p count %zu", fd, buf, count);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1) {
		return (Write_func)(fd, buf, count);
	} else {
		errno = EINVAL;
		return -1;
	}
}

/*
 * pread -- XXX
 */
ssize_t
pread(int fd, void *buf, size_t count, off_t offset)
{
	LOG(3, "fd %d buf %p count %zu offset %ju", fd, buf, count, offset);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1) {
		return (Pread_func)(fd, buf, count, offset);
	} else {
		errno = EINVAL;
		return -1;
	}
}

/*
 * pwrite -- XXX
 */
ssize_t
pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	LOG(3, "fd %d buf %p count %zu offset %ju", fd, buf, count, offset);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1) {
		return (Pwrite_func)(fd, buf, count, offset);
	} else {
		errno = EINVAL;
		return -1;
	}
}

/*
 * lseek -- XXX
 */
off_t
lseek(int fd, off_t offset, int whence)
{
	LOG(3, "fd %d offset %ju whence %d", fd, offset, whence);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1) {
		return (Lseek_func)(fd, offset, whence);
	} else {
		return 0;
	}
}

/*
 * fsync -- XXX
 */
int
fsync(int fd)
{
	LOG(3, "fd %d", fd);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1) {
		return (Fsync_func)(fd);
	} else {
		errno = EINVAL;
		return -1;
	}
}

/*
 * ftruncate -- XXX
 */
int
ftruncate(int fd, off_t length)
{
	LOG(3, "fd %d length %ju", fd, length);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1) {
		return (Ftruncate_func)(fd, length);
	} else {
		errno = EINVAL;
		return -1;
	}
}

/*
 * posix_fallocate -- XXX
 */
int
posix_fallocate(int fd, off_t offset, off_t len)
{
	LOG(3, "fd %d offset %ju len %ju", fd, offset, len);

	int idx = is_dev_dax_fd(fd);
	if (idx == -1) {
		return (Posix_fallocate_func)(fd, offset, len);
	} else {
		return ENODEV;
	}
}

/*
 * libdaxemu_load -- loads devices configuration
 */
static void
libdaxemu_load(void)
{
	LOG(3, NULL);

	char line[MAX_LINE];
	char *s;
	char *cp;
	struct devdax d;

	char *path = os_getenv(DAXEMU_CFG_FILE_VAR);
	if (path == NULL)
		FATAL("no config file specified");

	FILE *fs = os_fopen(path, "r");
	if (fs == NULL)
		FATAL("!fopen \"%s\"", path);

	while ((s = fgets(line, MAX_LINE, fs)) != NULL) {
		if (s) {
			/* chop off newline and comments */
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			if (cp != s && (cp = strchr(line, '#')) != NULL)
				*cp = '\0';

			/* skip comments and blank lines */
			if (cp == s)
				continue;
		}

		unsigned x, y, z;
		if (sscanf(line, "version: %u.%u.%u", &x, &y, &z) == 3) {
			Version = 10000 * x + 100 * y + z;
			LOG(4, "version: %u", Version);
		}

		if (sscanf(line, "device: %s %s %zu %zu %u:%u",
				d.path, d.fake_path,
				&d.length, &d.alignment,
				&d.major, &d.minor) == 6) {
			LOG(4, "device: %s %s %zu %zu %u:%u",
					d.path, d.fake_path,
					d.length, d.alignment,
					d.major, d.minor);
			sprintf(d.sys_path, DAXEMU_SYS_PATH, d.major, d.minor);
			for (int i = 0; i < 10; i++)
				d.fd[i] = -1;
			Devices = realloc(Devices,
					(size_t)(Ndev + 1) * sizeof(d));
			if (Devices == NULL)
				FATAL("!realloc");
			Devices[Ndev++] = d;

		}
	}
}

/*
 * libdaxemu_init -- load-time initialization for libdaxemu
 *
 * Called automatically by the run-time loader.
 */
__attribute__((constructor))
static void
libdaxemu_init(void)
{
	util_init();
	out_init(DAXEMU_LOG_PREFIX, DAXEMU_LOG_LEVEL_VAR,
			DAXEMU_LOG_FILE_VAR, 0, 0);

	Open_func = dlsym(RTLD_NEXT, "open");
	Creat_func = dlsym(RTLD_NEXT, "creat");
	Close_func = dlsym(RTLD_NEXT, "close");
	Mmap_func = dlsym(RTLD_NEXT, "mmap");
	Munmap_func = dlsym(RTLD_NEXT, "munmap");
	Msync_func = dlsym(RTLD_NEXT, "msync");
	Mprotect_func = dlsym(RTLD_NEXT, "mprotect");
	Xstat_func = dlsym(RTLD_NEXT, "__xstat");
	Fxstat_func = dlsym(RTLD_NEXT, "__fxstat");
	Realpath_func = dlsym(RTLD_NEXT, "realpath");
	Access_func = dlsym(RTLD_NEXT, "access");
	Read_func = dlsym(RTLD_NEXT, "read");
	Write_func = dlsym(RTLD_NEXT, "write");
	Pread_func = dlsym(RTLD_NEXT, "pread");
	Pwrite_func = dlsym(RTLD_NEXT, "pwrite");
	Lseek_func = dlsym(RTLD_NEXT, "lseek");
	Fsync_func = dlsym(RTLD_NEXT, "fsync");
	Ftruncate_func = dlsym(RTLD_NEXT, "ftruncate");
	Posix_fallocate_func = dlsym(RTLD_NEXT, "posix_fallocate");

	libdaxemu_load();
}

/*
 * libdaxemu_fini -- libdaxemu cleanup routine
 *
 * Called automatically when the process terminates.
 */
__attribute__((destructor))
static void
libdaxemu_fini(void)
{
	LOG(3, NULL);

	free(Devices);
}
