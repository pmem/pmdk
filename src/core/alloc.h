// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#ifndef COMMON_ALLOC_H
#define COMMON_ALLOC_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*Malloc_func)(size_t size);
typedef void *(*Realloc_func)(void *ptr, size_t size);

extern Malloc_func fn_malloc;
extern Realloc_func fn_realloc;

#if FAULT_INJECTION
void *_flt_Malloc(size_t, const char *);
void *_flt_Realloc(void *, size_t, const char *);

#define Malloc(size) _flt_Malloc(size, __func__)
#define Realloc(ptr, size) _flt_Realloc(ptr, size, __func__)
#else
void *_Malloc(size_t);
void *_Realloc(void *, size_t);

#define Malloc(size) _Malloc(size)
#define Realloc(ptr, size) _Realloc(ptr, size)
#endif

void set_func_malloc(void *(*malloc_func)(size_t size));
void set_func_realloc(void *(*realloc_func)(void *ptr, size_t size));

/*
 * overridable names for malloc & friends used by this library
 */
typedef void (*Free_func)(void *ptr);
typedef char *(*Strdup_func)(const char *s);

extern Free_func Free;
extern Strdup_func Strdup;
extern void *Zalloc(size_t sz);

#ifdef __cplusplus
}
#endif
#endif
