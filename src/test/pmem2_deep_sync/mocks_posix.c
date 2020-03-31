// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * mocks_posix.c -- mocked functions used in deep_sync.c (Posix implementation)
 */

#include "map.h"
#include "os.h"
#include "pmem2_deep_sync.h"
#include "pmem2_utils.h"
#include "unittest.h"
#include "util.h"

#define BUS_DEVICE_PATH "/sys/bus/nd/devices"
#define DEV_DEVICE_PATH "/sys/dev/char/234:137/device/dax_region"

extern int n_msynces;
extern int n_persists;
extern int is_devdax;

/*
 * open -- open mock
 */
FUNC_MOCK(os_open, int, const char *path, int flags, ...)
FUNC_MOCK_RUN_DEFAULT {
	va_list ap;
	va_start(ap, flags);
	int mode = va_arg(ap, int);
	va_end(ap);

	char *is_bus_device_path = strstr(path, BUS_DEVICE_PATH);
	char *is_dev_device_path = strstr(path, DEV_DEVICE_PATH);

	if (is_bus_device_path || is_dev_device_path) {
		char mocked_path[PATH_MAX] = {0};
		getcwd(mocked_path, PATH_MAX);
		/*
		 * Windows doesn't allow use ':' character, that's why it is
		 * replaced.
		 */
		char *invalid_char = strchr(path, ':');
		if (invalid_char)
			*invalid_char = '_';

		strcat(mocked_path, path);

		return _FUNC_REAL(os_open)(mocked_path, flags, mode);
	}

	return _FUNC_REAL(os_open)(path, flags, mode);
}
FUNC_MOCK_END

/*
 * pmem2_get_type_from_stat -- pmem2_get_type_from_stat mock
 */
FUNC_MOCK(pmem2_get_type_from_stat, int, const os_stat_t *st,
		enum pmem2_file_type *type)
FUNC_MOCK_RUN_DEFAULT {
	if (is_devdax)
		*type = PMEM2_FTYPE_DEVDAX;
	else
		*type = PMEM2_FTYPE_REG;

	return 0;
}
FUNC_MOCK_END

/*
 * write -- msync mock
 */
FUNC_MOCK(msync, int, void *addr, size_t len, int flags)
FUNC_MOCK_RUN_DEFAULT {
	n_msynces++;
	return 0;
}
FUNC_MOCK_END

/*
 * pmem2_set_flush_fns -- pmem2_set_flush_fns mock
 */
FUNC_MOCK(pmem2_set_flush_fns, void, struct pmem2_map *map)
FUNC_MOCK_RUN_DEFAULT {
	map->persist_fn = pmem2_persist_mock;
}
FUNC_MOCK_END
