/*
 * Copyright 2016-2019, Intel Corporation
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
 * rpmem_fip.h -- rpmem libfabric provider module header file
 */

#ifndef RPMEM_FIP_H
#define RPMEM_FIP_H

#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rpmem_fip;

struct rpmem_fip_attr {
	enum rpmem_provider provider;
	size_t max_wq_size;
	enum rpmem_persist_method persist_method;
	void *laddr;
	size_t size;
	size_t buff_size;
	unsigned nlanes;
	void *raddr;
	uint64_t rkey;
};

struct rpmem_fip *rpmem_fip_init(const char *node, const char *service,
		struct rpmem_fip_attr *attr, unsigned *nlanes);
void rpmem_fip_fini(struct rpmem_fip *fip);

int rpmem_fip_connect(struct rpmem_fip *fip);
int rpmem_fip_close(struct rpmem_fip *fip);
int rpmem_fip_process_start(struct rpmem_fip *fip);
int rpmem_fip_process_stop(struct rpmem_fip *fip);

int rpmem_fip_flush(struct rpmem_fip *fip, size_t offset, size_t len,
		unsigned lane, unsigned flags);

int rpmem_fip_drain(struct rpmem_fip *fip, unsigned lane);

int rpmem_fip_persist(struct rpmem_fip *fip, size_t offset, size_t len,
		unsigned lane, unsigned flags);

int rpmem_fip_read(struct rpmem_fip *fip, void *buff,
		size_t len, size_t off, unsigned lane);
void rpmem_fip_probe_fork_safety(void);

size_t rpmem_fip_get_wq_size(struct rpmem_fip *fip);

#ifdef __cplusplus
}
#endif

#endif
