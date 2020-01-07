// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * sys/param.h -- a few useful macros
 */

#ifndef SYS_PARAM_H
#define SYS_PARAM_H 1

#define roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))
#define howmany(x, y)	(((x) + ((y) - 1)) / (y))

#define BPB 8	/* bits per byte */

#define setbit(b, i)	((b)[(i) / BPB] |= 1 << ((i) % BPB))
#define isset(b, i)	((b)[(i) / BPB] & (1 << ((i) % BPB)))
#define isclr(b, i)	(((b)[(i) / BPB] & (1 << ((i) % BPB))) == 0)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#endif /* SYS_PARAM_H */
