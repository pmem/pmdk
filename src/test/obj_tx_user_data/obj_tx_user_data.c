// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * obj_tx_user_data.c -- unit test for pmemobj_tx_(get/set)_user_data
 */
#include "unittest.h"

#define LAYOUT_NAME "tx_user_data"

#define USER_DATA_V1 (void *) 123456789ULL
#define USER_DATA_V2 (void *) 987654321ULL

/*
 * do_tx_set_get_user_data_nested -- do set and verify user data in a tx
 */
static void
do_tx_set_get_user_data_nested(PMEMobjpool *pop)
{
	TX_BEGIN(pop) {
		pmemobj_tx_set_user_data(USER_DATA_V1);
		UT_ASSERTeq(USER_DATA_V1, pmemobj_tx_get_user_data());

		TX_BEGIN(pop) {
			UT_ASSERTeq(USER_DATA_V1, pmemobj_tx_get_user_data());
			pmemobj_tx_set_user_data(USER_DATA_V2);

			UT_ASSERTeq(USER_DATA_V2, pmemobj_tx_get_user_data());
		} TX_ONABORT {
			UT_ASSERT(0);
		} TX_END
	} TX_ONCOMMIT {
		UT_ASSERTeq(USER_DATA_V2, pmemobj_tx_get_user_data());
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END

	TX_BEGIN(pop) {
		UT_ASSERTeq(NULL, pmemobj_tx_get_user_data());
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

/*
 * do_tx_set_get_user_data_abort -- do set and verify user data in a tx after
 * tx abort
 */
static void
do_tx_set_get_user_data_abort(PMEMobjpool *pop)
{
	TX_BEGIN(pop) {
		pmemobj_tx_set_user_data(USER_DATA_V1);
		UT_ASSERTeq(USER_DATA_V1, pmemobj_tx_get_user_data());

		pmemobj_tx_abort(-1);
	} TX_ONCOMMIT {
		UT_ASSERT(0);
	} TX_ONABORT {
		UT_ASSERTeq(USER_DATA_V1, pmemobj_tx_get_user_data());
	} TX_END

	TX_BEGIN(pop) {
		UT_ASSERTeq(NULL, pmemobj_tx_get_user_data());
	} TX_ONABORT {
		UT_ASSERT(0);
	} TX_END
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_tx_user_data");

	if (argc != 2)
		UT_FATAL("usage: %s [file]", argv[0]);

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(argv[1], LAYOUT_NAME, PMEMOBJ_MIN_POOL,
	    S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create");

	do_tx_set_get_user_data_nested(pop);
	do_tx_set_get_user_data_abort(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
