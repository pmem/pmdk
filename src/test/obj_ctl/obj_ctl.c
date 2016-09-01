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
 * obj_ctl.c -- tests for the libpmemobj control module
 */

#include "unittest.h"

static void
test_ctl_parser(PMEMobjpool *pop)
{
	int ret;
	ret = pmemobj_ctl(pop, "a.b.c.d", NULL, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "", NULL, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "debug.", NULL, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, ".", NULL, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "..", NULL, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "1.2.3.4", NULL, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "debug.1.", NULL, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "debug.1.invalid", NULL, NULL);
	UT_ASSERTne(ret, 0);

	/* test methods set read to 0 and write to 1 if successful */
	int arg_read = 1;
	int arg_write = 0;

	/* correct name, wrong args */
	ret = pmemobj_ctl(pop, "debug.test_rw", NULL, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "debug.test_wo", &arg_read, NULL);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "debug.test_wo", &arg_read, &arg_write);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "debug.test_ro", NULL, &arg_write);
	UT_ASSERTne(ret, 0);
	ret = pmemobj_ctl(pop, "debug.test_ro", &arg_read, &arg_write);
	UT_ASSERTne(ret, 0);

	ret = pmemobj_ctl(pop, "debug.test_rw", &arg_read, &arg_write);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
	UT_ASSERTeq(arg_write, 1);

	arg_read = 1;
	arg_write = 0;

	ret = pmemobj_ctl(pop, "debug.test_ro", &arg_read, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 0);
	UT_ASSERTeq(arg_write, 0);

	arg_read = 1;
	arg_write = 0;

	ret = pmemobj_ctl(pop, "debug.test_wo", NULL, &arg_write);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(arg_read, 1);
	UT_ASSERTeq(arg_write, 1);

	long index_value = 0;
	ret = pmemobj_ctl(pop, "debug.5.index_value", &index_value, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(index_value, 5);

	ret = pmemobj_ctl(pop, "debug.10.index_value", &index_value, NULL);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(index_value, 10);

}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;
	if ((pop = pmemobj_create(path, "ctl", PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	test_ctl_parser(pop);

	pmemobj_close(pop);

	DONE(NULL);
}
