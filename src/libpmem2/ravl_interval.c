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
	ravl_interval_min *get_min;
	ravl_interval_max *get_max;
};

/*
 * ravl_interval_node - structure holding min, max functions and address
 */
struct ravl_interval_node {
	void *addr;
	struct ravl_interval interval;
};

/*
 * ravl_interval_compare -- compare intervals by left boundary
 */
static int
ravl_interval_compare(const void *lhs, const void *rhs)
{
	const struct ravl_interval_node *l = (struct ravl_interval_node *)lhs;
	const struct ravl_interval_node *r = (struct ravl_interval_node *)rhs;

	if (l->interval.get_min(l->addr) < r->interval.get_min(r->addr))
		return -1;
	if (l->interval.get_min(l->addr) > r->interval.get_min(r->addr))
		return 1;
	return 0;
}

/*
 * ravl_interval_delete - finalize the ravl interval module
 */
void
ravl_interval_delete(struct ravl **tree, struct ravl_interval **ri)
{
	ravl_delete(*tree);
	*tree = NULL;
	Free(*ri);
}

/*
 * ravl_interval_new -- initialize the ravl interval module
 */
struct ravl_interval *
ravl_interval_new(struct ravl **tree, ravl_interval_min *get_min,
		ravl_interval_max *get_max)
{
	*tree = ravl_new_sized(ravl_interval_compare,
		sizeof(struct ravl_interval_node));

	if (!(*tree))
		return NULL;

	int ret;
	struct ravl_interval *interval = pmem2_malloc(sizeof(*interval), &ret);
	interval->get_min = get_min;
	interval->get_max = get_max;

	return interval;
}

/*
 * ravl_interval_add -- add interval entry to the tree
 */
int
ravl_interval_insert(struct ravl **tree, struct ravl_interval *ri, void *addr)
{
	int ret;

	struct ravl_interval_node rie;
	rie.interval = *ri;
	rie.addr = addr;

	ret = ravl_emplace_copy(*tree, &rie);

	if (ret)
		return PMEM2_E_ERRNO;

	return 0;
}

/*
 * ravl_interval_remove -- remove interval entry from the tree
 */
int
ravl_interval_remove(struct ravl **tree, struct ravl_interval *ri, void *addr)
{
	int ret = 0;

	struct ravl_interval_node rie;
	rie.interval = *ri;
	rie.addr = addr;

	struct ravl_node *n = ravl_find(*tree, &rie, RAVL_PREDICATE_EQUAL);
	if (n)
		ravl_remove(*tree, n);
	else
		ret = PMEM2_E_MAPPING_NOT_FOUND;

	return ret;
}

/*
 * ravl_interval_find_prior_or_eq -- find overlapping interval starting prior to
 *                                   the current one or at the same place
 */
static void *
ravl_interval_find_prior_or_eq(struct ravl **tree,
		struct ravl_interval_node *rie)
{
	struct ravl_node *n;
	struct ravl_interval_node *cur;

	n = ravl_find(*tree, rie, RAVL_PREDICATE_LESS_EQUAL);
	if (!n)
		return NULL;

	cur = ravl_data(n);
	/*
	 * If the end of the found interval is below the searched boundary, then
	 * this is not our interval.
	 */
	if (cur->interval.get_max(cur->addr) <=
		rie->interval.get_min(rie->addr))
		return NULL;

	return cur->addr;
}

/*
 * ravl_interval_find_later -- find overlapping interval starting later than
 *                             the current one
 */
static void *
ravl_interval_find_later(struct ravl **tree, struct ravl_interval_node *rie)
{
	struct ravl_node *n;
	struct ravl_interval_node *cur;

	n = ravl_find(*tree, rie, RAVL_PREDICATE_LESS_EQUAL);
	if (!n)
		return NULL;

	cur = ravl_data(n);

	/*
	 * If the beginning of the found interval is above the end of
	 * the searched range, then this is not our interval.
	 */
	if (cur->interval.get_min(cur->addr) >=
		rie->interval.get_max(rie->addr))
		return NULL;

	return cur->addr;
}

/*
 * ravl_interval_find -- find the earliest interval with (min, max) range
 */
void *
ravl_interval_find(struct ravl **tree, struct ravl_interval *ri, void *addr)
{
	void *found_addr;

	struct ravl_interval_node rie;
	rie.interval = *ri;
	rie.addr = addr;

	found_addr = ravl_interval_find_prior_or_eq(tree, &rie);
	if (!found_addr)
		found_addr = ravl_interval_find_later(tree, &rie);

	return found_addr;
}
