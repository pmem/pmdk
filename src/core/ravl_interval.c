// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ravl_interval.c -- ravl_interval implementation
 */

#include "alloc.h"
#include "ravl_interval.h"
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
	const struct ravl_interval_node *left = lhs;
	const struct ravl_interval_node *right = rhs;

	if (left->get_max(left->addr) <= right->get_min(right->addr))
		return -1;
	if (left->get_min(left->addr) >= right->get_max(right->addr))
		return 1;
	return 0;
}

/*
 * ravl_interval_delete - finalize the ravl interval module
 */
void
ravl_interval_delete(struct ravl_interval *ri)
{
	ravl_delete(ri->tree);
	ri->tree = NULL;
	Free(ri);
}

/*
 * ravl_interval_new -- initialize the ravl interval module
 */
struct ravl_interval *
ravl_interval_new(ravl_interval_min *get_min, ravl_interval_max *get_max)
{
	struct ravl_interval *interval = Malloc(sizeof(*interval));
	if (!interval)
		return NULL;

	interval->tree = ravl_new_sized(ravl_interval_compare,
			sizeof(struct ravl_interval_node));
	if (!(interval->tree))
		goto free_alloc;

	interval->get_min = get_min;
	interval->get_max = get_max;

	return interval;

free_alloc:
	Free(interval);
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

	int ret = ravl_emplace_copy(ri->tree, &rin);

	if (ret && errno)
		return -errno;

	return ret;
}

/*
 * ravl_interval_remove -- remove interval entry from the tree
 */
int
ravl_interval_remove(struct ravl_interval *ri, struct ravl_interval_node *rin)
{
	struct ravl_node *node = ravl_find(ri->tree, rin,
			RAVL_PREDICATE_EQUAL);
	if (!node)
		return -ENOENT;

	ravl_remove(ri->tree, node);

	return 0;
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
 * ravl_interval_find_equal -- find the interval with exact (min, max) range
 */
struct ravl_interval_node *
ravl_interval_find_equal(struct ravl_interval *ri, void *addr)
{
	struct ravl_interval_node range;
	range.addr = addr;
	range.get_min = ri->get_min;
	range.get_max = ri->get_max;

	struct ravl_node *node;
	node = ravl_find(ri->tree, &range, RAVL_PREDICATE_EQUAL);
	if (!node)
		return NULL;

	return ravl_data(node);
}

/*
 * ravl_interval_find -- find the earliest interval within (min, max) range
 */
struct ravl_interval_node *
ravl_interval_find(struct ravl_interval *ri, void *addr)
{
	struct ravl_interval_node range;
	range.addr = addr;
	range.get_min = ri->get_min;
	range.get_max = ri->get_max;

	struct ravl_interval_node *cur;
	cur = ravl_interval_find_prior_or_eq(ri->tree, &range);
	if (!cur)
		cur = ravl_interval_find_later(ri->tree, &range);

	return cur;
}

/*
 * ravl_interval_find_closest_prior -- find the closest interval
 *                                     neighbor prior to the current one
 */
struct ravl_interval_node *
ravl_interval_find_closest_prior(struct ravl_interval *ri, void *addr)
{
	struct ravl_interval_node range;
	range.addr = addr;
	range.get_min = ri->get_min;
	range.get_max = ri->get_max;

	struct ravl_node *node;
	node = ravl_find(ri->tree, &range, RAVL_PREDICATE_LESS);
	if (!node)
		return NULL;

	return ravl_data(node);
}

/*
 * ravl_interval_find_closest_later -- find the closest interval neighbor
 *                                     that occurs after the current one
 */
struct ravl_interval_node *
ravl_interval_find_closest_later(struct ravl_interval *ri, void *addr)
{
	struct ravl_interval_node range;
	range.addr = addr;
	range.get_min = ri->get_min;
	range.get_max = ri->get_max;

	struct ravl_node *node;
	node = ravl_find(ri->tree, &range, RAVL_PREDICATE_GREATER);
	if (!node)
		return NULL;

	return ravl_data(node);
}

/*
 * ravl_interval_data -- returns the data contained within an interval node
 */
void *
ravl_interval_data(struct ravl_interval_node *rin)
{
	return (void *)rin->addr;
}

/*
 * ravl_interval_first -- returns first interval in the tree
 */
struct ravl_interval_node *
ravl_interval_find_first(struct ravl_interval *ri)
{
	struct ravl_node *first;
	first = ravl_first(ri->tree);
	if (first)
		return ravl_data(first);

	return NULL;
}

/*
 * ravl_interval_find_next -- returns interval successing the one provided
 */
struct ravl_interval_node *
ravl_interval_find_next(struct ravl_interval *ri, void *addr)
{
	struct ravl_interval_node range;
	range.addr = addr;
	range.get_min = ri->get_min;
	range.get_max = ri->get_max;

	struct ravl_node *next = NULL;
	next = ravl_find(ri->tree, &range, RAVL_PREDICATE_GREATER);
	if (next)
		return ravl_data(next);

	return NULL;
}
