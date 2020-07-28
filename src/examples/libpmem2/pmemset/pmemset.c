// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#include "pmemset.h"

struct pmemset {

};

struct pmemset_config {

};

struct pmemset_source {

};

struct pmemset_part_descriptor {

};

struct pmemset_part {

};

int
pmemset_config_new(struct pmemset_config **config)
{
	return 0;
}

void
pmemset_config_set_create_if_none(struct pmemset_config *config,
	int value)
{
}

void
pmemset_config_set_create_if_invalid(struct pmemset_config *config,
	int value)
{
}

void
pmemset_config_set_event_callback(struct pmemset_config *config,
	pmemset_event_callback *callback, void *arg)
{
}

void
pmemset_config_set_reservation(struct pmemset_config *config,
	struct pmem2_vm_reservation *rsv)
{
}

void
pmemset_config_set_contiguous_part_coalescing(
	struct pmemset_config *config, int value)
{
}

void
pmemset_config_set_layout_name(struct pmemset_config *config,
	const char *layout)
{
}

void
pmemset_config_set_version(struct pmemset_config *config,
	int major, int minor)
{
}

void
pmemset_config_delete(struct pmemset_config **config)
{
}

int
pmemset_source_from_external(struct pmemset_source **source,
	struct pmem2_source *ext_source)
{
	return 0;
}

#ifdef WIN32
void
pmemset_source_from_wfile(struct pmemset_source **source,
	const wchar *file)
{

}

void
pmemset_source_from_wfile_params(struct pmemset_source **source,
	const wchar *file)
{

}

#endif

int
pmemset_source_from_file(struct pmemset_source **source,
	const char *file)
{
	return 0;
}

int
pmemset_source_from_temporary(struct pmemset_source **source,
	const char *dir)
{
	return 0;
}

int
pmemset_source_fallocate(struct pmemset_source *source, int flags,
	size_t offset, size_t size)
{
	return 0;
}

void
pmemset_source_delete(struct pmemset_source **source)
{

}

int
pmemset_new(struct pmemset **set, struct pmemset_config *config)
{
	return 0;
}

void
pmemset_header_init(struct pmemset_header *header,
	const char *layout, int major, int minor)
{

}

int
pmemset_part_descriptor_new(struct pmemset_part_descriptor **part,
	struct pmemset *set,
	struct pmemset_source *source,
	size_t offset, size_t length)
{
	return 0;
}

ssize_t
pmemset_part_descriptor_pread_mcsafe(
	struct pmemset_part_descriptor *part,
	void *dst, size_t size, size_t offset)
{
	return 0;
}

ssize_t
pmemset_part_descriptor_pwrite_mcsafe(
	struct pmemset_part_descriptor *part,
	void *dst, size_t size, size_t offset)
{
	return 0;
}

int
pmemset_part_finalize(struct pmemset *set,
	struct pmemset_part_descriptor *descriptor,
	const struct pmemset_header *header_in,
	struct pmemset_header *header_out,
	const struct pmemset_part_shutdown_state_data *sds_in,
	struct pmemset_part_shutdown_state_data *sds_out,
	enum pmemset_part_state *state)
{
	return 0;
}

void
pmemset_remove_part(struct pmemset *set, struct pmemset_part **part)
{
}

int
pmemset_remove_range(struct pmemset *set, void *addr, size_t length)
{
	return 0;
}

void *
pmemset_part_address(struct pmemset_part *part)
{
	return NULL;
}

size_t
pmemset_part_length(struct pmemset_part *part)
{
	return 0;
}

void
pmemset_part_first(struct pmemset *set, struct pmemset_part **part)
{
}

void
pmemset_part_next(struct pmemset *set, struct pmemset_part **part)
{
}

int
pmemset_part_by_address(struct pmemset *set, struct pmemset_part **part,
	void *addr)
{
	return 0;
}

void
pmemset_part_drop(struct pmemset_part *part)
{

}

void
pmemset_persist(struct pmemset *set, const void *ptr, size_t size)
{
}

void
pmemset_flush(struct pmemset *set, const void *ptr, size_t size)
{
}

void
pmemset_drain(struct pmemset *set)
{
}

void *
pmemset_memmove(struct pmemset *set, void *pmemdest,
	const void *src, size_t len, unsigned flags)
{
	return NULL;
}

void *pmemset_memcpy(struct pmemset *set, void *pmemdest,
	const void *src, size_t len, unsigned flags)
{
	return NULL;
}

void *
pmemset_memset(struct pmemset *set, void *pmemdest,
	int c, size_t len, unsigned flags)
{
	return NULL;
}

int
pmemset_deep_flush(struct pmemset *set, void *ptr, size_t size)
{
	return 0;
}

void
pmemset_delete(struct pmemset **set)
{

}
