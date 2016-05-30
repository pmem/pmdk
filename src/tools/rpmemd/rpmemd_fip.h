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
 * rpmemd_fip.h -- rpmemd libfabric provider module header file
 */

#include <stddef.h>

struct rpmemd_fip;

struct rpmemd_fip_attr {
	void *addr;
	size_t size;
	unsigned nlanes;
	size_t nthreads;
	enum rpmem_provider provider;
	enum rpmem_persist_method persist_method;
	void (*persist)(const void *addr, size_t len);
};

struct rpmemd_fip *rpmemd_fip_init(const char *node,
		const char *service,
		struct rpmemd_fip_attr *attr,
		struct rpmem_resp_attr *resp,
		enum rpmem_err *err);
void rpmemd_fip_fini(struct rpmemd_fip *fip);

int rpmemd_fip_accept(struct rpmemd_fip *fip);
int rpmemd_fip_process_start(struct rpmemd_fip *fip);
int rpmemd_fip_process_stop(struct rpmemd_fip *fip);
int rpmemd_fip_wait_close(struct rpmemd_fip *fip, int timeout);
int rpmemd_fip_close(struct rpmemd_fip *fip);
