// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * daos_dtx.c -- XXX
 */

#include <ddb.h>

#include "unittest.h"

#define SYS_DB_NAME "sys_db"

#define DTX_DUMP_PATH "/591d19e4-69fe-11ef-b13d-a4bf0165c389"

static bool
is_sys_db(const char *path)
{
	const char *file_name = strrchr(path, '/');
	++file_name;
	return strncmp(file_name, SYS_DB_NAME, sizeof(SYS_DB_NAME)) == 0;
}

/*
 * cmd_verify -- XXX
 */
static int
cmd_verify(const struct test_case *tc, int argc, char *argv[])
{
	struct ddb_ctx ctx;
	struct open_options open;
	struct dtx_dump_options dtx_dump;
	int rc;

	if (argc < 1) {
		UT_FATAL("usage: %s filename", __FUNCTION__);
	}

	const char *path = argv[0];

	if (is_sys_db(path)) {
		/*
		 * pmreorder asks to verify the consistency of each of
		 * the recorded files separately. This test ignores changes made
		 * to sys_db so just skip it.
		 */
		END(0);
	}

	fprintf(stderr, "%s %s\n", __FUNCTION__, path);

	/*
	 * The setting preferred by the pmreorder's verify implementations.
	 */
	int y = 1;
	pmemobj_ctl_set(NULL, "copy_on_write.at_open", &y);

	rc = ddb_init();
	UT_ASSERTeq(rc, 0);

	ddb_ctx_init(&ctx);
	ctx.dc_io_ft.ddb_print_message = ctx.dc_io_ft.ddb_print_error;

	/* open */
	open.write_mode = false;
	open.path = (char *)path;
	rc = ddb_run_open(&ctx, &open);
	UT_ASSERTeq(rc, 0);

	/* dtx_dump */
	dtx_dump.active = true;
	dtx_dump.committed = true;
	dtx_dump.path = DTX_DUMP_PATH;
	rc = ddb_run_dtx_dump(&ctx, &dtx_dump);
	UT_ASSERTeq(rc, 0);

	/* close */
	rc = ddb_run_close(&ctx);
	UT_ASSERTeq(rc, 0);

	ddb_fini();

	/*
	 * If the verify did not fail till now it has passed successfully.
	 * Return the result ASAP.
	 */
	END(0);
}

static struct test_case test_cases[] = {
	TEST_CASE(cmd_verify),
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "daos_dtx");

	TEST_CASE_PROCESS(argc, argv, test_cases, ARRAY_SIZE(test_cases));

	DONE(NULL);
}
