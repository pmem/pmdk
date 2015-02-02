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
 * backend.h -- internal definitions for backend
 */

enum backend_type {
	BACKEND_NOOP,
	BACKEND_PERSISTENT,
	/* BACKEND_VOLATILE, */
	MAX_BACKEND
};

struct bucket_backend_operations {

};

struct arena_backend_operations {
	/*
	 * set_alloc_ptr
	 *
	 * Set the value to the location referenced by the pointer. Called by
	 * the interface functions to update the location to which the
	 * allocation/free is being made to.
	 */
	void (*set_alloc_ptr)(struct arena *arena,
		uint64_t *ptr, uint64_t value);
};

struct pool_backend_operations {

};

struct backend {
	enum backend_type type;
	struct bucket_backend_operations *b_ops;
	struct arena_backend_operations *a_ops;
	struct pool_backend_operations *p_ops;
};

void backend_open(struct backend *backend, enum backend_type type,
	struct bucket_backend_operations *b_ops,
	struct arena_backend_operations *a_ops,
	struct pool_backend_operations *p_ops);

void backend_close(struct backend *backend);
