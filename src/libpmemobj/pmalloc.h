/*
 * Copyright 2015-2016, Intel Corporation
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
 * pmalloc.h -- internal definitions for persistent malloc
 */

typedef int (*pmalloc_constr)(PMEMobjpool *pop, void *ptr,
		size_t usable_size, void *arg);

int heap_boot(PMEMobjpool *pop);
int heap_init(PMEMobjpool *pop);
void heap_vg_open(PMEMobjpool *pop);
void heap_cleanup(PMEMobjpool *pop);
int heap_check(PMEMobjpool *pop);

int pmalloc(PMEMobjpool *pop, uint64_t *off, size_t size);
int pmalloc_construct(PMEMobjpool *pop, uint64_t *off, size_t size,
	pmalloc_constr constructor, void *arg);
int
palloc_operation(PMEMobjpool *pop,
	uint64_t off, uint64_t *dest_off, size_t size,
	pmalloc_constr constructor,
	void *arg, struct operation_entry *entries, size_t nentries);


int prealloc(PMEMobjpool *pop, uint64_t *off, size_t size);
int prealloc_construct(PMEMobjpool *pop, uint64_t *off, size_t size,
	pmalloc_constr constructor, void *arg);

uint64_t pmalloc_first(PMEMobjpool *pop);
uint64_t pmalloc_next(PMEMobjpool *pop, uint64_t off);

size_t pmalloc_usable_size(PMEMobjpool *pop, uint64_t off);
void pfree(PMEMobjpool *pop, uint64_t *off);
