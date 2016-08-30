/*
 * Copyright 2015-2016, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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

/*
 * libpmemobj_func_mock.c -- implementation for FUNC_MOCK utilities for
 * both Windows and Linux
 */

#include "ctree.h"
Ctree_new_func Ctree_new = ctree_new;
Ctree_delete_func Ctree_delete = ctree_delete;
Ctree_insert_func Ctree_insert = ctree_insert;
Ctree_remove_func Ctree_remove = ctree_remove;

/*
 * set_ctree_funcs -- allow one to override ctree related functions.
 */
void
set_ctree_funcs(struct ctree *(*ctree_new_func)(void),
	void(*ctree_delete_func)(struct ctree *t),
	int(*ctree_insert_func)(
		struct ctree *t, uint64_t key, uint64_t value),
	uint64_t(*ctree_remove_func)(
		struct ctree *t, uint64_t key, int eq))
{
	Ctree_new = (ctree_new_func == NULL) ? ctree_new : ctree_new_func;
	Ctree_delete =
		(ctree_delete_func == NULL) ? ctree_delete : ctree_delete_func;
	Ctree_insert =
		(ctree_insert_func == NULL) ? ctree_insert : ctree_insert_func;
	Ctree_remove =
		(ctree_remove_func == NULL) ? ctree_remove : ctree_remove_func;
}
