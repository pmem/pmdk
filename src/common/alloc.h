/*
 * Copyright 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
