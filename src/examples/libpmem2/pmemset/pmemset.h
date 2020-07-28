// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#ifndef LIBPMEMSET_H
#define LIBPMEMSET_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <libpmem2.h>

struct pmemset;
struct pmemset_config;
struct pmemset_source;
struct pmemset_part;
struct pmemset_part_map;

int pmemset_config_new(struct pmemset_config **config);

/*
 * By default, the library only attempts to open the parts. These two functions
 * modify this behavior.
 * Create if none behaves like O_CREAT, but also initializes part metadata.
 * Create if invalid checks if the existing file contains a valid pool,
 * and if not, creates a new one (this is useful for close-to-open).
 */
void pmemset_config_set_create_if_none(struct pmemset_config *config,
	int value);
void pmemset_config_set_create_if_invalid(struct pmemset_config *config,
	int value);

/*
 * The synchronous event stream. These events are generated inside of the
 * flush/memcpy functions on the set.
 */
enum pmemset_event {
	PMEMSET_EVENT_COPY,
	PMEMSET_EVENT_FLUSH,
	PMEMSET_EVENT_DRAIN,
	PMEMSET_EVENT_PERSIST,
	PMEMSET_EVENT_BAD_BLOCK,
	PMEMSET_EVENT_PART_ADD,
	PMEMSET_EVENT_PART_REMOVE,
};

struct pmemset_event_copy {
	void *addr;
	size_t len;
};

struct pmemset_event_flush {
	void *addr;
	size_t len;
};

struct pmemset_event_drain {
};

struct pmemset_event_persist {
	void *addr;
	size_t len;
};

struct pmemset_event_bad_block {
	void *addr;
	size_t len;
};

struct pmemset_event_part_remove {
	void *addr;
	size_t len;
	int fd;
};

struct pmemset_event_part_add {
	void *addr;
	size_t len;
	int fd;
};

struct pmemset_event_context {
	enum pmemset_event type;
	union {
		char _data[64];
		struct pmemset_event_copy copy;
		struct pmemset_event_flush flush;
		struct pmemset_event_drain drain;
		struct pmemset_event_persist persist;
		struct pmemset_event_bad_block bad_block;
		struct pmemset_event_part_remove part_remove;
		struct pmemset_event_part_add part_add;
	} data;
};

/*
 * This callback can be used to create a copy of the data or directly
 * replicate it somewhere. This is *not* an append-only log, nor is the
 * data versioned in any way. Once the function exits, the memory range
 * can no longer be accessed.
 * There's no guarantee that accessing the data inside of the callback
 * is thread-safe. The library user must guarantee this by not
 * having multiple threads mutating the same region on the set.
 */
typedef int pmemset_event_callback(struct pmemset *set,
	struct pmemset_event_context *context, void *arg);

void pmemset_config_set_event_callback(struct pmemset_config *config,
	pmemset_event_callback *callback, void *arg);

/*
 * Sets the address space reservation that will be used by the pmemset.
 */
void pmemset_config_set_reservation(struct pmemset_config *config,
	struct pmem2_vm_reservation *rsv);

/*
 * By default, pmemset will not place parts contiguously in memory. This config
 * parameters needs to be set to true for that to happen.
 * This will also merge any runtime structures for the parts so that it consumes
 * fewer runtime resources.
 */
void pmemset_config_set_contiguous_part_coalescing(
	struct pmemset_config *config, int value);

/*
 * Layout and Versioning fields for the metadata.
 * Can be left blank - defaults to 0.
 */
void pmemset_config_set_layout_name(struct pmemset_config *config,
	const char *layout);
void pmemset_config_set_version(struct pmemset_config *config,
	int major, int minor);

void pmemset_config_delete(struct pmemset_config **config);

/*
 * Creates new pmemset source.
 * This source is then used to create one or more parts.
 */
int pmemset_source_from_external(struct pmemset_source **source,
	struct pmem2_source *ext_source);

#ifdef WIN32
int pmemset_source_from_wfile(struct pmemset_source **source,
	const wchar *file);
int pmemset_source_from_wfile_params(struct pmemset_source **source,
	const wchar *file);
#endif
int pmemset_source_from_file(struct pmemset_source **source,
	const char *file);

int pmemset_source_from_temporary(struct pmemset_source **source,
	const char *dir);

/*
 * Need to come up with a better OS-agnostic name.
 * The functionality we need is to extend, shrink and punch a hole in a
 * source.
 */
int pmemset_source_fallocate(struct pmemset_source *source, int flags,
	size_t offset, size_t size);

void pmemset_source_delete(struct pmemset_source **source);

/*
 * Creates a new set. This does not create any mappings.
 */
int pmemset_new(struct pmemset **set, struct pmemset_config *config);

/*
 * Shutdown state data must be stored by the user externally for reliability.
 * This needs to be read by the user and given to the add part function so that
 * the current shutdown state can be compared with the old one.
 */
struct pmemset_part_shutdown_state_data {
	const char data[1024];
};

/*
 * Just like shutdown state, the header for the set is external and managed
 * by the user. It contains the basic features like versioning, layout names,
 * and compatibility checking.
 *
 * TODO: compat/incompat features, think about UUIDs for parts.
 */
struct pmemset_header {
	char data[1024];
};

void pmemset_header_init(struct pmemset_header *header,
	struct pmemset_config *config);

/*
 * Creates a new part associated with a set.
 * The part is not directly accessible until it's finalized.
 */
int pmemset_part_new(struct pmemset_part **part,
	struct pmemset *set,
	struct pmemset_source *source,
	size_t offset, size_t length);

/*
 * Deletes a part. This only needs to be called if the part was never
 * used to create a map.
 */
void pmemset_part_delete(struct pmemset_part **part);

/*
 * Performs a read/write on a part and a user buffer,
 * returns an error if there's a bad block on the region.
 *
 * These can be used to fix bad blocks and safely read/write
 * sds and header data.
 */
ssize_t pmemset_part_pread_mcsafe(
	struct pmemset_part *part,
	void *dst, size_t size, size_t offset);

ssize_t pmemset_part_pwrite_mcsafe(
	struct pmemset_part *part,
	void *dst, size_t size, size_t offset);

enum pmemset_part_state {
	/*
	 * The pool state cannot be determined because of errors during
	 * retrieval of device information.
	 */
	PMEMSET_PART_STATE_INDETERMINATE,

	/*
	 * The pool is internally consistent and was closed cleanly.
	 * Application can assume that no custom recovery is needed.
	 */
	PMEMSET_PART_STATE_OK,

	/*
	 * The pool is internally consistent, but it was not closed cleanly.
	 * Application must perform consistency checking and custom recovery
	 * on user data.
	 */
	PMEMSET_PART_STATE_OK_BUT_INTERRUPTED,

	/*
	 * The pool can contain invalid data as a result of hardware failure.
	 * Reading the pool is unsafe.
	 */
	PMEMSET_PART_STATE_CORRUPTED,
};

/*
 * Consumes a mapping and creates its mapping. Performs all additional
 * checks and fires off all relevant part-related events
 * (add/remove and bad blocks).
 *
 * This function optionally takes input and output header and sds data
 * structures. The input struct is used to check if all the relevant invariants
 * are upheld. E.g., the header if layout and version matches.
 * The output struct must be then written out and reused the next time.
 * TODO: we should probably force the header to reside somewhere in the pool.
 * It will be easier from the API point of view, and we will have more
 * flexiblity to mutate it. Right now there's only one field we might want to
 * change inside of it, and that's the graceful shutdown check.
 *
 * This function also returns the state of the part which indicates whether
 * reading from the part is safe.
 */
int pmemset_part_map_new(
	struct pmemset_part_map **pmap,
	struct pmemset_part **part,
	const struct pmemset_header *header_in,
	struct pmemset_header *header_out,
	const struct pmemset_part_shutdown_state_data *sds_in,
	struct pmemset_part_shutdown_state_data *sds_out,
	enum pmemset_part_state *state);

/*
 * Parts can be removed from the set at runtime.
 */
void pmemset_remove_part_map(struct pmemset *set,
	struct pmemset_part_map **part);

/*
 * If this were to punch a hole in the middle of the mapping and
 * the pmemset_config_set_contiguous_part_coalescing is set to true,
 * then the function will fail.
 */
int pmemset_remove_range(struct pmemset *set, void *addr, size_t length);

/*
 * Since data in a set is not necessarily contiguous, the parts have their
 * own iteration and accessor functions.
 * Parts can be coalesced if possible, meaning that there might be fewer parts
 * in the iterator than was added (but the total amount of available memory
 * should be the same).
 */
void *pmemset_part_map_address(struct pmemset_part_map *pmap);
size_t pmemset_part_map_length(struct pmemset_part_map *pmap);

void pmemset_part_map_first(struct pmemset *set,
	struct pmemset_part_map **pmap);
void pmemset_part_map_next(struct pmemset *set,
	struct pmemset_part_map **pmap);
int pmemset_part_by_address(struct pmemset *set,
	struct pmemset_part_map **pmap, void *addr);

/*
 * Parts are owned by the set, not the caller. They need to be dropped when no
 * longer needed.
 * This function can also be used to drop a part that has not been turned into
 * a mapping.
 */
void pmemset_part_map_drop(struct pmemset_part_map **pmap);

/*
 * Same functions as in libpmem2 map, but for the pmemset.
 * The lowest common granularity across the parts is chosen for those functions.
 */
void pmemset_persist(struct pmemset *set, const void *ptr, size_t size);
void pmemset_flush(struct pmemset *set, const void *ptr, size_t size);
void pmemset_drain(struct pmemset *set);

#define PMEMSET_F_MEM_NODRAIN	(1U << 0)

#define PMEMSET_F_MEM_NONTEMPORAL	(1U << 1)
#define PMEMSET_F_MEM_TEMPORAL	(1U << 2)

#define PMEMSET_F_MEM_WC		(1U << 3)
#define PMEMSET_F_MEM_WB		(1U << 4)

#define PMEMSET_F_MEM_NOFLUSH	(1U << 5)

#define PMEMSET_F_MEM_VALID_FLAGS (PMEMSET_F_MEM_NODRAIN | \
		PMEMSET_F_MEM_NONTEMPORAL | \
		PMEMSET_F_MEM_TEMPORAL | \
		PMEMSET_F_MEM_WC | \
		PMEMSET_F_MEM_WB | \
		PMEMSET_F_MEM_NOFLUSH)

void *pmemset_memmove(struct pmemset *set, void *pmemdest,
	const void *src, size_t len, unsigned flags);

void *pmemset_memcpy(struct pmemset *set, void *pmemdest,
	const void *src, size_t len, unsigned flags);

void *pmemset_memset(struct pmemset *set, void *pmemdest,
	int c, size_t len, unsigned flags);

int pmemset_deep_flush(struct pmemset *set, void *ptr, size_t size);

/*
 * This deletes all the runtime state and unmaps all the parts. There's no need
 * for the application to track parts and delete parts on close.
 * This must be called so that the graceful shutdown can be tracked.
 */
void pmemset_delete(struct pmemset **set);

#endif	/* libpmemset.h */
