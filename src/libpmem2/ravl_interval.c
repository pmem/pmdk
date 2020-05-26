// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ravl_interval.c -- ravl_interval implementation
 */

#include "alloc.h"
#include "map.h"
#include "ravl_interval.h"
#include "pmem2_utils.h"
#include "sys_util.h"
#include "os_thread.h"
#include "ravl.h"

/*
 * ravl_interval - structure representing two points
 *                 on the number line
 */
struct ravl_interval {
	struct ravl *tree;
	ravl_interval_min *get_min;
	ravl_interval_max *get_max;
};

/*
 * ravl_interval_node - structure holding min, max functions and address
 */
struct ravl_interval_node {
	void *addr;
	ravl_interval_min *get_min;
	ravl_interval_max *get_max;
};

/*
 * ravl_interval_compare -- compare intervals by its boundaries,
 *                          no overlapping allowed
 */
static int
ravl_interval_compare(const void *lhs, const void *rhs)
{
	const struct ravl_interval_node *cur = (struct ravl_interval_node *)lhs;
	const struct ravl_interval_node *nxt = (struct ravl_interval_node *)rhs;

	if (cur->get_min(cur->addr) < nxt->get_min(nxt->addr) &&
			cur->get_max(cur->addr) < nxt->get_min(nxt->addr))
		return -1;
	if (cur->get_min(cur->addr) > nxt->get_min(nxt->addr) &&
			cur->get_max(cur->addr) > nxt->get_min(nxt->addr))
		return 1;
	return 0;
}

/*
 * ravl_interval_delete - finalize the ravl interval module
 */
void
ravl_interval_delete(struct ravl_interval **ri)
{
	ravl_delete((*ri)->tree);
	(*ri)->tree = NULL;
	Free(*ri);
	(*ri) = NULL;
}

/*
 * ravl_interval_new -- initialize the ravl interval module
 */
struct ravl_interval *
ravl_interval_new(ravl_interval_min *get_min, ravl_interval_max *get_max)
{
	int ret;
	struct ravl_interval *interval = pmem2_malloc(sizeof(*interval), &ret);
	if (ret)
		goto ret_null;

	interval->tree = ravl_new_sized(ravl_interval_compare,
			sizeof(struct ravl_interval_node));
	if (!(interval->tree))
		goto free_alloc;

	interval->get_min = get_min;
	interval->get_max = get_max;

	return interval;

free_alloc:
	Free(interval);
ret_null:
	return NULL;
}

/*
 * ravl_interval_insert -- insert interval entry into the tree
 */
int
ravl_interval_insert(struct ravl_interval *ri, void *addr)
{
	struct ravl_interval_node rin;
	rin.addr = addr;
	rin.get_min = ri->get_min;
	rin.get_max = ri->get_max;

	if (ravl_emplace_copy(ri->tree, &rin))
		return PMEM2_E_ERRNO;

	return 0;
}

/*
 * ravl_interval_remove -- remove interval entry from the tree
 */
int
ravl_interval_remove(struct ravl_interval *ri, void *addr)
{
	int ret = 0;

	struct ravl_interval_node rin;
	rin.addr = addr;
	rin.get_min = ri->get_min;
	rin.get_max = ri->get_max;

	struct ravl_node *node = ravl_find(ri->tree, &rin,
			RAVL_PREDICATE_EQUAL);
	if (node)
		ravl_remove(ri->tree, node);
	else
		ret = PMEM2_E_MAPPING_NOT_FOUND;

	return ret;
}

/*
 * ravl_interval_find_prior_or_eq -- find overlapping interval starting prior to
 *                                   the current one or at the same place
 */
static struct ravl_interval_node *
ravl_interval_find_prior_or_eq(struct ravl *tree,
		struct ravl_interval_node *rin)
{
	struct ravl_node *node;
	struct ravl_interval_node *cur;

	node = ravl_find(tree, rin, RAVL_PREDICATE_LESS_EQUAL);
	if (!node)
		return NULL;

	cur = ravl_data(node);
	/*
	 * If the end of the found interval is below the searched boundary, then
	 * this is not our interval.
	 */
	if (cur->get_max(cur->addr) <= rin->get_min(rin->addr))
		return NULL;

	return cur;
}

/*
 * ravl_interval_find_later -- find overlapping interval starting later than
 *                             the current one
 */
static struct ravl_interval_node *
ravl_interval_find_later(struct ravl *tree, struct ravl_interval_node *rin)
{
	struct ravl_node *node;
	struct ravl_interval_node *cur;

	node = ravl_find(tree, rin, RAVL_PREDICATE_GREATER);
	if (!node)
		return NULL;

	cur = ravl_data(node);

	/*
	 * If the beginning of the found interval is above the end of
	 * the searched range, then this is not our interval.
	 */
	if (cur->get_min(cur->addr) >= rin->get_max(rin->addr))
		return NULL;

	return cur;
}

/*
 * ravl_interval_find -- find the earliest interval within (min, max) range
 */
struct ravl_interval_node *
ravl_interval_find(struct ravl_interval *ri, void *addr)
{
	struct ravl_interval_node *cur;

	struct ravl_interval_node range;
	range.addr = addr;
	range.get_min = ri->get_min;
	range.get_max = ri->get_max;

	cur = ravl_interval_find_prior_or_eq(ri->tree, &range);
	if (!cur)
		cur = ravl_interval_find_later(ri->tree, &range);

	return cur;
}

/*
 * ravl_interval_data -- returns the data contained within interval node
 */
void *
ravl_interval_data(struct ravl_interval_node *rin)
{
	return (void *)rin->addr;
}
