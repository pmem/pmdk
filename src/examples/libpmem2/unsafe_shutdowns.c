// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * unsafe_shutdowns.c -- unsafe shutdowns example for the libpmem2
 */

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

/* flags for struct shutdown_state */
#define CLEAR_FLAG 0
#define DIRTY_FLAG 1

/*
 * SDS_LEN -- overall struct shutdown_state len
 */
#define SDS_LEN 4096

/*
 * UUID_LEN -- length of the struct shutdown_state without fields: usc,
 * flag, checksum (SDS_LEN - 8 - 1 - 8 = 4079)
 */
#define UUID_LEN 4079

/*
 * SDS_LEN_NO_CS -- length of the struct shutdown_state without checksum
 * field
 */
#define SDS_LEN_NO_CS offsetof(struct shutdown_state, checksum)

/*
 * shutdown_state -- structure holding data about shutdown state
 */
struct shutdown_state {
	uint64_t usc; /* unsafe shutdown count */
	uint8_t uuid[UUID_LEN]; /* UID */
	uint8_t flag;
	uint64_t checksum;
};

/*
 * checksum_compute -- calculate checksum of the structure using
 * Fletcher's 64 algorithm
 */
static void
checksum_compute(void *addr, size_t len, uint64_t *csump)
{
	if (len % 4 != 0)
		abort();

	uint32_t *p32 = addr;
	uint32_t *p32end = (uint32_t *)((char *)addr + len);
	uint32_t lo32 = 0;
	uint32_t hi32 = 0;

	while (p32 < p32end) {
			lo32 = (lo32 + *p32) % 255;
			hi32 = (hi32 + lo32) % 255;
			++p32;
	}

	*csump = (uint64_t)hi32 << 32 | lo32;
}

/*
 * prepare_environment -- prepare environment for create/read operation
 */
static void
prepare_environment(int *fd, char *file, struct pmem2_config *cfg,
		struct pmem2_source **src, struct pmem2_map **map,
		void **mapping_addr, size_t *mapping_size)
{
	if ((*fd = open(file, O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		exit(1);
	}

	if (pmem2_source_from_fd(src, *fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		exit(1);
	}

	if (pmem2_map(cfg, *src, map)) {
		pmem2_perror("pmem2_map");
		exit(1);
	}

	*mapping_addr = pmem2_map_get_address(*map);
	*mapping_size = pmem2_map_get_size(*map);
}

/*
 * cleanup -- cleaning up mapping, config and file descriptor
 */
static void
cleanup(int *fd, struct pmem2_config *cfg, struct pmem2_map *map)
{
	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	close(*fd);
}

/*
 * create_data -- creating mapping and saving data to the mapping
 */
static void
create_data(int *fd, char *file, struct pmem2_config *cfg,
		struct pmem2_source *src, struct pmem2_map *map)
{
	void *mapping_addr = NULL;
	size_t mapping_size;
	pmem2_flush_fn flush;

	prepare_environment(fd, file, cfg, &src, &map,
			&mapping_addr, &mapping_size);
	flush = pmem2_get_flush_fn(map);

	/*
	 * 1. File mapping
	 * FAIL: BAD UUID, BAD USC, BAD checksum, UNDEFINED flag
	 */
	struct shutdown_state *sds = (struct shutdown_state *)mapping_addr;

	/*
	 * 2. Init struct shutdown_state
	 * FAIL: BAD UUID, BAD USC, BAD checksum, UNDEFINED flag
	 */
	memset(sds, 0, sizeof(struct shutdown_state));

	/*
	 * 3. Flush
	 * FAIL: BAD UUID, BAD USC flag == CLEAR_FLAG
	 */
	flush(mapping_addr, mapping_size);

	/*
	 * 4. Add UUID and USC to the struct shutdown_state
	 * FAIL: BAD UUID, BAD USC, BAD checksum
	 */
	/* let's get dimm's UUID and USC */
	size_t len2 = SDS_LEN_NO_CS;
	if (pmem2_source_device_id(src, (char *)&sds->uuid, &len2)) {
		pmem2_perror("pmem2_source_device_id");
		exit(1);
	}
	if (pmem2_source_device_usc(src, &sds->usc)) {
		pmem2_perror("pmem2_source_device_usc");
		exit(1);
	}

	/*
	 * 5. Flush
	 * FAIL: BAD UUID, BAD USC (may be good if last file), BAD checksum
	 */
	flush(mapping_addr, mapping_size);

	/*
	 * 6. Calculate checksum
	 * FAIL: BAD UUID, BAD USC (may be good if last file), BAD checksum
	 */
	checksum_compute(sds, SDS_LEN_NO_CS, &sds->checksum);

	/*
	 * 7. Flush
	 * FAIL: BAD UUID, BAD USC (may be good if last file), GOOD checksum
	 */
	flush(mapping_addr, mapping_size);

	/*
	 * 8. Set flag
	 * FAIL: GOOD UUID, GOOD USC, GOOD || BAD checksum,
	 * FLAG == CLEAR_FLAG || FLAG == DIRTY_FLAG
	 */
	sds->flag = DIRTY_FLAG;

	/*
	 * 9. Flush
	 * FAIL: GOOD UUID, GOOD USC, BAD checksum, FLAG == 1
	 */
	flush(mapping_addr, mapping_size);

	/*
	 * 10. Calculate checksum
	 * FAIL: GOOD UUID, GOOD USC, BAD checksum, FLAG == DIRTY_FLAG
	 */
	checksum_compute(sds, SDS_LEN_NO_CS, &sds->checksum);

	/*
	 * 11. Flush
	 * FAIL: GOOD UUID, GOOD USC, GOOD checksum, FLAG == DIRTY_FLAG
	 */
	flush(mapping_addr, mapping_size);

	/*
	 * 12. Save something else to the pool and flush
	 * FAIL: GOOD UUID, GOOD USC, GOOD checksum, FLAG == 1
	 */
	char *string_to_save = "hello, persistent memory";
	strcpy((char *)mapping_addr + SDS_LEN, string_to_save);
	flush(mapping_addr, mapping_size);

	/*
	 * 13. Clear flag
	 * FAIL: GOOD UUID, GOOD USC, GOOD || BAD checksum,
	 * FLAG == 1 || FLAG == 0
	 */
	sds->flag = CLEAR_FLAG;

	/*
	 * 14. Flush
	 * FAIL: GOOD UUID, GOOD USC, BAD checksum, FLAG == 0
	 */
	flush(mapping_addr, mapping_size);

	/*
	 * 15. Calculate checksum
	 * FAIL: GOOD UUID, GOOD USC, BAD checksum, FLAG == CLEAR_FLAG
	 */
	checksum_compute(sds, SDS_LEN_NO_CS, &sds->checksum);

	/*
	 * 16 Flush
	 * FAIL: GOOD UUID, GOOD USC, GOOD checksum, FLAG == 0
	 */
	flush(mapping_addr, mapping_size);

	/* 17. File unmapping */
	cleanup(fd, cfg, map);
}

/*
 * read_data -- reading mapping and checking/fixing integrity of the data
 */
static void
read_data(int *fd, char *file, struct pmem2_config *cfg,
		struct pmem2_source *src, struct pmem2_map *map)
{
	void *mapping_addr = NULL;
	size_t mapping_size;
	pmem2_flush_fn flush;

	prepare_environment(fd, file, cfg, &src, &map,
			&mapping_addr, &mapping_size);
	struct shutdown_state *sds = (struct shutdown_state *)mapping_addr;
	flush = pmem2_get_flush_fn(map);

	/* stack variables used to compare data with the *sds */
	uint64_t usc; /* unsafe shutdown count */
	uint8_t uuid[UUID_LEN]; /* UID */
	uint64_t checksum;

	/* let's get dimm's UUID, USC and get *sds checksum */
	size_t len = SDS_LEN_NO_CS;
	if (pmem2_source_device_id(src, (char *)uuid, &len)) {
		pmem2_perror("pmem2_source_device_id");
		exit(1);
	}
	if (pmem2_source_device_usc(src, &usc)) {
		pmem2_perror("pmem2_source_device_usc");
		exit(1);
	}
	checksum_compute(sds, SDS_LEN_NO_CS, &checksum);

	/* UUID && USC == GOOD */
	if (sds->uuid == uuid && sds->usc == usc) {
		/* FLAG == CLEAR_FLAG */
		if (sds->flag == CLEAR_FLAG) {
			/*
			 * checksum == correct, POOL is OK
			 * Points: 8, 16
			 */
			if (sds->checksum == checksum) {
				goto cleanup;
			}
			/*
			 * checksum != correct, fix the checksum and POOL is OK
			 * Points: 13, 14, 15
			 */
			else {
				checksum_compute(sds, SDS_LEN_NO_CS,
						&sds->checksum);
				flush(mapping_addr, mapping_size);
				goto cleanup;
			}
		}
		/* FLAG != CLEAR_FLAG */
		else {
			/*
			 * checksum == correct, POOL is OK, (pool was no closed
			 * but no ADR failure)
			 * Points: 11, 12, 13
			 */
			if (sds->checksum == checksum) {
				goto cleanup;
			}
			/*
			 * checksum != correct, fix the checksum and POOL is OK
			 * Points: 8, 9, 10
			 */
			else {
				checksum_compute(sds, SDS_LEN_NO_CS,
						&sds->checksum);
				flush(mapping_addr, mapping_size);
				goto cleanup;
			}
		}
	}
	/* UUID && USC != GOOD */
	else {
		/* FLAG == CLEAR_FLAG */
		if (sds->flag == CLEAR_FLAG) {
			/*
			 * checksum == correct, fix UUID && USC and POOL is OK
			 * (ADR failure but pool was not open)
			 * Points: 3
			 */
			if (sds->checksum == checksum) {
				sds->usc = usc;
				strcpy((char *)sds->uuid, (char *)uuid);
				flush(mapping_addr, mapping_size);
				goto cleanup;
			}
			/*
			 * checksum != correct, reinit all and POOL is OK
			 * Points: 4, 5, 6, 7
			 */
			else {
				memset(sds, 0, sizeof(struct shutdown_state));
				flush(mapping_addr, mapping_size);
				goto cleanup;
			}
		}
		/* FLAG != CLEAR_FLAG */
		else {
			/* checksum == correct, POOL is BROKEN */
			if (sds->checksum == checksum) {
				goto cleanup;
			}
			/*
			 * checksum != correct, reinit all and POOL is OK
			 * Points: 1, 2
			 */
			else {
				memset(sds, 0, sizeof(struct shutdown_state));
				flush(mapping_addr, mapping_size);
				goto cleanup;
			}
		}
	}
	/* File unmapping */
	cleanup(fd, cfg, map);

cleanup:
	cleanup(fd, cfg, map);
}

int
main(int argc, char *argv[])
{
	int fd;
	struct pmem2_config *cfg = NULL;
	struct pmem2_source *src = NULL;
	struct pmem2_map *map = NULL;

	if (argc != 3 && (strcmp(argv[1],
			"create") || strcmp(argv[1], "read"))) {
		fprintf(stderr, "usage: %s [create|read] file\n",
				argv[0]);
		exit(1);
	}

	char *mode = argv[1];
	char *file = argv[2];

	if (strcmp(mode, "create") == 0)
		create_data(&fd, file, cfg, src, map);
	if (strcmp(mode, "read") == 0)
		read_data(&fd, file, cfg, src, map);

	free(src);
	return 0;
}
