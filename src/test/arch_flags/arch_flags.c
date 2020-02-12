// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * arch_flags.c -- unit test for architecture flags
 */
#include <inttypes.h>
#include <string.h>

#include "unittest.h"
#include "pool_hdr.h"
#include "pmemcommon.h"

#define FATAL_USAGE()\
UT_FATAL(\
	"usage: arch_flags <machine>:<machine_class>:<data>:<alignment_desc>:<reserved>")
#define ARCH_FLAGS_LOG_PREFIX "arch_flags"
#define ARCH_FLAGS_LOG_LEVEL_VAR "ARCH_FLAGS_LOG_LEVEL"
#define ARCH_FLAGS_LOG_FILE_VAR "ARCH_FLAGS_LOG_FILE"
#define ARCH_FLAGS_LOG_MAJOR 0
#define ARCH_FLAGS_LOG_MINOR 0

/*
 * read_arch_flags -- read arch flags from file
 */
static int
read_arch_flags(char *opts, struct arch_flags *arch_flags)
{
	uint64_t alignment_desc;
	uint64_t reserved;
	uint16_t machine;
	uint8_t machine_class;
	uint8_t data;

	if (sscanf(opts, "%" SCNu16 ":%" SCNu8 ":%" SCNu8
			":0x%" SCNx64 ":0x%" SCNx64,
			&machine, &machine_class, &data,
			&alignment_desc, &reserved) != 5)
		return -1;

	util_get_arch_flags(arch_flags);

	if (machine)
		arch_flags->machine = machine;

	if (machine_class)
		arch_flags->machine_class = machine_class;

	if (data)
		arch_flags->data = data;

	if (alignment_desc)
		arch_flags->alignment_desc = alignment_desc;

	if (reserved)
		memcpy(arch_flags->reserved,
				&reserved, sizeof(arch_flags->reserved));

	return 0;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "arch_flags");

	common_init(ARCH_FLAGS_LOG_PREFIX,
		ARCH_FLAGS_LOG_LEVEL_VAR,
		ARCH_FLAGS_LOG_FILE_VAR,
		ARCH_FLAGS_LOG_MAJOR,
		ARCH_FLAGS_LOG_MINOR);

	if (argc < 2)
		FATAL_USAGE();

	int i;
	for (i = 1; i < argc; i++) {
		int ret;
		struct arch_flags arch_flags;

		if ((ret = read_arch_flags(argv[i], &arch_flags)) < 0)
			FATAL_USAGE();
		else if (ret == 0) {
			ret = util_check_arch_flags(&arch_flags);
			UT_OUT("check: %d", ret);
		}
	}

	common_fini();

	DONE(NULL);
}
