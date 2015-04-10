/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * list.h -- internal definitions for persistent atomic lists module
 */

struct list_entry {
	PMEMoid pe_next;
	PMEMoid pe_prev;
};

struct list_head {
	PMEMoid pe_first;
	PMEMmutex lock;
};

/*
 * XXX move to other file, change size etc.
 */
struct oob_header {
	struct list_entry oob;
	uint16_t internal_type;
	uint16_t user_type;
	uint8_t padding[12];
};

PMEMoid list_insert_new(PMEMobjpool *pop, struct list_head *oob_head,
	size_t pe_offset, struct list_head *head, PMEMoid dest, int before,
	size_t size, void (*constructor)(void *ptr, void *arg), void *arg);

int list_realloc(PMEMobjpool *pop, struct list_head *oob_head,
	size_t pe_offset, struct list_head *head,
	size_t size, void (*constructor)(void *ptr, void *arg), void *arg,
	uint64_t field_offset, uint64_t field_value,
	PMEMoid *oid);

int list_realloc_move(PMEMobjpool *pop, struct list_head *oob_head_old,
	struct list_head *oob_head_new, size_t pe_offset,
	struct list_head *head, size_t size,
	void (*constructor)(void *ptr, void *arg), void *arg,
	uint64_t field_offset, uint64_t field_value,
	PMEMoid *oid);

int list_insert(PMEMobjpool *pop,
	size_t pe_offset, struct list_head *head, PMEMoid dest, int before,
	PMEMoid oid);

int list_remove_free(PMEMobjpool *pop, struct list_head *oob_head,
	size_t pe_offset, struct list_head *head,
	PMEMoid oid);

int list_remove(PMEMobjpool *pop,
	size_t pe_offset, struct list_head *head,
	PMEMoid oid);

int list_move(PMEMobjpool *pop,
	size_t pe_offset_old, struct list_head *head_old,
	size_t pe_offset_new, struct list_head *head_new,
	PMEMoid dest, int before, PMEMoid oid);

int list_move_oob(PMEMobjpool *pop,
	struct list_head *head_old, struct list_head *head_new,
	PMEMoid oid);
