/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2018-2020, Intel Corporation */
/*
 * poolset_util.hpp -- this file provides interface for creating
 * poolsets of specified size
 */

#ifndef POOLSET_UTIL_HPP
#define POOLSET_UTIL_HPP

#include <stddef.h>

#define POOLSET_PATH "pool.set"

int dynamic_poolset_create(const char *path, size_t size);

#endif
