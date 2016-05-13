/*
 * Copyright 2016, Intel Corporation
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
 * memops.h -- aggregated memory operations helper definitions
 */

enum operation_type {
	OPERATION_SET,
	OPERATION_AND,
	OPERATION_OR,

	MAX_OPERATION_TYPE
};

struct operation_entry {
	uint64_t *ptr;
	uint64_t value;
	enum operation_type type;
};

#define MAX_TRANSIENT_ENTRIES 10
#define MAX_PERSITENT_ENTRIES 10

enum operation_entry_type {
	ENTRY_PERSISTENT,
	ENTRY_TRANSIENT,

	MAX_OPERATION_ENTRY_TYPE
};

/*
 * operation_context -- context of an ongoing palloc operation
 */
struct operation_context {
	PMEMobjpool *pop;
	struct redo_log *redo;

	size_t nentries[MAX_OPERATION_ENTRY_TYPE];
	struct operation_entry
		entries[MAX_OPERATION_ENTRY_TYPE][MAX_PERSITENT_ENTRIES];
};

void operation_init(PMEMobjpool *pop, struct operation_context *ctx,
	struct redo_log *redo);
void operation_add_entry(struct operation_context *ctx,
	void *ptr, uint64_t value, enum operation_type type);
void operation_add_typed_entry(struct operation_context *ctx,
	void *ptr, uint64_t value,
	enum operation_type type, enum operation_entry_type en_type);
void operation_add_entries(struct operation_context *ctx,
	struct operation_entry *entries, size_t nentries);
void operation_process(struct operation_context *ctx);
