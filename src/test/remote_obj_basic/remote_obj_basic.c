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
 * remote_obj_basic.c -- unit test for remote tests support
 *
 * usage: remote_obj_basic <create|open> <poolset-file>
 */

#include "unittest.h"

#define LAYOUT_NAME "remote_obj_basic"

int
main(int argc, char *argv[])
{
	PMEMobjpool *pop;

	START(argc, argv, "remote_obj_basic");

	if (argc != 3)
		UT_FATAL("usage: %s <create|open> <poolset-file>", argv[0]);

	const char *mode = argv[1];
	const char *file = argv[2];

	if (strcmp(mode, "create") == 0) {
		if ((pop = pmemobj_create(file, LAYOUT_NAME, 0,
						S_IWUSR | S_IRUSR)) == NULL)
			UT_FATAL("!pmemobj_create: %s", file);
		else
			UT_OUT("The pool set %s has been created", file);

	} else if (strcmp(mode, "open") == 0) {
		if ((pop = pmemobj_open(file, LAYOUT_NAME)) == NULL)
			UT_FATAL("!pmemobj_open: %s", file);
		else
			UT_OUT("The pool set %s has been opened", file);

	} else {
		UT_FATAL("wrong mode: %s\n", argv[1]);
	}

	pmemobj_close(pop);

	DONE(NULL);
}
