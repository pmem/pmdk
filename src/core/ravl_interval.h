/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * ravl_interval.h -- internal definitions for ravl_interval
 */

#ifndef RAVL_INTERVAL_H
#define RAVL_INTERVAL_H

#include "os_thread.h"
#include "ravl.h"

struct ravl_interval;
struct ravl_interval_node;

typedef size_t ravl_interval_min(void *addr);
typedef size_t ravl_interval_max(void *addr);

struct ravl_interval *ravl_interval_new(ravl_interval_min *min,
		ravl_interval_min *max);
void ravl_interval_delete(struct ravl_interval *ri);
void ravl_interval_delete_cb(struct ravl_interval *ri, ravl_cb cb, void *arg);
int ravl_interval_insert(struct ravl_interval *ri, void *addr);
int ravl_interval_remove(struct ravl_interval *ri,
		struct ravl_interval_node *rin);
struct ravl_interval_node *ravl_interval_find_equal(struct ravl_interval *ri,
		void *addr);
struct ravl_interval_node *ravl_interval_find(struct ravl_interval *ri,
		void *addr);
struct ravl_interval_node *ravl_interval_find_closest_prior(
	struct ravl_interval *ri, void *addr);
struct ravl_interval_node *ravl_interval_find_closest_later(
		struct ravl_interval *ri, void *addr);
struct ravl_interval_node *ravl_interval_find_first(struct ravl_interval *ri);
struct ravl_interval_node *ravl_interval_find_next(struct ravl_interval *ri,
		void *addr);
void *ravl_interval_data(struct ravl_interval_node *rin);
#endif
