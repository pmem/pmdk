// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * mover.c -- default pmem2 data mover
 */

#include "libpmem2.h"
#include "mover.h"
#include "map.h"
#include "util.h"
#include <stdlib.h>
#include <unistd.h>

static void
mover_memcpy(void *arg, struct future_context *context)
{
	struct vdm_memcpy_data *data = future_context_get_data(context);
	struct vdm_memcpy_output *output = future_context_get_output(context);
	struct vdm_descriptor *dsc = (struct vdm_descriptor *)arg;
	struct pmem2_map *map = dsc->vdm_data;

	output->dest = pmem2_get_memcpy_fn(map)(data->dest, data->src,
		data->n, PMEM2_F_MEM_NONTEMPORAL);
	data->vdm_cb(context);
}

struct vdm *
mover_new(struct pmem2_map *map)
{
	struct vdm_descriptor *desc = malloc(sizeof(*desc));

	if (desc == NULL)
		return NULL;

	desc->vdm_data = map;
	desc->memcpy = mover_memcpy;

	struct vdm *vdm = vdm_new(desc);

	if (vdm == NULL) {
		free(desc);
		return NULL;
	}
	return vdm;
}

struct vdm_memcpy_future pmem2_memcpy_async(struct pmem2_map *map,
	void *pmemdest,	const void *src, size_t len, unsigned flags)
{
	SUPPRESS_UNUSED(flags);
	return vdm_memcpy(map->vdm, pmemdest, (void *)src, len);
}

void
mover_delete(struct vdm *vdm)
{
/*	free(vdm->vdm_data); */
	free(vdm);
}
