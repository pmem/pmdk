// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * unsafe_shutdowns.c -- unsafe shutdowns example for the libpmem2
 */

/*
 * Overall comment:
 * This example is about to help understand what is happening when
 * unsafe shutdown occurs and how to fix the data.
 * Comments above the instruction inform what operation is happening.
 * Comments with a number below the instruction, inform what is
 * happening after the failure - show status of the sds structure
 * fields (UUID, USC, checksum and flag).
 * Numbers inform about the possible failures which can happen during
 * the unsafe shutdown after the instruction is called.
 * Comments in the sds_vetrify function inform also how to fix the pool
 * after the failure.
 */

#include <sys/stat.h>
#include <fcntl.h>
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
 * checksum_fix -- fix checksum of the shutdown_state structure
 */
static void
checksum_fix(pmem2_persist_fn persist, struct shutdown_state *sds,
		size_t len, uint64_t *csump)
{
	checksum_compute(sds, len, csump);
	persist(sds, sizeof(*sds));
}

/*
 * sds_reinit -- reinit of the shutdown_state struct
 */
static void
sds_reinit(pmem2_memset_fn memset_fn, struct shutdown_state *sds)
{
	memset(sds, 0, sizeof(*sds));

	/* Flush */
	memset_fn(sds, 0, sizeof(*sds), 0);
}

/*
 * uuid_usc_fix -- fix uuid and usc in the shutdown_state struct
 */
static void
uuid_usc_fix(pmem2_persist_fn persist, struct shutdown_state *sds,
		uint8_t *uuid, uint64_t usc)
{
	strcpy((char *)sds->uuid, (char *)uuid);
	sds->usc = usc;
	persist(sds, sizeof(*sds));
}

/*
 * sds_init -- init of the shutdown_state struct
 */
static int
sds_init(pmem2_persist_fn persist, pmem2_memset_fn memset_fn,
		struct pmem2_source *src, struct shutdown_state *sds)
{
	/* Init struct shutdown_state */
	memset(sds, 0, sizeof(*sds));
	/*
	 * 2. FAIL: UNDEFINED UUID, UNDEFINED USC, UNDEFINED checksum,
	 * UNDEFINED flag
	 */

	/* Flush */
	memset_fn(sds, 0, sizeof(*sds), 0);
	/*
	 * 3. FAIL: INCORRECT UUID, INCORRECT USC CORRECT checksum, CLEAR flag
	 */

	/* Add UUID and USC to the struct shutdown_state */
	size_t len = SDS_LEN_NO_CS;
	int ret = pmem2_source_device_id(src, (char *)&sds->uuid, &len);
	if (ret) {
		pmem2_perror("pmem2_source_device_id");
		return ret;
	}

	ret = pmem2_source_device_usc(src, &sds->usc);

	if (ret) {
		if (ret == PMEM2_E_NOSUPP) {
			printf("Getting unsafe shutdown count is not supported"
					" on this system\n");
		} else {
			pmem2_perror("pmem2_source_device_usc");
			return ret;
		}
	}
	/*
	 * 4. FAIL: INCORRECT UUID, INCORRECT USC, INCORRECT checksum,
	 * CLEAR flag
	 */

	/* Flush */
	persist(sds, sizeof(*sds));
	/*
	 * 5. FAIL: CORRECT UUID, CORRECT USC, INCORRECT checksum, CLEAR flag
	 */

	/* Calculate checksum */
	checksum_compute(sds, SDS_LEN_NO_CS, &sds->checksum);
	/*
	 * 6. FAIL: CORRECT UUID, CORRECT USC, INCORRECT checksum, CLEAR flag
	 */

	/* Flush */
	persist(sds, sizeof(*sds));
	/*
	 * 7. FAIL: CORRECT UUID, CORRECT USC, CORRECT checksum, CLEAR flag
	 */

	return 0;
}

/*
 * sds_set_dirty_flag -- set dirty pool flag
 */
static void
sds_set_dirty_flag(pmem2_persist_fn persist, struct shutdown_state *sds)
{
	/* Set flag */
	sds->flag = DIRTY_FLAG;
	/*
	 * 8. FAIL: GOOD UUID, CORRECT USC, UNDEFINED checksum, UNDEFINED flag
	 * checksum depends on flag, if flag is UNDEFINED, checksum is so on
	 */

	/* Flush */
	persist(sds, sizeof(*sds));
	/*
	 * 9. FAIL: CORRECT UUID, CORRECT USC, INCORRECT checksum, DIRTY flag
	 */

	/* Calculate checksum */
	checksum_compute(sds, SDS_LEN_NO_CS, &sds->checksum);
	/*
	 * 10. FAIL: CORRECT UUID, CORRECT USC, CORRECT checksum, DIRTY flag
	 */

	/* Flush */
	persist(sds, sizeof(*sds));
	/*
	 * 11. FAIL: CORRECT UUID, CORRECT USC, CORRECT checksum, DIRTY flag
	 */
}

/*
 * save_string_to_pmem -- save something else to the pool and flush
 */
static void
save_string_to_pmem(pmem2_persist_fn persist, void *memory_above_sds)
{
	/* Save something else to the pool and flush */
	char *string_to_save = "hello, persistent memory";
	strcpy(memory_above_sds, string_to_save);
	persist(memory_above_sds, sizeof(*string_to_save));
	/*
	 * 12. FAIL: CORRECT UUID, CORRECT USC, CORRECT checksum, CLEAR flag
	 */
}

/*
 * sds_set_clear_flag -- set clear pool flag
 */
static void
sds_set_clear_flag(pmem2_persist_fn persist, struct shutdown_state *sds)
{
	/* Clear flag */
	sds->flag = CLEAR_FLAG;
	/*
	 * 13 .FAIL: CORRECT UUID, CORRECT USC, UNDEFINED checksum,
	 * UNDEFINED flag
	 * checksum depends on flag, if flag is UNDEFINED, checksum is so on
	 */

	/* Flush */
	persist(sds, sizeof(*sds));
	/*
	 * 14. FAIL: CORRECT UUID, CORRECT USC, INCORRECT checksum, CLEAR flag
	 */

	/* Calculate checksum */
	checksum_compute(sds, SDS_LEN_NO_CS, &sds->checksum);
	/*
	 * 15. FAIL: CORRECT UUID, CORRECT USC, INCORRECT checksum, CLEAR flag
	 */

	/* Flush */
	persist(sds, sizeof(*sds));
	/*
	 * 16. FAIL: CORRECT UUID, CORRECT USC, CORRECT checksum, CLEAR flag
	 */
}

/*
 * sds_verify -- read mapping and check/fix integrity of the data
 */
static int
sds_verify(pmem2_persist_fn persist, pmem2_memset_fn memset_fn,
		struct pmem2_source *src, struct shutdown_state *sds)
{
	/* variables used to compare data with the sds */
	uint64_t usc; /* unsafe shutdown count */
	uint8_t uuid[UUID_LEN]; /* UID */
	uint64_t checksum;

	/* let's get dimm's UUID, USC and get sds checksum */
	size_t len = SDS_LEN_NO_CS;
	int ret = pmem2_source_device_id(src, (char *)uuid, &len);
	if (ret) {
		pmem2_perror("pmem2_source_device_id");
		return ret;
	}

	ret = pmem2_source_device_usc(src, &usc);

	if (ret) {
		if (ret == PMEM2_E_NOSUPP) {
		printf("Getting unsafe shutdown count is not supported"
					" on this system\n");
		} else {
			pmem2_perror("pmem2_source_device_usc");
			return ret;
		}
	}
	checksum_compute(sds, SDS_LEN_NO_CS, &checksum);

	bool is_uuid_usc_correct = (sds->uuid == uuid) && (sds->usc == usc);
	bool is_checksum_correct = sds->checksum == checksum;
	bool is_flag_correct = sds->flag == CLEAR_FLAG;

	if (is_uuid_usc_correct) {
		if (is_flag_correct) {
			/* Points: 8, 16 - POOL is OK */
			if (is_checksum_correct) {
				return 0;
			/*
			 * Points: 13, 14, 15 - fix the checksum and POOL is OK
			 */
			} else {
				checksum_fix(persist, sds, SDS_LEN_NO_CS,
						&sds->checksum);
				return 0;
			}
		/*
		 * Points: 11, 12, 13 - POOL is OK, POOL was not closed
		 * but there was no ADR failure
		 */
		} else {
			if (is_checksum_correct) {
				return 0;
			/*
			 * Points: 8, 9, 10 - fix the checksum and POOL is OK
			 */
			} else {
				checksum_fix(persist, sds, SDS_LEN_NO_CS,
						&sds->checksum);
				return 0;
			}
		}
	} else {
		if (is_flag_correct) {
			/*
			 * Points: 3 - fix UUID and USC and POOL is OK, there
			 * was ADR failure but the POOL was not opened
			 */
			if (is_checksum_correct) {
				uuid_usc_fix(persist, sds, uuid, usc);
				return 0;
			/*
			 * Points: 4, 5, 6, 7 - reinit sds and POOL is OK
			 */
			} else {
				sds_reinit(memset_fn, sds);
				return 0;
			}
		} else {
			if (is_checksum_correct) {
				return 0;
			/*
			 * Points: 1, 2 - reinit sds and POOL is OK
			 */
			} else {
				sds_reinit(memset_fn, sds);
				return 0;
			}
		}
	}
	return 0;
}

/*
 * read_string_from_pmem -- read and print string
 * (which was recently saved) from pmem
 */
static void
read_string_from_pmem(void *memory_above_sds)
{
	char *string_to_print = memory_above_sds;

	printf("%s\n", string_to_print);
}

int
main(int argc, char *argv[])
{
	if (argc != 3 && (strcmp(argv[1],
			"create") || strcmp(argv[1], "read"))) {
		fprintf(stderr, "usage: %s [create|read] file\n",
				argv[0]);
		return 1;
	}

	char *mode = argv[1];
	char *file = argv[2];

	struct pmem2_config *cfg = NULL;
	struct pmem2_source *src = NULL;
	struct pmem2_map *map = NULL;

	int ret;
	int fd = open(file, O_RDWR);
	if (fd < 0) {
		perror("open");
		ret = 1;
		goto err_fd;
	}

	ret = pmem2_config_new(&cfg);
	if (ret) {
		pmem2_perror("pmem2_config_new");
		goto err_config;
	}

	ret = pmem2_source_from_fd(&src, fd);
	if (ret) {
		pmem2_perror("pmem2_source_from_fd");
		goto err_source;
	}

	ret = pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE);
	if (ret) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		goto err_granularity_or_map;
	}

	ret = pmem2_map(cfg, src, &map);
	if (ret) {
		pmem2_perror("pmem2_map");
		goto err_granularity_or_map;
	}

	void *mapping_addr = pmem2_map_get_address(map);
	pmem2_persist_fn persist = pmem2_get_persist_fn(map);
	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
	struct shutdown_state *sds = NULL;

	/* File mapping */
	sds = (struct shutdown_state *)mapping_addr;
	/*
	 * 1. FAIL: UNDEFINED UUID, UNDEFINED USC, INCORRECT checksum,
	 * UNDEFINED flag
	 */

	/* let's get pointer to the memory just above the sds structure  */
	void *memory_above_sds = (char *)sds + SDS_LEN;

	if (strcmp(mode, "create") == 0) {
		ret = sds_init(persist, memset_fn, src, sds);
		if (ret)
			goto err;
		sds_set_dirty_flag(persist, sds);
		save_string_to_pmem(persist, memory_above_sds);
		sds_set_clear_flag(persist, sds);
	}

	if (strcmp(mode, "read") == 0) {
		ret = sds_verify(persist, memset_fn, src, sds);
		if (ret)
			goto err;
		read_string_from_pmem(memory_above_sds);
	}

	ret = 0;

err:
	pmem2_unmap(&map);
err_granularity_or_map:
	pmem2_source_delete(&src);
err_source:
	pmem2_config_delete(&cfg);
err_config:
	close(fd);
err_fd:
	return ret;
}
