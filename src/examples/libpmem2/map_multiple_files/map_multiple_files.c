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
 * file_dsc - a structure that keeps information about file mapping
 */
struct file_dsc {
	int fd;
	size_t size;
	struct pmem2_source *src;
	struct pmem2_map *map;
};

/*
 * file_dsc_init - initialize file_dsc structure values
 */
static int
file_dsc_init(struct file_dsc *fdsc, char *path)
{
	if ((fdsc->fd = open(path, O_RDWR)) < 0) {
		perror("open");
		goto fail;
	}

	if (pmem2_source_from_fd(&fdsc->src, fdsc->fd)) {
		pmem2_perror("pmem2_source_from_fd");
		goto file_close;
	}

	if (pmem2_source_size(fdsc->src, &fdsc->size)) {
		pmem2_perror("pmem2_source_size");
		goto source_delete;
	}

	return 0;

source_delete:
	pmem2_source_delete(&fdsc->src);
file_close:
	close(fdsc->fd);
fail:
	return 1;
}

/*
 * file_dsc_fini - deinitialize file_dsc structure values
 */
static void
file_dsc_fini(struct file_dsc *fdsc)
{
	close(fdsc->fd);
	pmem2_source_delete(&fdsc->src);
}

/*
 * file_check_align - check if file is aligned
 */
static int
file_check_align(struct file_dsc *fdsc)
{
	size_t alignment;
	if (pmem2_source_alignment(fdsc->src, &alignment)) {
		pmem2_perror("pmem2_source_alignment");
		return 1;
	}

	if (fdsc->size % alignment != 0) {
		fprintf(stderr,
			"usage: files must be aligned to %zu bytes\n",
			alignment);
		return 1;
	}

	return 0;
}

/*
 * files_check_same_align - check if files have the same alignment
 */
static int
files_check_same_align(struct file_dsc *fdsc, int nfiles)
{
	size_t alignment, nalignment;

	if (pmem2_source_alignment(fdsc[0].src, &alignment)) {
		pmem2_perror("pmem2_source_alignment");
		return 1;
	}

	for (int n = 1; n < nfiles; n++) {
		if (pmem2_source_alignment(fdsc[n].src, &nalignment)) {
			pmem2_perror("pmem2_source_alignment");
			return 1;
		}

		if (alignment != nalignment) {
			fprintf(stderr,
				"usage: files must have the same alignment\n");
			return 1;
		}
	}

	return 0;
}

/*
 * files_check_memset - check if mappings retrieve the same memset function
 */
static int
files_check_same_memset(struct file_dsc *fdsc, int nfiles,
		pmem2_memset_fn memset_fn)
{
	for (int n = 0; n < nfiles; n++) {
		if (memset_fn != pmem2_get_memset_fn(fdsc[n].map)) {
			fprintf(stderr,
				"usage: filesystems must be compatible for a side by side mapping\n");
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

	int nfiles = argc - 1;

	struct file_dsc *fdsc = malloc(sizeof(struct file_dsc) * nfiles);
	if (!fdsc) {
		perror("malloc");
		return ret;
	}

	int ninit;
	for (ninit = 0; ninit < nfiles; ninit++) {
		if (file_dsc_init(&fdsc[ninit], argv[ninit + 1])) {
			goto fdsc_fini;
		}
	}

	for (int n = 0; n < nfiles; n++) {
		if (file_check_align(&fdsc[n]))
			goto fdsc_fini;
	}

	if (files_check_same_align(fdsc, nfiles))
		goto fdsc_fini;

	size_t reservation_size = 0;
	for (int n = 0; n < nfiles; n++) {
		reservation_size += fdsc[n].size;
	}

	struct pmem2_vm_reservation *rsv;
	if (!pmem2_vm_reservation_new(&rsv, reservation_size, NULL)) {
		pmem2_perror("pmem2_vm_reservation_new");
		goto fdsc_fini;
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

		offset += fdsc[nmap].size;

		if (pmem2_map(cfg, fdsc[nmap].src, &fdsc[nmap].map)) {
			pmem2_perror("pmem2_map");
			goto unmap;
		}
	}

	char *addr = pmem2_map_get_address(fdsc[0].map);
	if (addr == NULL) {
		pmem2_perror("pmem2_map_get_address");
		goto unmap;
	}

	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(fdsc[0].map);

	if (files_check_same_memset(fdsc, nfiles, memset_fn))
		goto unmap;

	memset_fn(addr, '-', reservation_size, PMEM2_F_MEM_NONTEMPORAL);

	ret = 0;

unmap:
	for (nmap--; nmap >= 0; nmap--) {
		pmem2_unmap(&fdsc[nmap].map);
	}
delete_config:
	pmem2_config_delete(&cfg);
delete_vm_reservation:
	pmem2_vm_reservation_delete(&rsv);
fdsc_fini:
	for (ninit--; ninit >= 0; ninit--)
		file_dsc_fini(&fdsc[ninit]);
	free(fdsc);
	return ret;
}
