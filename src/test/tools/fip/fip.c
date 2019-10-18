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
 * fip.c -- simple application which helps detecting libfabric providers
 *
 * usage: fip <addr> [<provider>]
 *
 * If no <provider> argument is specified returns 0 if any supported provider
 * from libfabric is available. Otherwise returns 1;
 *
 * If <provider> argument is specified returns 0 if <provider> is supported
 * by libfabric. Otherwise returns 1;
 *
 * On error returns -1.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "rpmem_common.h"
#include "rpmem_fip_common.h"

int
main(int argc, char *argv[])
{
	struct rpmem_fip_probe probe;
	int ret;

	if (argc > 3 || argc < 2) {
		fprintf(stderr, "usage: %s <addr> [<provider>]\n", argv[0]);
		return -1;
	}

	char *addr = argv[1];
	char *prov_str = NULL;
	if (argc == 3)
		prov_str = argv[2];

	struct rpmem_target_info *info;

	info = rpmem_target_parse(addr);
	if (!info) {
		fprintf(stderr, "error: cannot parse address -- '%s'", addr);
		return -1;
	}

	ret = rpmem_fip_probe_get(info->node, &probe);
	if (ret) {
		fprintf(stderr, "error: probing on '%s' failed\n", info->node);
		return -1;
	}

	if (!prov_str) {
		if (!rpmem_fip_probe_any(probe)) {
			printf("no providers found\n");
			ret = 1;
			goto out;
		}

		ret = 0;
		goto out;
	}

	enum rpmem_provider prov = rpmem_provider_from_str(prov_str);
	if (prov == RPMEM_PROV_UNKNOWN) {
		fprintf(stderr, "error: unsupported provider '%s'\n",
				prov_str);
		ret = -1;
		goto out;
	}

	if (!rpmem_fip_probe(probe, prov)) {
		printf("'%s' provider not available at '%s'\n",
				prov_str, info->node);
		ret = 1;
		goto out;
	}

out:
	rpmem_target_free(info);
	return ret;
}
