// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * unsafe_shutdown.c -- unsafe shutdown example for the libpmem2
 *
 * This examples demonstrates how a normal application should consume the
 * deep flush and unsafe shutdown count interfaces to provide a reliable
 * and recoverable access to persistent memory resident data structures.
 */

#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <libpmem2.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#define DEVICE_ID_LEN ((size_t)32ULL)

enum pool_state {
	/*
	 * The pool state cannot be determined because of errors during
	 * retrieval of device information.
	 */
	POOL_STATE_INDETERMINATE,

	/*
	 * The pool is internally consistent and was closed cleanly.
	 * Application can assume that no custom recovery is needed.
	 */
	POOL_STATE_OK,

	/*
	 * The pool is internally consistent, but it was not closed cleanly.
	 * Application must perform consistency checking and custom recovery
	 * on user data.
	 */
	POOL_STATE_OK_BUT_INTERRUPTED,

	/*
	 * The pool can contain invalid data as a result of hardware failure.
	 * Reading the pool is unsafe.
	 */
	POOL_STATE_CORRUPTED,
};

struct device_info {
	char id[DEVICE_ID_LEN];
	uint64_t usc; /* unsafe shutdown count */
};

struct pool_header {
	struct device_info info;
	uint8_t in_use;
};

struct pool_data {
	struct pool_header header;
	char usable_data[];
};

struct pool {
	struct pool_data *data;
	struct pmem2_source *src;
	struct pmem2_map *map;
};

/*
 * pool_new -- creates a new runtime pool instance
 */
static struct pool *
pool_new(int fd)
{
	struct pool *pool = malloc(sizeof(struct pool));
	if (pool == NULL)
		goto err_malloc;

	int ret = pmem2_source_from_fd(&pool->src, fd);
	if (ret) {
		pmem2_perror("pmem2_source_from_fd");
		goto err_source;
	}

	struct pmem2_config *cfg;
	/* prepare configuration */
	ret = pmem2_config_new(&cfg);
	if (ret) {
		pmem2_perror("pmem2_config_new");
		goto err_config;
	}

	ret = pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	if (ret) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		goto err_config_settings;
	}

	/* prepare the mapping */
	ret = pmem2_map(cfg, pool->src, &pool->map);
	if (ret) {
		pmem2_perror("pmem2_map");
		goto err_map;
	}

	pool->data = pmem2_map_get_address(pool->map);

	pmem2_config_delete(&cfg);

	return pool;

err_map:
err_config_settings:
	pmem2_config_delete(&cfg);
err_config:
	pmem2_source_delete(&pool->src);
err_source:
	free(pool);
err_malloc:
	return NULL;
}

/*
 * pool_delete -- deletes a runtime pool instance
 */
static void
pool_delete(struct pool *pool)
{
	pmem2_unmap(&pool->map);
	pmem2_source_delete(&pool->src);
	free(pool);
}

/*
 * map_try_deep_flush -- attempts to perform a deep flush on given range
 */
static int
map_try_deep_flush(struct pmem2_map *map, void *ptr, size_t size)
{
	int ret = pmem2_deep_flush(map, ptr, size);

	/* software should generally not assume that deep flush is supported */
	if (ret == PMEM2_E_NOSUPP)
		ret = 0;

	return ret;
}

/*
 * device_info_read -- populates device_info with data
 */
static int
device_info_read(struct device_info *di, struct pmem2_source *src)
{
	/* obtain device unsafe shutdown counter value (USC) */
	int ret = pmem2_source_device_usc(src, &di->usc);
	if (ret) {
		pmem2_perror("pmem2_source_device_usc");
		return ret;
	}

	/* obtain a device's ID */
	size_t len = 0;
	ret = pmem2_source_device_id(src, NULL, &len);
	if (ret) {
		pmem2_perror(
			"pmem2_source_device_id failed querying device ID length");
		return ret;
	}

	if (len > DEVICE_ID_LEN) {
		fprintf(stderr, "the device ID is too long "
			"(%zu > %zu)\n",
			len, DEVICE_ID_LEN);
		return 1;
	}

	ret = pmem2_source_device_id(src, (char *)&di->id, &len);
	if (ret) {
		pmem2_perror("pmem2_source_device_id failed reading device ID");
		return ret;
	}

	return ret;
}

/*
 * device_info_write -- safely writes new device info into the old location
 */
static int
device_info_write(struct device_info *di_old, const struct device_info *di_new,
	struct pmem2_map *map)
{
	int ret;

	pmem2_memset_fn pmem_memset = pmem2_get_memset_fn(map);
	pmem2_memcpy_fn pmem_memcpy = pmem2_get_memcpy_fn(map);

	/* First, clear any leftover invalid state from the structure */
	pmem_memset(di_old, 0, sizeof(*di_old), 0);
	ret = map_try_deep_flush(map, di_old, sizeof(*di_old));
	if (ret) {
		pmem2_perror("pmem2_deep_flush on device_info memset failed");
		return ret;
	}

	/*
	 * Next, write, persist and deep sync the USC value. It has to be stored
	 * on the persistent medium before it will be validated by writing
	 * device ID.
	 */
	di_old->usc = di_new->usc;
	ret = map_try_deep_flush(map, &di_old->usc, sizeof(di_old->usc));
	if (ret) {
		pmem2_perror("pmem2_deep_flush USC failed");
		return ret;
	}

	/* valid device ID validates already stored USC value */
	pmem_memcpy(di_old->id, di_new->id, DEVICE_ID_LEN, 0);
	ret = map_try_deep_flush(map, di_old->id, DEVICE_ID_LEN);
	if (ret) {
		pmem2_perror("pmem2_deep_flush device ID failed");
		return ret;
	}

	return ret;
}

/*
 * device_info_is_initalized -- checks if the content of device info is
 * initialized.
 *
 * This function returns false if device info was never initialized,
 * initialization was interrupted or the file was moved to a different device.
 * Otherwise, the function returns true.
 */
static bool
device_info_is_initalized(const struct device_info *di_old,
	const struct device_info *di_new)
{
	return strncmp(di_new->id, di_old->id, DEVICE_ID_LEN) == 0;
}

/*
 * device_info_is_consistent -- checks if the device info indicates possible
 * silent data corruption.
 *
 * This function returns false if the unsafe shutdown count of the device
 * was incremented since the last open.
 * Otherwise, the function returns true.
 */
static bool
device_info_is_consistent(const struct device_info *di_old,
	const struct device_info *di_new)
{
	return di_old->usc == di_new->usc;
}

/*
 * pool_state_check_and_maybe_init -- verifies various invariants about the pool
 * and returns its state.
 * If the device state of the pool is initialized if needed.
 */
static enum pool_state
pool_state_check_and_maybe_init(struct pool *pool)
{
	struct device_info di_new;
	enum pool_state state = pool->data->header.in_use ?
		POOL_STATE_OK_BUT_INTERRUPTED : POOL_STATE_OK;

	int ret;
	if ((ret = device_info_read(&di_new, pool->src)) != 0) {
		if (ret == PMEM2_E_NOSUPP)
			return state;
		else
			return POOL_STATE_INDETERMINATE;
	}

	struct device_info *di_old = &pool->data->header.info;

	bool initialized = device_info_is_initalized(di_old, &di_new);

	if (initialized) {
		/*
		 * Pool is corrupted only if it wasn't closed cleanly and
		 * the device info indicates that device info is inconsistent.
		 */
		if (device_info_is_consistent(di_old, &di_new))
			return state;
		else if (state == pool->data->header.in_use)
			return POOL_STATE_CORRUPTED;

		/*
		 * If device info indicates inconsistency, but the pool was
		 * not in use, we just need to reinitialize the device info.
		 */
		initialized = false;
	}

	if (!initialized) {
		if (device_info_write(di_old, &di_new, pool->map) != 0)
			return POOL_STATE_INDETERMINATE;
	}

	return state;
}

/*
 * pool_set_in_use -- marks the pool as either in use or not.
 */
static int
pool_set_in_use(struct pool *pool, bool in_use)
{
	int ret;

	if (!in_use) {
		ret = map_try_deep_flush(pool->map, pool->data,
			pmem2_map_get_size(pool->map));
		if (ret != 0)
			return ret;
	}

	uint8_t *in_usep = &pool->data->header.in_use;
	pmem2_get_memcpy_fn(pool->map)(in_usep,
		&in_use, sizeof(*in_usep), 0);

	ret = map_try_deep_flush(pool->map, in_usep, sizeof(pool));

	return ret;
}

/*
 * pool_access_data -- verifies the pool state and, if possible, accesses
 * the pool for reading and writing
 */
static enum pool_state
pool_access_data(struct pool *pool, void **data, size_t *size)
{
	enum pool_state state = pool_state_check_and_maybe_init(pool);

	if (state == POOL_STATE_OK || state == POOL_STATE_OK_BUT_INTERRUPTED) {
		*data = &pool->data->usable_data;
		*size = pmem2_map_get_size(pool->map) -
			sizeof(struct pool_header);

		if (pool_set_in_use(pool, true) != 0)
			return POOL_STATE_INDETERMINATE;
	}

	return state;
}

/*
 * pool_access_drop -- drops the access for the pool and marks it as not in use
 */
static void
pool_access_drop(struct pool *pool)
{
	if (pool_set_in_use(pool, false) != 0) {
		fprintf(stderr,
			"Failed to drop access to pool which might cause inconsistent state during next open.\n");
	}
}

/*
 * pool_get_memcpy -- retrieves the pool's memcpy function
 */
static pmem2_memcpy_fn
pool_get_memcpy(struct pool *pool)
{
	assert(pool->data->header.in_use);
	return pmem2_get_memcpy_fn(pool->map);
}

/*
 * pool_get_memset -- retrieves the pool's memset function
 */
static pmem2_memset_fn
pool_get_memset(struct pool *pool)
{
	assert(pool->data->header.in_use);
	return pmem2_get_memset_fn(pool->map);
}

/*
 * pool_get_persist -- retrieves the pool's persist function
 */
static pmem2_persist_fn
pool_get_persist(struct pool *pool)
{
	assert(pool->data->header.in_use);
	return pmem2_get_persist_fn(pool->map);
}

#define USAGE_STR "usage: %s <command> <file> [<arg>]\n" \
	"Where available commands are:\n" \
	"\tread - print the file contents\n" \
	"\twrite - store <arg> into the file\n"

/*
 * If the state of the pool is ok, the invariant on this data structure is that
 * the persistent variable is set to 1 only if the string has valid content.
 * If the persistent variable is set to 0, the string should be empty.
 *
 * If the pool state is ok but interrupted, the string variable can contain
 * garbage if persistent variable is set to 0. To restore the previously
 * described invariant, the recovery method needs to zero-out the string if
 * the persistent is 0.
 *
 * If the pool state is corrupted, the previous invariants don't hold, and
 * we cannot assume that the string is valid if persistent variable is set to 1.
 * The only correct course of action is to either reinitialize the data or
 * restore from backup.
 */
struct user_data {
	int persistent; /* flag indicates whether buf contains a valid string */
	char buf[]; /* user string */
};

/*
 * user_data_recovery -- this function restores the invariant that if
 * persistent variable is 0, then the string is empty.
 */
static void
user_data_recovery(struct pool *pool,
	struct user_data *data, size_t usable_size)
{
	size_t max_str_size = usable_size - sizeof(data->persistent);

	if (!data->persistent)
		pool_get_memset(pool)(data->buf, 0, max_str_size, 0);
}

/*
 * user_data_read -- this function prints out the string.
 *
 * Inside of this method, we can be sure that our invariants hold.
 */
static int
user_data_read(struct pool *pool,
	struct user_data *data, size_t usable_size, void *arg)
{
	if (data->persistent)
		printf("%s\n", data->buf);
	else
		printf("empty string\n");

	return 0;
}

/*
 * user_data_write -- persistently writes a string to a variable.
 *
 * Inside of this method, we can be sure that our invariants hold.
 */
static int
user_data_write(struct pool *pool,
	struct user_data *data, size_t usable_size, void *arg)
{
	if (arg == NULL) {
		fprintf(stderr, "expected string input argument\n");
		return 1;
	}
	char *str = arg;

	size_t max_str_size = usable_size - sizeof(data->persistent);

	/*
	 * To make sure our invariants hold, we first write the string and then
	 * set the persistent variable to 1.
	 */
	size_t str_size = strlen(str) + 1;
	if (str_size <= max_str_size) {
		pool_get_memcpy(pool)(data->buf, str, str_size, 0);
		data->persistent = 1;
		pool_get_persist(pool)(&data->persistent,
			sizeof(data->persistent));
	}

	return 0;
}

enum user_data_operation_type {
	USER_OP_READ,
	USER_OP_WRITE,

	MAX_USER_OP,
};

typedef int user_data_operation_fn(struct pool *pool,
	struct user_data *data, size_t usable_size, void *arg);

static struct user_data_operation {
	const char *name;
	user_data_operation_fn *fn;
} user_data_operations[MAX_USER_OP] = {
	[USER_OP_READ] = { .name = "read", .fn = user_data_read },
	[USER_OP_WRITE] = { .name = "write", .fn = user_data_write },
};

/*
 * user_data_operation_parse -- parses the user data operation
 */
static struct user_data_operation *
user_data_operation_parse(char *op)
{
	for (int i = 0; i < MAX_USER_OP; ++i) {
		if (strcmp(user_data_operations[i].name, op) == 0)
			return &user_data_operations[i];
	}
	return NULL;
}

int
main(int argc, char *argv[])
{
	/* parse and validate arguments */
	if (argc < 3) {
		fprintf(stderr, USAGE_STR, argv[0]);
		return 1;
	}

	char *file = argv[2];

	struct user_data_operation *op = user_data_operation_parse(argv[1]);
	if (op == NULL) {
		fprintf(stderr, USAGE_STR, argv[0]);
		return 1;
	}

	/* open file and prepare source */
	int fd = open(file, O_RDWR);
	if (fd < 0) {
		perror(file);
		return 1;
	}

	struct pool *pool = pool_new(fd);
	if (pool == NULL) {
		fprintf(stderr, "unable open a pool from %s", file);
		return 1;
	}

	struct user_data *data;
	size_t size;
	enum pool_state state = pool_access_data(pool, (void **)&data, &size);

	int ret = 1;
	switch (state) {
		case POOL_STATE_INDETERMINATE:
		fprintf(stderr,
			"Unable to determine the state of the pool %s. Accessing the pool might be unsafe.",
			file);
		goto exit;
		case POOL_STATE_CORRUPTED:
		fprintf(stderr,
			"The pool %s might be corrupted, silent data corruption is possible. Accessing the pool is unsafe.",
			file);
		goto exit;
		case POOL_STATE_OK_BUT_INTERRUPTED:
		fprintf(stderr,
			"The pool %s was not closed cleanly. User data recovery is required.",
			file);
		user_data_recovery(pool, data, size);
		break;
		case POOL_STATE_OK:
		break;
	}

	ret = op->fn(pool, data, size, argv[3]);

	pool_access_drop(pool);

exit:
	pool_delete(pool);

	close(fd);

	return ret;
}
