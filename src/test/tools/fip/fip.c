// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

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
