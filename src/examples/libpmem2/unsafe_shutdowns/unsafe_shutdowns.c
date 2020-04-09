// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * unsafe_shutdowns.c -- unsafe shutdowns example for the libpmem2
 */

/*
 * The memory pool contains a few things:
 * - a pool state which includes:
 * -- a backing device state (ID and USC value)
 * -- file-in-use indicator
 * - usable data (array of characters)
 *
 * The pool state allows judging whether the usable data is not corrupted.
 * The pool may exists in a few possible states:
 * A. Zero-initialized (at startup only)
 * - assuming zeroed-out device ID is incorrect it invalidates USC value
 * - file-in-use == 0 indicates file is closed
 * B. Zero-initialized but with correct USC value (at startup only)
 * - zeroed-out device ID still invalidates USC value (no matter it is correct)
 * - the file is still marked as closed
 * - it is the required state between A and C
 * - may be reached during fixing (B')
 * -- in this case, the USC value requires update despite being non-zero
 * C. The device ID is matching and (USC-new == USC-old) and file-in-use == 0
 *    (at startup only)
 * - from this state it is possible to detect:
 * -- unsafe shutdowns if USC value will change (device ID validates stored USC)
 * -- moving the file to another backing device (device ID mismatch)
 * - note the file is still marked as closed so even unsafe shutdown cannot
 *   corrupt the usable data
 * D. The device ID is matching and (USC-new == USC-old) but file-in-use == 1
 *    (FILE_ARMED)
 * - at runtime:
 * -- this is the only state in which file contents may be modified
 * -- this is the only state in which unsafe shutdown may corrupt the usable
 *    data
 * -- before closing the file the pool should transition to state C
 * - at startup:
 * -- this state indicates D -> C transition was interrupted;
 * -- depending on the persistent structure resilience it may indicate the
 *    persistent structure requires recovery
 * E. Device ID mismatch (at startup only)
 * - E0. file-in-use == 0 (FILE_UNARMED)
 * -- this indicates the file was moved to another backing device
 * -- since the file was closed cleanly before moving the usable data is not
 *    corrupted
 * - E1. file-in-use == 1 (FILE_ARMED)
 * -- this indicates the file was moved to another backing device
 * -- since the file was NOT closed cleanly before moving the usable data may
 *    be corrupted
 * F. The device ID is matching but (USC-new != USC-old)
 * - this indicates the unsafe shutdown occurred
 * - F0. file-in-use == 0 (FILE_UNARMED)
 * -- since the file was closed cleanly / or not yet armed for writing before
 *    the unsafe shutdown occurred the usable data is not corrupted
 * - F1. file-in-use == 1 (FILE_ARMED)
 * -- since the file was armed for writing when the unsafe shutdown occurred
 *    the usable data MAY be corrupted
 *
 * This application distinguishes between the states of the pool which allows
 * detecting the possibility of usable data corruption. The only false-positive
 * possible is when despite the usable data was in the power-fail-safe domain
 * (not on the persistent medium yet) while an unsafe-shutdown happen it
 * miraculously reach the persistent medium. Such miracles are indetectable.
 *
 * Note: You can further strengthen usable-data-corruption-detection by building
 * and handling persistent structures in such a way so, in the face of data
 * corruption, it will be possible to recover the consistent state of the
 * structure from the point in time before the failed modifications have
 * started. XXX missing reference.
 *
 * Distinguishing between the pool states requires:
 * - deep syncing changes required to transition between states
 * - intermediate states are impossible in case of the unsafe shutdown
 * -- in case of variables <= 8 bytes (usc, file_in_use) it is guaranteed by the
 *    hardware
 * -- in case of device id (which is > 8 bytes) all bytes are required to have
 *    full device ID match
 *
 * States A and B occur during the pool initialization. After A -> B -> C
 * transition the pool is ready for writing the usable data. State C is also
 * a normal state during every startup.
 * Before writing the usable data the pool transitions C -> D. In state D the
 * usable data is written. When the file will be closed the pool transitions
 * back D -> C. Which allows at the next startup detect if the file was closed
 * cleanly.
 * If, at startup, the pool is in state other than C it indicates the abnormal
 * situation happened:
 * - state A or B indicates the interruption happened during the initialization
 *   so usable data is not corrupted (since it was not yet written)
 * - state D means the pool was unexpectedly closed (it was not because of the
 *   unsafe shutdown). Depending on the persistent structure resilience this
 *   error may be recoverable.
 * - state E indicates the file was moved. E0 means the usable data is not
 *   corrupted whereas E1 is similar to D.
 * - state F indicates the unsafe shutdown happened. F0 means the usable data is
 *   not corrupted whereas F1 means the usable data may be corrupted. A
 *   resilient persistent structure is not enough to survive the unsafe shutdown
 *   since it can not rely on normally guaranteed-to-work persistency
 *   primitives.
 *
 * Fixing the pool at startup:
 * 0. fixing the usable data
 * - if the pool state indicates the usable data is corrupted, the usable data
 *   should be removed to prevent using it after fixing the pool state
 * - if the pool state indicates the usable data may require the recovery,
 *   the recovery should be done after fixing the pool state (state D required)
 * 1. disarming the file [D, E1, F1] -> [C, E0, F0], when this operation
 *    succeed:
 * - state C is normal so the pool is ready to use
 * - state E0 and F0 requires updating device ID and/or USC value
 * 2. invalidating the USC value by zeroing device ID [E0, F0] -> [B', B']
 * 3. updating the USC value B' -> B
 * 4. writing the correct device ID B -> C
 */

#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libpmem2.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#define FLAGS_ZERO 0 /* flush + drain */

#define DEVICE_ID_LEN 32UL

#define FILE_UNARMED 0 /* file is closed or used only for reading */
#define FILE_ARMED 1 /* file is ready for writing */

struct pool_state {
	struct device_state {
		char id[DEVICE_ID_LEN];
		uint64_t usc; /* unsafe shutdown counter value */
	} device_state;

	uint8_t file_in_use;
};

struct pool_content {
	struct pool_state ps;
	char usable_data[];
};

struct pool_data {
	struct pool_content *content;

	/* source and mapping objects */
	struct pmem2_source *src;
	struct pmem2_map *map;

	size_t map_size;
	size_t usable_space_size;

	/* mapping-specific functions */
	pmem2_persist_fn persist;
	pmem2_memset_fn memset;
	pmem2_memcpy_fn memcpy;
};

/*
 * device_state_read -- read device UUID and USC
 */
static int
device_state_read(struct pmem2_source *src, struct device_state *ds)
{
	/* obtain device unsafe shutdown counter value (USC) */
	int ret = pmem2_source_device_usc(src, &ds->usc);
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
		fprintf(stderr, "the device ID is too long (%zu > %zu)\n",
			len, DEVICE_ID_LEN);
		return 1;
	}

	ret = pmem2_source_device_id(src, (char *)&ds->id, &len);
	if (ret) {
		pmem2_perror("pmem2_source_device_id failed reading device ID");
		return ret;
	}

	return 0;
}

/*
 * device_state_init -- initialize device state in a unsafe-shutdown-safe way
 */
static int
device_state_init(struct pool_data *pool)
{
	struct pool_state *ps = &pool->content->ps;
	struct device_state *ds = &ps->device_state;

	/* validate initial pool state */
	const struct pool_state ps_zeroed = {0};
	if (memcmp(&pool->content->ps, &ps_zeroed, sizeof(ps_zeroed)) != 0) {
		fprintf(stderr, "the file is not zero-initialized\n");
		return 1;
	}

	/* read current device state */
	struct device_state ds_curr = {0};
	int ret = device_state_read(pool->src, &ds_curr);

	/*
	 * write, persist and deep sync the USC value. It has to be stored on
	 * the persistent medium before it will be validated by writing device
	 * ID.
	 */
	ds->usc = ds_curr.usc;
	pool->persist(&ds->usc, sizeof(ds->usc));
	ret = pmem2_deep_sync(pool->map, &ds->usc, sizeof(ds->usc));
	if (ret) {
		pmem2_perror("pmem2_deep_sync USC deep sync failed");
		return ret;
	}

	/* valid device ID validates already stored USC value */
	pool->memcpy(ds->id, ds_curr.id, DEVICE_ID_LEN, FLAGS_ZERO);
	ret = pmem2_deep_sync(pool->map, ds->id, DEVICE_ID_LEN);
	if (ret) {
		pmem2_perror("pmem2_deep_sync device ID failed");
		return ret;
	}

	return 0;
}

/*
 * device_state_reinit -- reinitialize device state
 * This is required in a few cases:
 * - the primary initialization was interrupted leaving the pool half-backed
 * - the pool was moved which invalidates all collected device-specific data
 */
static int
device_state_reinit(struct pool_data *pool)
{
	struct pool_state *ps = &pool->content->ps;
	struct device_state *ds = &ps->device_state;
	int ret = 0;

	/*
	 * file has to be unarmed before reinitializing its device state.
	 * Otherwise, if the process of reinitializing will be interrupted the
	 * state of the pool will be indistinguishable from the state of the
	 * pool after closing not cleanly and moving it to another backing
	 * device.
	 */
	assert(ps->file_in_use == FILE_UNARMED);

	/* invalidate USC value by overwriting device ID */
	pool->memset(ds->id, 0, DEVICE_ID_LEN, FLAGS_ZERO);
	ret = pmem2_deep_sync(pool->map, ds->id, DEVICE_ID_LEN);
	if (ret) {
		pmem2_perror("pmem2_deep_sync invalid device ID failed");
		return ret;
	}

	/* reset the remaining */
	pool->memset(&ds->usc, 0, sizeof(ds->usc), FLAGS_ZERO);
	/*
	 * no persist nor deep sync required since USC will be the first value
	 * to be modified and no matter if unsafe shutdown happens the USC
	 * value is invalidated by invalid device ID which is already deep
	 * synced.
	 */

	return device_state_init(pool);
}

/*
 * pool_state_verify -- consider pool state (including the backing device state
 * and whether the file was closed cleanly) to decide if the usable data is
 * valid
 */
static int
pool_state_verify(struct pool_data *pool)
{
	struct pool_state *ps = &pool->content->ps;
	struct device_state *ds_old = &ps->device_state;

	/* read a current device state */
	struct device_state ds_curr = {0};
	int ret = device_state_read(pool->src, &ds_curr);
	if (ret) {
		fprintf(stderr, "Cannot validate device state.\n");
		return ret;
	}

	/* all required checks */
	int is_id_the_same =
		(strncmp(ds_curr.id, ds_old->id, DEVICE_ID_LEN) == 0);
	int is_usc_the_same = (ds_curr.usc == ds_old->usc);
	int is_file_in_use = (ps->file_in_use == FILE_ARMED);

	if (is_id_the_same) {
		if (is_usc_the_same) {
			/* the unsafe shutdown has NOT occurred... */

			if (is_file_in_use) {
				/*
				 * ... but the file was NOT closed cleanly.
				 * Because used data structure (simple character
				 * sequence) does not have built-in correctness
				 * check it may be corrupted.
				 */
				fprintf(stderr,
					"File closed not cleanly. The string may be broken.\n");
				return 1;
			} else {
				/*
				 * ... and the file WAS closed cleanly.
				 * Data is safe.
				 */
				return 0;
			}
		} else {
			/* the unsafe shutdown HAS occurred... */

			if (is_file_in_use) {
				/* ... and the file was in use. */
				fprintf(stderr,
					"Unsafe shutdown detected. The usable data might be corrupted.\n");
				return 1;
			} else {
				/* ... but the file was not in use. */
				fprintf(stderr,
					"Unsafe shutdown detected but the usable data is safe.\n");

				/* only the device state reinit is required */
				return device_state_reinit(pool);
			}
		}
	} else {
		/*
		 * Device ID mismatch indicates two possibilities:
		 * - either the file was moved (in this case it is still
		 * important if the file was closed cleanly since the data may
		 * be corrupted e.g. by application crash) or
		 * - the shutdown / crash happened in the middle of
		 * device_state_init
		 * (in this case file_in_use == FILE_UNARMED and no usable data
		 * was modified so it is not corrupted)
		 */

		if (is_file_in_use) {
			/* The file was moved after not clean close. */
			fprintf(stderr,
				"The file was not closed cleanly and the file was moved. "
				"The usable data might be corrupted.\n");
			return 1;
		} else {
			/*
			 * The file was closed cleanly OR pool_device_state_init
			 * was interrupted. Only the device state reinit is
			 * required.
			 */
			return device_state_reinit(pool);
		}
	}
}

/*
 * pool_arm -- indicate pool is in use before modifying its content
 */
static int
pool_arm(struct pool_data *pool)
{
	uint8_t *file_in_use = &pool->content->ps.file_in_use;

	assert(*file_in_use == FILE_UNARMED);

	*file_in_use = FILE_ARMED;
	pool->persist(file_in_use, sizeof(*file_in_use));
	int ret = pmem2_deep_sync(pool->map, file_in_use, sizeof(*file_in_use));
	if (ret) {
		pmem2_perror("pmem2_deep_sync file in use failed");
		return ret;
	}

	return 0;
}

/*
 * pool_disarm -- indicate pool modifications are completed
 */
static int
pool_disarm(struct pool_data *pool)
{
	uint8_t *file_in_use = &pool->content->ps.file_in_use;

	assert(*file_in_use == FILE_ARMED);

	/* deep sync whole mapping to make sure all persists are completed */
	pmem2_deep_sync(pool->map, pool->content, pool->map_size);

	*file_in_use = FILE_UNARMED;
	pool->persist(file_in_use, sizeof(*file_in_use));
	int ret = pmem2_deep_sync(pool->map, file_in_use, sizeof(*file_in_use));
	if (ret) {
		pmem2_perror("pmem2_deep_sync file in use failed");
		return ret;
	}

	return 0;
}

#define USAGE_STR "usage: %s <command> <file> [<arg>]\n" \
	"Where available commands are:\n" \
	"\tinit - initialize the file metadata\n" \
	"\treset - zero file contents and reinit file metadata\n" \
	"\tread - print file contents\n" \
	"\twrite - store <arg> into the file\n"


int
main(int argc, char *argv[])
{
	struct pmem2_config *cfg = NULL;
	struct pool_data pool = {0};

	/* parse and validate arguments */
	if (argc < 3) {
		fprintf(stderr, USAGE_STR, argv[0]);
		return 1;
	}

	int mode_init = (strcmp(argv[1], "init") == 0);
	int mode_reset = (strcmp(argv[1], "reset") == 0);
	int mode_read = (strcmp(argv[1], "read") == 0);
	int mode_write = (strcmp(argv[1], "write") == 0);
	char *file = argv[2];
	char *str = NULL;

	if (mode_init + mode_reset + mode_read + mode_write == 0) {
		/* either create, read or write mode has to be chosen */
		fprintf(stderr, USAGE_STR, argv[0]);
		return 1;
	}

	if (mode_write) {
		if (argc < 4) {
			fprintf(stderr, USAGE_STR, argv[0]);
			return 1;
		}
		str = argv[3];
	}

	/* open file and prepare source */
	int fd = open(file, O_RDWR);
	if (fd < 0) {
		perror(file);
		return 1;
	}

	int ret = pmem2_source_from_fd(&pool.src, fd);
	if (ret) {
		pmem2_perror("pmem2_source_from_fd");
		goto err_source;
	}

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
		goto err_granularity;
	}

	/* prepare the mapping */
	ret = pmem2_map(cfg, pool.src, &pool.map);
	if (ret) {
		pmem2_perror("pmem2_map");
		goto err_map;
	}

	pool.content = pmem2_map_get_address(pool.map);
	pool.usable_space_size =
		pmem2_map_get_size(pool.map) - sizeof(struct pool_state);
	pool.persist = pmem2_get_persist_fn(pool.map);
	pool.memset = pmem2_get_memset_fn(pool.map);
	pool.memcpy = pmem2_get_memcpy_fn(pool.map);

	if (mode_init) {
		ret = device_state_init(&pool);
		if (ret) {
			/* zero pool state on error */
			struct pool_state *ps = &pool.content->ps;
			pool.memset(ps, sizeof(*ps), 0, FLAGS_ZERO);
		}
	} else if (mode_reset) {
		/* before modifying device state file has to be unarmed */
		uint8_t *file_in_use = &pool.content->ps.file_in_use;
		if (*file_in_use == FILE_ARMED) {
			*file_in_use = FILE_UNARMED;
			pool.persist(file_in_use, sizeof(*file_in_use));
			ret = pmem2_deep_sync(pool.map, file_in_use,
				sizeof(*file_in_use));
			if (ret) {
				pmem2_perror(
					"pmem2_deep_sync file in use failed");
				goto exit;
			}
		}

		/* zero file contents */
		pool.memset(pool.content->usable_data, 0,
			pool.usable_space_size, FLAGS_ZERO);
		ret = pmem2_deep_sync(pool.map, pool.content->usable_data,
			pool.usable_space_size);
		if (ret) {
			pmem2_perror("pmem2_deep_sync file contents");
			goto exit;
		}

		/* reinitialize device state */
		ret = device_state_reinit(&pool);
		if (ret) {
			/* zero pool state on error */
			struct pool_state *ps = &pool.content->ps;
			pool.memset(ps, 0, sizeof(*ps), FLAGS_ZERO);
		}
	} else if (mode_write | mode_read) {
		/* verify if file contents are trustworthy */
		ret = pool_state_verify(&pool);
		if (ret)
			goto exit;

		char *usable_data = pool.content->usable_data;

		if (mode_write) {
			/*
			 * validate if new content size fits into available
			 * space
			 */
			size_t str_size = strlen(str) + 1;
			if (str_size > pool.usable_space_size) {
				fprintf(stderr,
					"New content too long (%zu > %zu)\n",
					str_size,
					pool.usable_space_size);
				ret = 1;
				goto exit;
			}

			/* prepare pool for writing */
			ret = pool_arm(&pool);
			if (ret)
				goto exit;

			/* write new contents */
			pool.memcpy(usable_data, str, str_size, FLAGS_ZERO);
			pool.persist(usable_data, str_size);

			/* mark end of writing */
			ret = pool_disarm(&pool);
			if (ret)
				goto exit;
		} else if (mode_read) {
			/*
			 * reading pool contents does not require any
			 * preparations
			 */
			printf("%s\n", usable_data);
		}
	}

exit:
	pmem2_unmap(&pool.map);
err_map:
err_granularity:
	pmem2_config_delete(&cfg);
err_config:
	pmem2_source_delete(&pool.src);
err_source:
	close(fd);
	return ret;
}
