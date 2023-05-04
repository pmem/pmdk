// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021-2023, Intel Corporation */

/*
 * ringbuf.c -- a simple lock-free single producer single consumer ring buffer
 *	implemented using libpmem2.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <libpmem2.h>

#define RINGBUF_SIGNATURE "my_fast_ringbuf"
#define RINGBUF_SIGNATURE_LEN (sizeof(RINGBUF_SIGNATURE))

#define RINGBUF_POS_PERSIST_BIT (1ULL << 63)

/*
 * Persistent on-media format of the ring buffer.
 *
 * Fields are carefully aligned and padded. This is done to avoid: a) flushing
 * metadata fields when unnecessary, and b) misaligned non-temporal writes.
 */
struct ringbuf_data {
	uint8_t signature[RINGBUF_SIGNATURE_LEN]; /* 0 - 16 */

	uint64_t nentries; /* 16 - 24 */

	uint64_t entry_size; /* 24 - 32 */

	uint8_t padding[32]; /* 32 - 64 */

	struct {
		uint64_t read;	/* 64 - 72 */
		uint64_t write; /* 72 - 80 */
	} pos;
	uint8_t padding2[48]; /* 80 - 128 */

	uint8_t data[]; /* 128 - */
};

/* on-media format of a single entry. nothing but data. */
struct ringbuf_entry {
	uint8_t data[0];
};

/* runtime (ephemeral) ring buffer state */
struct ringbuf {
	struct pmem2_map *map;

	pmem2_persist_fn persist;
	pmem2_memcpy_fn memcpy;
	enum pmem2_granularity granularity;

	struct ringbuf_data *data;
};

/*
 * ringbuf_data_is_initialized -- checks whether the ring buffer data has
 * initialized signature in the header. If so, it's assumed that the
 * rest of the on-media format is valid as well.
 */
static int
ringbuf_data_is_initialized(const struct ringbuf_data *rbuf_data)
{
	return memcmp(rbuf_data->signature, RINGBUF_SIGNATURE,
			RINGBUF_SIGNATURE_LEN) == 0;
}

/*
 * ringbuf_data_force_page_allocation -- ensures that all the pages for
 * the ring buffer data are allocated by rewriting a byte from each page.
 * This serves two purposes: a) eliminating the kernel page allocation overheads
 * from the time measurements, and b) making sure that the process won't be
 * killed due to lack of space.
 */
static void
ringbuf_data_force_page_allocation(struct ringbuf_data *rbuf_data, size_t size)
{
	long pagesize = sysconf(_SC_PAGESIZE);
	volatile char *cur_addr = (char *)rbuf_data;
	char *addr_end = (char *)cur_addr + size;
	for (; cur_addr < addr_end; cur_addr += pagesize) {
		*cur_addr = *cur_addr;
		VALGRIND_SET_CLEAN(cur_addr, 1);
	}
}

/*
 * ringbuf_initialize_data -- initializes the header of the on-media
 * format in a fail-safe atomic manner.
 * The data is written out and persisted first, followed by the signature.
 */
static void
ringbuf_initialize_data(struct ringbuf *rbuf, uint64_t nentries,
			uint64_t entry_size)
{
	rbuf->data->pos.write = 0;
	rbuf->data->pos.read = 0;
	rbuf->data->nentries = nentries;
	rbuf->data->entry_size = entry_size + sizeof(struct ringbuf_entry);
	rbuf->persist(rbuf->data, sizeof(*rbuf->data));

	rbuf->memcpy(rbuf->data->signature, RINGBUF_SIGNATURE,
			RINGBUF_SIGNATURE_LEN, 0);
}

/*
 * ringbuf_new -- creates a new instance of the ring buffer on the provided
 * source. If the source contains an already initialized ring buffer,
 * that existing data will be made accessible again.
 */
static struct ringbuf *
ringbuf_new(struct pmem2_source *source, uint64_t entry_size)
{
	struct ringbuf *rbuf = malloc(sizeof(*rbuf));
	if (rbuf == NULL)
		return NULL;

	struct pmem2_config *config;
	if (pmem2_config_new(&config) != 0) {
		pmem2_perror("pmem2_config_new");
		goto err_config;
	}

	if (pmem2_config_set_required_store_granularity(config,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		goto err_config;
	}

	if (pmem2_map_new(&rbuf->map, config, source) != 0) {
		pmem2_perror("pmem2_map_new");
		goto err_map;
	}

	rbuf->data = pmem2_map_get_address(rbuf->map);
	rbuf->persist = pmem2_get_persist_fn(rbuf->map);
	rbuf->memcpy = pmem2_get_memcpy_fn(rbuf->map);
	rbuf->granularity = pmem2_map_get_store_granularity(rbuf->map);

	size_t size = pmem2_map_get_size(rbuf->map);
	size -= sizeof(struct ringbuf_data);
	size_t nentries = size / entry_size;

	if (!ringbuf_data_is_initialized(rbuf->data)) {
		ringbuf_initialize_data(rbuf, nentries, entry_size);
	}

	if (rbuf->data->entry_size != entry_size) {
		fprintf(stderr,
			"entry size (%lu) different than provided (%lu)\n",
			rbuf->data->entry_size, entry_size);
		goto err_layout;
	}

	if (rbuf->data->nentries != nentries) {
		fprintf(stderr,
			"number of entries (%lu) different than provided (%lu)\n",
			rbuf->data->nentries, nentries);
		goto err_layout;
	}

	ringbuf_data_force_page_allocation(rbuf->data, size);
	pmem2_config_delete(&config);

	return rbuf;

err_layout:
	pmem2_map_delete(&rbuf->map);
err_map:
	pmem2_config_delete(&config);
err_config:
	free(rbuf);
	return NULL;
}

/*
 * ringbuf_delete -- deletes the runtime state of a ring buffer.
 * This does not have any effect on the underlying data.
 */
static void
ringbuf_delete(struct ringbuf *rbuf)
{
	pmem2_map_delete(&rbuf->map);
	free(rbuf);
}

/*
 * ringbuf_entry_size -- returns the size of a single ring buffer entry
 */
static uint64_t
ringbuf_entry_size(const struct ringbuf *rbuf)
{
	return rbuf->data->entry_size;
}

/*
 * ringbuf_data_entry_get -- returns a pointer to a ring buffer entry
 * with a given position.
 */
static struct ringbuf_entry *
ringbuf_data_entry_get(const struct ringbuf_data *rbuf, size_t pos)
{
	return (void *)rbuf->data + (rbuf->entry_size * pos);
}

/*
 * ringbuf_store_position -- atomically updates a ring buffer position
 */
static void
ringbuf_store_position(struct ringbuf *rbuf, uint64_t *pos, uint64_t val)
{
	/*
	 * Ordinarily, an atomic store becomes globally visible prior to being
	 * persistent. This is a problem since applications have to make sure
	 * that they never make progress on data that isn't yet persistent.
	 * In this example, this is addressed by using the MSB of the value as a
	 * "possibly-not-yet-persistent flag".
	 * First, a value is stored with that flag set, persisted, and then
	 * stored again with the flag cleared. Any threads that load
	 * that variable need to first check the flag and, if set, persist it
	 * before proceeding.
	 * This ensures that the loaded variable is always persistent.
	 *
	 * However, if the map can be persistently written to with byte
	 * granularity (i.e., the system is eADR equipped), then data
	 * visibility is the same as data persistence. This eliminates the
	 * need for the persistent flag algorithm.
	 */
	if (rbuf->granularity == PMEM2_GRANULARITY_BYTE) {
		__atomic_store_n(pos, val, __ATOMIC_RELEASE);
		VALGRIND_SET_CLEAN(pos, sizeof(val));
	} else {
		__atomic_store_n(pos, val | RINGBUF_POS_PERSIST_BIT,
				__ATOMIC_RELEASE);
		rbuf->persist(pos, sizeof(val));

		__atomic_store_n(pos, val, __ATOMIC_RELEASE);
		rbuf->persist(pos, sizeof(val));
	}
}

/*
 * ringbuf_load_position -- atomically loads the ring buffer positions
 */
static void
ringbuf_load_position(const struct ringbuf *rbuf,
	uint64_t *read, uint64_t *write)
{
	uint64_t w;
	uint64_t r;

	w = __atomic_load_n(&rbuf->data->pos.write, __ATOMIC_ACQUIRE);
	r = __atomic_load_n(&rbuf->data->pos.read, __ATOMIC_ACQUIRE);

	/* on systems with byte store granularity, this will never be true */
	if (w & RINGBUF_POS_PERSIST_BIT || r & RINGBUF_POS_PERSIST_BIT) {
		/*
		 * We could store the value with the persist bit cleared,
		 * helping other threads make progress. But, in this case,
		 * it's likely that the coordination required to do that safely
		 * would be more costly than the current approach.
		 */
		rbuf->persist(&rbuf->data->pos, sizeof(rbuf->data->pos));
		w &= ~RINGBUF_POS_PERSIST_BIT;
		r &= ~RINGBUF_POS_PERSIST_BIT;
	}

	*read = r;
	*write = w;
}

/*
 * ringbuf_enqueue -- atomically appends a new entry to the ring buffer.
 * This function fails if the ring buffer is full.
 */
static int
ringbuf_enqueue(struct ringbuf *rbuf, const char *src)
{
	uint64_t r;
	uint64_t w;
	ringbuf_load_position(rbuf, &r, &w);

	uint64_t w_next = (w + 1) % rbuf->data->nentries;
	if (w_next == r) /* ring buffer is full */
		return -1;

	struct ringbuf_entry *entry = ringbuf_data_entry_get(rbuf->data, w);
	rbuf->memcpy(entry->data, src, rbuf->data->entry_size,
		PMEM2_F_MEM_NONTEMPORAL);

	ringbuf_store_position(rbuf, &rbuf->data->pos.write, w_next);

	return 0;
}

/*
 * ringbuf_dequeue -- atomically removes one entry from the ring buffer.
 * This function fails if the ring buffer is empty.
 */
static int
ringbuf_dequeue(struct ringbuf *rbuf, char *dst)
{
	uint64_t r;
	uint64_t w;
	ringbuf_load_position(rbuf, &r, &w);

	if (w == r) /* ring buffer is empty */
		return -1;

	struct ringbuf_entry *entry = ringbuf_data_entry_get(rbuf->data, r);

	memcpy(dst, entry->data, rbuf->data->entry_size);

	ringbuf_store_position(rbuf, &rbuf->data->pos.read,
			(r + 1) % rbuf->data->nentries);

	return 0;
}

struct thread_args {
	struct ringbuf *rbuf;
	size_t nops;
};

/*
 * thread_consumer -- dequeues data from the ring buffer a given number
 * of times. Busy loops if the ring buffer is empty.
 * Ideally, if used as a benchmark, this thread would be pinned to a dedicated
 * core. This is not done in the example as the code needs to remain generic.
 */
static void *
thread_consumer(void *arg)
{
	struct thread_args *targs = arg;
	struct ringbuf *rbuf = targs->rbuf;
	uint64_t entry_size = ringbuf_entry_size(rbuf);
	char *dst = malloc(entry_size);

	for (int i = 0; i < targs->nops; ++i) {
		/* busy loop is intentional, avoids coordination overhead */
		while (ringbuf_dequeue(rbuf, dst) != 0)
			;
	}

	free(dst);

	return NULL;
}

/*
 * thread_producer -- enqueues data into the ring buffer a given number
 * of times. Busy loops if the ring buffer is full.
 * Ideally, if used as a benchmark, this thread would be pinned to a dedicated
 * core. This is not done in the example as the code needs to remain generic.
 */
static void *
thread_producer(void *arg)
{
	struct thread_args *targs = arg;
	struct ringbuf *rbuf = targs->rbuf;
	uint64_t entry_size = ringbuf_entry_size(rbuf);
	char *src = malloc(entry_size);
	memset(src, 0xc, entry_size);

	for (int i = 0; i < targs->nops; ++i) {
		/* busy loop is intentional, avoids coordination overhead */
		while (ringbuf_enqueue(rbuf, src) != 0)
			;
	}

	free(src);

	return NULL;
}

/*
 * helper_time_delta_sec -- a helper function that calculates the number of
 * seconds elapsed between two timed events.
 */
static float
helper_time_delta_sec(struct timespec *start, struct timespec *end)
{
#define SEC_NSEC (1.0 / 1000000000)
	return ((float)end->tv_sec + (SEC_NSEC * end->tv_nsec)) -
		((float)start->tv_sec + (SEC_NSEC * start->tv_nsec));
#undef SEC_NSEC
}

int
main(int argc, const char *argv[])
{
	int fd;
	struct pmem2_source *src;

	if (argc != 4) {
		fprintf(stderr, "usage: %s file entry_size nops\n", argv[0]);
		exit(1);
	}

	size_t entry_size = atoll(argv[2]);
	if (entry_size == 0 || entry_size > (2 << 20)) {
		fprintf(stderr,
			"invalid entry size, must be between 0 and 2MB\n");
		exit(1);
	}

	size_t nops = atoll(argv[3]);
	if (nops == 0) {
		fprintf(stderr, "invalid number of operations\n");
		exit(1);
	}

	if ((fd = open(argv[1], O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		close(fd);
		exit(1);
	}

	struct ringbuf *rbuf = ringbuf_new(src, entry_size);
	if (rbuf == NULL) {
		close(fd);
		exit(1);
	}

	struct thread_args args;
	args.nops = nops;
	args.rbuf = rbuf;

	struct timespec start;
	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	pthread_t threads[2];
	/*
	 * Ideally these threads would be pinned with
	 * pthread_attr_setaffinity_np to dedicated cores. But since this is
	 * a generic example, that is left as an exercise for the reader.
	 */
	pthread_create(&threads[0], NULL, thread_producer, &args);
	pthread_create(&threads[1], NULL, thread_consumer, &args);

	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);

	clock_gettime(CLOCK_MONOTONIC, &end);
	float time = helper_time_delta_sec(&start, &end);
	printf("Time elapsed: %f seconds\n", time);
	printf("Bandwidth: %f megabytes per second\n",
		(((nops * 2) * entry_size) / time) / 1024 / 1024);

	pmem2_source_delete(&src);
	close(fd);
	ringbuf_delete(rbuf);

	return 0;
}
