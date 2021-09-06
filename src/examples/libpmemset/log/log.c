// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * log.c -- libpmemset based simple write ahead log with binary replication
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <libpmemset.h>

#define LOG_NAME_LEN 32
#define LOG_PART_SIZE (4 * 1024 * 1024) /* 4 MB */
#define MAX_PART 999999

struct entry {
	size_t len;
	char data[];
};

/*
 * this structure will be updated for each append - for best write performance
 * we increase its size to write it as an enire page at one
 */
struct header {
	size_t last;
	char unused[4096 - sizeof(size_t)];
};

struct replica {
	struct pmemset *set;
	struct pmemset_sds sds;
	char *name;
	const char *dir;
	size_t len;
	char *offset_ptr;
	char *data;
};

struct log {
	struct pmemset *set;
	const char *dir;
	char *name;
	int create_new_parts;
	size_t len;
	unsigned partsNum;
	struct header hdr; /* volatile copy */
	struct pmemset_sds sds;

	struct log_part_pmem *log;
	struct replica rep;
};

struct log_part_pmem {
	struct header hdr;
	char data[]; /* struct entry */
};

/*
 * log_update_hdr -- store volatile copy of the header to the pmem
 */
static void
log_update_hdr(struct pmemset *set, struct log *l)
{
	/*
	 * by using non temporal stores and increasing size of log to full page,
	 * we eliminate any potential cache miss during this operation
	 */
	pmemset_memcpy(set, &l->log->hdr, &l->hdr, sizeof(l->log->hdr),
		PMEMSET_F_MEM_NONTEMPORAL);
}

/*
 * assemble_path -- utility function to concatenate path to the part file
 */
static char *
assemble_path(const char *dir, const char *name, unsigned partNum)
{
	size_t len = strlen(name) + 1;

	if (len > LOG_NAME_LEN) {
		fprintf(stderr, "log name: %s is to long ", name);
		return NULL;
	}

	/* '.' + up to 10 digits + '\0' 2^32 has 10 digits in decimal */
	len += strlen(dir) + 12;
	char *path = malloc(len);

	if (path == NULL) {
		perror("malloc failed");
		return NULL;
	}

	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, name);
	if (partNum != 0) {
		sprintf(path + strlen(path), ".%d", partNum);
	}

	return path;
}

/*
 * part_create -- create or open part file
 */
static int
part_create(struct pmemset *set, const char *dir, const char *name, bool create,
	unsigned partNum, struct pmemset_sds *sds,
	struct pmemset_part_descriptor *desc)
{
	struct pmemset_map_config *config;
	if (pmemset_map_config_new(&config)) {
		pmemset_perror("pmemset_map_config_new");
		return -1;
	}

	char *path = assemble_path(dir, name, partNum);

	struct pmemset_source *src;
	int ret;
	unsigned flags = 0;
	if (create) {
		if (pmemset_source_from_file(&src, path) == 0) {
			fprintf(stderr,
				"log: %s already exists, please delete it before continuing\n",
				path);
			pmemset_source_delete(&src);
			free(path);
			return -1;
		}

		pmemset_map_config_set_length(config, LOG_PART_SIZE);
		flags = PMEMSET_SOURCE_FILE_CREATE_ALWAYS;
	}

	ret = pmemset_xsource_from_file(&src, path, flags);
	free(path);

	if (ret) {
		if (ret == PMEMSET_E_INVALID_SOURCE_PATH)
			return 1;

		pmemset_perror("pmemset_xsource_from_file");
		return -1;
	}

	enum pmemset_part_state state = PMEMSET_PART_STATE_OK |
		PMEMSET_PART_STATE_OK_BUT_ALREADY_OPEN |
		PMEMSET_PART_STATE_OK_BUT_INTERRUPTED|
		PMEMSET_PART_STATE_INDETERMINATE;

	pmemset_source_set_sds(src, sds, &state);

	ret = pmemset_map(set, src, config, desc);
	if (ret == PMEMSET_E_SDS_ENOSUPP) {
		pmemset_source_set_sds(src, NULL, NULL);
		ret = pmemset_map(set, src, config, desc);
	}

	if (ret) {
		pmemset_perror("pmemset_map");
		return -1;
	}

	return 0;
}

/*
 * replica_event_callback -- sds event handler for a replica set
 */
static int
replica_event_callback(struct pmemset *set, struct pmemset_event_context *ctx,
	void *arg) {
	struct replica *rep = arg;
	if (ctx->type == PMEMSET_EVENT_SDS_UPDATE) {
		size_t len = strlen(rep->dir) + 5;
		char *path = malloc(len);

		if (path == NULL) {
			perror("malloc failed");
			exit(1);
		}

		strcpy(path, rep->dir);
		strcat(path, "/");
		strcat(path, "sds");
		int fd = open(path, O_CREAT|O_RDWR|O_TRUNC,  0660);
		struct pmemset_event_sds_update data = ctx->data.sds_update;
		write(fd, data.sds, sizeof(*data.sds));
		close(fd);
		free(path);

	}
	return 0;
}

/*
 * log_event_callback -- replication and sds handler for a main set
 */
static int
log_event_callback(struct pmemset *set, struct pmemset_event_context *ctx,
	void *arg) {
	struct log *log = arg;
	struct replica *rep = &log->rep;
	if (ctx->type == PMEMSET_EVENT_PART_ADD) {
		if (!log->create_new_parts)
			return 0;

		struct pmemset_part_descriptor desc;

		int ret = part_create(rep->set, rep->dir, log->name, true,
			log->partsNum, &rep->sds, &desc);
		if (ret)
			exit(1);

		if (rep->data == NULL) /* First replica part created */
			rep->data = desc.addr;

		rep->len += desc.size;

	}
	if (ctx->type == PMEMSET_EVENT_SDS_UPDATE) {
		size_t len = strlen(log->dir) + sizeof("/sds");
		char *path = malloc(len);

		if (path == NULL) {
			perror("malloc failed");
			exit(1);
		}

		strcpy(path, log->dir);
		strcat(path, "/sds");

		int fd = open(path, O_CREAT|O_RDWR|O_TRUNC,  0660);
		struct pmemset_event_sds_update data = ctx->data.sds_update;
		write(fd, data.sds, sizeof(*data.sds));
		close(fd);
		free(path);
	}
	if (ctx->type == PMEMSET_EVENT_COPY) {
		struct pmemset_event_copy data = ctx->data.copy;
		size_t offset = (size_t)data.dest - (size_t)rep->offset_ptr;
		pmemset_memcpy(rep->set, rep->data + offset, data.src,
			data.len, data.flags);
	}

	if (ctx->type == PMEMSET_EVENT_FLUSH) {
		struct pmemset_event_flush data = ctx->data.flush;
		size_t offset = (size_t)data.addr - (size_t)rep->offset_ptr;
		pmemset_memcpy(rep->set, rep->data + offset, data.addr,
			data.len, 0);
	}
	return 0;
}

/*
 * log_create_set -- create pmemset structur
 */
static int
log_create_set(struct pmemset **set, void *arg, int replica)
{
	struct pmemset_config *config;

	if (pmemset_config_new(&config)) {
		pmemset_perror("pmemset_config_new");
		return -1;
	}

	pmemset_config_set_required_store_granularity(config,
		PMEM2_GRANULARITY_PAGE);

	if (replica)
		pmemset_config_set_event_callback(config,
			replica_event_callback, arg);
	else
		pmemset_config_set_event_callback(config, log_event_callback,
			arg);

	int ret = pmemset_new(set, config);

	pmemset_config_delete(&config);

	if (ret) {
		pmemset_perror("pmemset_new");
		return 1;
	}

	if (pmemset_set_contiguous_part_coalescing(*set,
			PMEMSET_COALESCING_FULL)) {
		pmemset_perror("pmemset_set_contiguous_part_coalescing");
		pmemset_delete(set);
		return 1;
	}
	return 0;
}

/*
 * log_open -- open the exising log
 */
static struct log *
log_open(const char *dir, const char *replicaDir, const char *name)
{
	struct log *log = malloc(sizeof(*log));
	if (log == NULL) {
		perror("malloc failed");
		goto exit;
	}

	log->name = strdup(name);
	log->create_new_parts = 0;
	log->dir = strdup(dir);
	log->rep.dir = replicaDir;
	log->rep.name = log->name;
	log->rep.len = 0;
	log->rep.data = NULL;
	log->set = NULL;
	log->rep.set = NULL;

	if (log_create_set(&log->set, log, false))
		goto err_malloc;

	if (log_create_set(&log->rep.set, &log->rep, true))
		goto err_malloc;

	struct pmemset_part_descriptor desc;
	int ret = part_create(log->set, dir, name, false, 0, &log->sds, &desc);
	if (ret)
		goto err_malloc;

	log->log = desc.addr;
	log->len = desc.size - offsetof(struct log_part_pmem, data);
	log->partsNum++;
	log->rep.offset_ptr = (char *)log->log;

	ret = part_create(log->rep.set, replicaDir, name, false, 0,
		&log->rep.sds, &desc);
	if (ret)
		goto err_malloc;

	log->rep.data = desc.addr;
	log->rep.len = desc.size;
	for (int i = 1; ; i++) {
		ret = part_create(log->set, dir, name, false, i, &log->sds,
			&desc);
		if (ret == 1)
			break;

		if (ret != 0)
			goto err_malloc;

		log->len += desc.size;
		log->partsNum++;
	}
	for (unsigned i = 1; i <= log->partsNum; i++) {
		ret = part_create(log->rep.set, dir, name, false, i, &log->sds,
			&desc);
		if (ret == 1)
			break;

		if (ret != 0)
			goto err_malloc;

		log->rep.len += desc.size;
	}
	memcpy(&log->hdr, &log->log->hdr, sizeof(struct header));

	log->create_new_parts = 1;
	return log;

err_malloc:
	free(log);
	pmemset_delete(&log->set);
	pmemset_delete(&log->rep.set);
exit:
	return NULL;
}

/*
 * log_new -- create new log
 */
static struct log *
log_new(const char *dir, const char *replicaDir, const char *name)
{
	struct log *log = malloc(sizeof(*log));
	if (log == NULL) {
		perror("malloc failed");
		return NULL;
	}

	log->name = strdup(name);
	log->dir = strdup(dir);
	log->rep.dir = replicaDir;
	log->rep.name = log->name;
	log->rep.len = 0;
	log->rep.data = NULL;
	memset(&log->sds, 0, sizeof(log->sds));

	log_create_set(&log->set, log, false);
	log_create_set(&log->rep.set, &log->rep, true);
	log->create_new_parts = 1;

	struct pmemset_part_descriptor desc;
	int ret = part_create(log->set, dir, name, true, 0, &log->sds, &desc);
	if (ret) {
		free(log);
		return NULL;
	}

	log->log = desc.addr;
	log->len = desc.size;
	log->hdr.last = 0;
	log->rep.offset_ptr = (char *)log->log;
	log_update_hdr(log->set, log);

	log->partsNum++;

	return log;
}

/*
 * log_extend -- add a new part to extend log
 */
static int
log_extend(struct log *log)
{
	struct pmemset_part_descriptor desc;
	int ret = part_create(log->set, log->dir, log->name,
		true, log->partsNum, &log->sds, &desc);

	if (ret)
		return -1;

	log->partsNum++;
	log->len += desc.size;
	return 0;
}

/*
 * log_add -- add a new entry to the log
 */
static int
log_add(struct log *log, void *data, size_t len)
{
	struct pmemset *set = log->set;

	size_t entry_size = len + sizeof(struct entry);
	while (log->len < log->hdr.last + entry_size) {
		if (log_extend(log))
			return 1;
	}

	struct entry *e = (struct entry *)(log->log->data + log->hdr.last);

	e->len = len;
	pmemset_flush(set, e, sizeof(*e));
	pmemset_memcpy(set, &e->data, data, len, PMEMSET_F_MEM_NONTEMPORAL);

	log->hdr.last += entry_size;
	log_update_hdr(set, log);
	return 0;
}

/*
 * log_print -- iterate over the log and print each entry
 */
static void
log_print(struct log *log)
{
	size_t it = 0;
	while (it < log->hdr.last) {
		struct entry *e = (struct entry *)(log->log->data + it);
		printf("entry: %s\n", e->data);
		it += e->len + sizeof(*e);
	}
}

/*
 * log_close -- close the log
 */
static void
log_close(struct log *l)
{
	pmemset_delete(&l->set);
	pmemset_delete(&l->rep.set);
	free(l);
}

int
main(int argc, char *argv[])
{
	if (argc != 4) {
		fprintf(stderr, "usage: %s dir replica_dir [c|o]\n", argv[0]);
		return 1;
	}

	struct log *l;
	if (*argv[3] == 'c')
		l = log_new(argv[1], argv[2], "testlog");
	else
		l = log_open(argv[1], argv[2], "testlog");

	if (l == NULL) {
		return 1;
	}

	/* add some data to the log */
	for (int i = 0; i < 3000; i++) {
		if (log_add(l, "123456789", 10)) {
			log_close(l);
			return 1;
		}
	}

	log_print(l);
	log_close(l);
	return 0;
}
