/*
 * Copyright 2014-2018, Intel Corporation
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
 * pmemmalloc.c -- simple tool for allocating objects from pmemobj
 *
 * usage: pmemalloc [-r <size>] [-o <size>] [-t <type_num>]
 *			[-c <count>] [-e <num>] <file>
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "libpmemobj.h"
#include "util.h"

#define USAGE()\
printf("usage: pmemalloc"\
	" [-r <size>] [-o <size>] [-t <type_num>]"\
	" [-s] [-f] [-e a|f|s] <file>\n")

int
main(int argc, char *argv[])
{
#ifdef _WIN32
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for (int i = 0; i < argc; i++) {
		argv[i] = util_toUTF8(wargv[i]);
		if (argv[i] == NULL) {
			for (i--; i >= 0; i--)
				free(argv[i]);
			fprintf(stderr, "Error during arguments conversion\n");
			return 1;
		}
	}
#endif
	int opt;
	int tmpi;
	long long tmpl;
	int ret = 0;
	size_t size = 0;
	size_t root_size = 0;
	unsigned type_num = 0;
	char exit_at = '\0';
	int do_set = 0;
	int do_free = 0;
	size_t alloc_class_size = 0;

	if (argc < 2) {
		USAGE();
		ret = 1;
		goto end;
	}

	while ((opt = getopt(argc, argv, "r:o:c:t:e:sf")) != -1) {
		switch (opt) {
		case 'r':
			tmpl = atoll(optarg);
			if (tmpl < 0) {
				USAGE();
				ret = 1;
				goto end;
			}
			root_size = (size_t)tmpl;
			break;
		case 'o':
			tmpl = atoll(optarg);
			if (tmpl < 0) {
				USAGE();
				ret = 1;
				goto end;
			}
			size = (size_t)tmpl;
			break;
		case 'c':
			tmpl = atoll(optarg);
			if (tmpl < 0) {
				USAGE();
				ret = 1;
				goto end;
			}
			alloc_class_size = (size_t)tmpl;
			break;
		case 't':
			tmpi = atoi(optarg);
			if (tmpi < 0) {
				USAGE();
				ret = 1;
				goto end;
			}
			type_num = (unsigned)tmpi;
			break;
		case 'e':
			exit_at = optarg[0];
			break;
		case 's':
			do_set = 1;
			break;
		case 'f':
			do_free = 1;
			break;
		default:
			USAGE();
			ret = 1;
			goto end;
		}
	}

	char *file = argv[optind];

	PMEMobjpool *pop;
	if ((pop = pmemobj_open(file, NULL)) == NULL) {
		fprintf(stderr, "pmemobj_open: %s\n", pmemobj_errormsg());
		ret = 1;
		goto end;
	}

	if (root_size) {
		PMEMoid oid = pmemobj_root(pop, root_size);
		if (OID_IS_NULL(oid)) {
			fprintf(stderr, "pmemobj_root: %s\n",
					pmemobj_errormsg());
			ret = 1;
			goto end;
		}
	}

	if (alloc_class_size) {
		PMEMoid oid;
		struct pobj_alloc_class_desc desc;
		desc.alignment = 0;
		desc.class_id = 0;
		desc.header_type = POBJ_HEADER_COMPACT;
		desc.unit_size = alloc_class_size;
		desc.units_per_block = 1;
		ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc", &desc);
		if (ret != 0)
			goto end;
		ret = pmemobj_xalloc(pop, &oid, 1, type_num,
			POBJ_CLASS_ID(desc.class_id), NULL, NULL);
		if (ret != 0)
			goto end;
	}

	if (size) {
		PMEMoid oid;
		TX_BEGIN(pop) {
			oid = pmemobj_tx_alloc(size, type_num);
			if (exit_at == 'a')
				exit(1);
		} TX_END
		if (OID_IS_NULL(oid)) {
			fprintf(stderr, "pmemobj_tx_alloc: %s\n",
					pmemobj_errormsg());
			ret = 1;
			goto end;
		}

		if (do_set) {
			TX_BEGIN(pop) {
				pmemobj_tx_add_range(oid, 0, size);
				if (exit_at == 's')
					exit(1);
			} TX_END
		}

		if (do_free) {
			TX_BEGIN(pop) {
				pmemobj_tx_free(oid);
				if (exit_at == 'f')
					exit(1);
			} TX_END
		}
	}

	pmemobj_close(pop);

end:
#ifdef _WIN32
	for (int i = argc; i > 0; i--)
		free(argv[i - 1]);
#endif
	return ret;
}
