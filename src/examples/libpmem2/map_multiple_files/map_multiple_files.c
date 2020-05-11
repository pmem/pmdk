// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * map_multiple_files.c -- implementation of virtual address allocation example
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <libpmem2.h>

/*
 * cleanup_arrays - free dynamically allocated memory
 */
static void
free_arrays(size_t **file_size, int **fd, struct pmem2_map ***map,
		struct pmem2_source ***src)
{
	free(*file_size);
	free(*fd);
	free(*map);
	free(*src);
}

/*
 * init_arrays - initialize arrays
 */
static int
init_arrays(size_t nfiles, size_t **file_size, int **fd,
		struct pmem2_map ***map, struct pmem2_source ***src)
{
	int ret = 0;

	*file_size = malloc(sizeof(size_t) * nfiles);
	*fd = malloc(sizeof(int) * nfiles);
	*map = malloc(sizeof(struct pmem2_map *) * nfiles);
	*src = malloc(sizeof(struct pmem2_source *) * nfiles);

	if (!(*file_size) || !(*fd) || !(*map) || !(*src)) {
		perror("malloc");
		free_arrays(file_size, fd, map, src);
		ret = 1;
	}

	return ret;
}

/*
 * close_fds - close files from file descriptors
 */
static void
close_fds(size_t nfiles, int *fd)
{
	for (int nopen = 0; nopen < nfiles; nopen++) {
		close(fd[nopen]);
	}
}

/*
 * open_fds - open files and save file descriptors
 */
static int
open_fds(size_t nfiles, int *fd, char *argv[])
{
	for (int nopen = 0; nopen < nfiles; nopen++) {
		if ((fd[nopen] = open(argv[nopen + 1], O_RDWR)) < 0) {
			perror("open");
			close_fds(nopen, fd);
			return 1;
		}
	}

	return 0;
}

/*
 * free_sources - free memory allocated in source pointer array
 */
static void
free_sources(size_t nfiles, struct pmem2_source **src)
{
	for (int nsrc = 0; nsrc < nfiles; nsrc++) {
		free(src[nsrc]);
	}
}

/*
 * init_sources - get pmem2_source structs from file descriptors
 */
static int
init_sources(size_t nfiles, int *fd, struct pmem2_source **src)
{
	for (int nsrc = 0; nsrc < nfiles; nsrc++) {
		if (pmem2_source_from_fd(&src[nsrc], fd[nsrc])) {
			pmem2_perror("pmem2_source_from_fd");
			free_sources(nsrc, src);
			return 1;
		}
	}

	return 0;
}

/*
 * get_file_sizes - get file sizes from sources
 */
static int
get_file_sizes(size_t nfiles, size_t *file_size, struct pmem2_source **src)
{
	for (int nsize = 0; nsize < nfiles; nsize++) {
		if (pmem2_source_size(src[nsize], &file_size[nsize])) {
			pmem2_perror("pmem2_source_size");
			return 1;
		}
	}

	return 0;
}

/*
 * get_file_alignments - get file alignments from sources
 */
static int
check_file_alignments(size_t nfiles, size_t *file_size,
		struct pmem2_source **src)
{
	size_t alignment;

	for (int nalign = 0; nalign < nfiles; nalign++) {
		pmem2_source_alignment(src[nalign], &alignment);

		if (file_size[nalign] % alignment != 0) {
			fprintf(stderr,
				"usage: files must be aligned\n");
			return 1;
		}
	}

	return 0;
}

/*
 * unmap_files - unmap mapped files
 */
static void
unmap_files(size_t nfiles, struct pmem2_map **map)
{
	for (int nmap = 0; nmap < nfiles; nmap++) {
		pmem2_unmap(&map[nmap]);
	}
}

/*
 * check_files_fs - check files filesystems
 */
static int
check_files_fs(size_t nfiles, struct pmem2_map **map)
{
	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map[0]);
	for (int nmemset = 1; nmemset < nfiles; nmemset++) {
		if (memset_fn != pmem2_get_memset_fn(map[nmemset])) {
			fprintf(stderr,
				"usage: files have different file systems\n");
			return 1;
		}
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int ret = 1;

	if (argc < 2) {
		fprintf(stderr,
			"usage: ./map_multiple_files <file1> <file2> ...\n");
		return ret;
	}

	size_t nfiles = argc - 1;

	size_t *file_size = NULL;
	int *fd = NULL;
	struct pmem2_map **map = NULL;
	struct pmem2_source **src = NULL;

	if (init_arrays(nfiles, &file_size, &fd, &map, &src))
		return ret;

	if (open_fds(nfiles, fd, argv))
		goto free_arrays;

	if (init_sources(nfiles, fd, src))
		goto close_fds;

	if (get_file_sizes(nfiles, file_size, src))
		goto free_sources;

	size_t reservation_size = 0;
	for (int nsize = 0; nsize < nfiles; nsize++) {
		reservation_size += file_size[nsize];
	}

	if (check_file_alignments(nfiles, file_size, src))
		goto free_sources;

	struct pmem2_vm_reservation *rsv;
	if (pmem2_vm_reservation_new(&rsv, reservation_size, NULL)) {
		pmem2_perror("pmem2_vm_reservation_new");
		goto free_sources;
	}

	struct pmem2_config *cfg;
	if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		goto delete_vm_reservation;
	}

	if (pmem2_config_set_required_store_granularity(
			cfg, PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		goto delete_config;
	}

	size_t offset = 0;

	int nmap;
	for (nmap = 0; nmap < nfiles; nmap++) {
		if (pmem2_config_set_vm_reservation(
				cfg, rsv, offset) != PMEM2_E_NOSUPP) {
			pmem2_perror("pmem2_config_set_vm_reservation");
			goto unmap;
		}

		offset += file_size[nmap];

		if (pmem2_map(cfg, src[nmap], &map[nmap])) {
			pmem2_perror("pmem2_map");
			goto unmap;
		}
	}

	if (check_files_fs(nfiles, map))
		goto unmap;

	char *addr = pmem2_map_get_address(map[0]);
	if (addr == NULL) {
		pmem2_perror("pmem2_map_get_address");
		goto unmap;
	}

	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map[0]);
	memset_fn(addr, '-', reservation_size, PMEM2_F_MEM_NONTEMPORAL);

	ret = 0;

unmap:
	unmap_files(nmap, map);
delete_config:
	pmem2_config_delete(&cfg);
delete_vm_reservation:
	pmem2_vm_reservation_delete(&rsv);
free_sources:
	free_sources(nfiles, src);
close_fds:
	close_fds(nfiles, fd);
free_arrays:
	free_arrays(&file_size, &fd, &map, &src);
	return ret;
}
