/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_utils.h -- libpmemset utility functions
 */

#ifndef PMEMSET_UTILS_H
#define PMEMSET_UTILS_H 1

#include <errno.h>

#include "os.h"
#include "out.h"
#include "source.h"

static inline int
pmemset_assert_errno(void)
{
	if (!errno) {
		ERR("pmemset errno is not set");
		ASSERTinfo(0, "pmemset errno is not set");
		return -EINVAL;
	}

	return -errno;
}

#define PMEMSET_E_ERRNO (pmemset_assert_errno())

#ifdef DEBUG
#define PMEMSET_ERR_CLR() \
{\
	errno = 0;\
	char *errormsg = (char *)out_get_errormsg();\
	strcpy(errormsg, "\0");\
}
#else
#define PMEMSET_ERR_CLR()
#endif

void *pmemset_malloc(size_t size, int *err);
void *pmemset_zalloc(size_t size, int *err);
void *pmemset_realloc(void *ptr, size_t size, int *err);

#ifdef _WIN32
int pmemset_lasterror_to_err();
#endif

#endif /* PMEMSET_UTILS_H */
