// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

#include <limits.h>
#include <inttypes.h>
#include <errno.h>

#include "call_all.h"
#include "unittest.h"

static char *_s = PATH;
static char _8s[8] = "Posuere";
static unsigned _u = UINT_MAX;
static unsigned long _lu = ULONG_MAX;
static int _d = INT_MAX;
static long _ld = LONG_MAX;
static size_t _zu = SIZE_MAX;
static void *_p = (void *)UINT64_MAX;
static uintmax_t _ju = UINTMAX_MAX;

/*
 * Prevent abort() from CORE_LOG_FATAL()
 */
#define abort() do {; } while (0)

#include "call_all.c.generated"
