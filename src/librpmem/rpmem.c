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
 * rpmem.c -- main source file for librpmem
 */
#include <stdlib.h>

#include "librpmem.h"

/*
 * struct rpmem_pool -- remote pool context
 */
struct rpmem_pool {
	void *pool_addr;
	size_t pool_size;
};

/*
 * rpmem_create -- create remote pool on target node
 *
 * target        -- name of target node in format <target_name>[:<port>]
 * pool_set_name -- remote pool set name
 * pool_addr     -- local pool memory address which will be replicated
 * pool_size     -- required pool size
 * nlanes        -- number of lanes
 * pool_attr     -- pool attributes, received from remote host
 */
RPMEMpool *
rpmem_create(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	const struct rpmem_pool_attr *create_attr)
{
	/* XXX */
	return NULL;
}

/*
 * rpmem_open -- open remote pool on target node
 *
 * target        -- name of target node in format <target_name>[:<port>]
 * pool_set_name -- remote pool set name
 * pool_addr     -- local pool memory address which will be replicated
 * pool_size     -- required pool size
 * nlanes        -- number of lanes
 * pool_attr     -- pool attributes, received from remote host
 */
RPMEMpool *
rpmem_open(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	struct rpmem_pool_attr *open_attr)
{
	/* XXX */
	return NULL;
}

/*
 * rpmem_remove -- remove remote pool on target node
 *
 * target        -- name of target node in format <target_name>[:<port>]
 * pool_set_name -- remote pool set name
 */
int
rpmem_remove(const char *target, const char *pool_set_name)
{
	/* XXX */
	return -1;
}

/*
 * rpmem_close -- close remote pool on target node
 */
int
rpmem_close(RPMEMpool *rpp)
{
	/* XXX */
	return -1;
}

/*
 * rpmem_persist -- persist operation on target node
 *
 * rpp           -- remote pool handle
 * offset        -- offset in pool
 * length        -- length of persist operation
 * lane          -- lane number
 */
int
rpmem_persist(RPMEMpool *rpp, size_t offset, size_t length, unsigned lane)
{
	/* XXX */
	return -1;
}

/*
 * rpmem_read -- read data from remote pool:
 *
 * rpp           -- remote pool handle
 * buff          -- output buffer
 * offset        -- offset in pool
 * length        -- length of read operation
 */
int
rpmem_read(RPMEMpool *rpp, void *buff, size_t offset, size_t length)
{
	/* XXX */
	return -1;
}
