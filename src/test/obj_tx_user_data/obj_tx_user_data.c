/*
 * Copyright 2019-2020, Intel Corporation
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
