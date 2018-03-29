/*
 * Copyright 2017-2018, Intel Corporation
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
 * pool_open.c -- a tool for verifying that an obj/blk/log pool opens correctly
 *
 * usage: pool_open <path> <obj|blk|log> <layout>
 */
#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "compat_incompat_features");
	if (argc < 3)
		UT_FATAL("usage: %s <obj|blk|log|cto> <path>", argv[0]);

	char *type = argv[1];
	char *path = argv[2];

	if (strcmp(type, "obj") == 0) {
		PMEMobjpool *pop = pmemobj_open(path, "");
		if (pop == NULL) {
			UT_FATAL("!%s: pmemobj_open failed", path);
		} else {
			UT_OUT("%s: pmemobj_open succeeded", path);
			pmemobj_close(pop);
		}
	} else if (strcmp(type, "blk") == 0) {
		PMEMblkpool *pop = pmemblk_open(path, 0);
		if (pop == NULL) {
			UT_FATAL("!%s: pmemblk_open failed", path);
		} else {
			UT_OUT("%s: pmemblk_open succeeded", path);
			pmemblk_close(pop);
		}
	} else if (strcmp(type, "log") == 0) {
		PMEMlogpool *pop = pmemlog_open(path);
		if (pop == NULL) {
			UT_FATAL("!%s: pmemlog_open failed", path);
		} else {
			UT_OUT("%s: pmemlog_open succeeded", path);
			pmemlog_close(pop);
		}
	} else if (strcmp(type, "cto") == 0) {
		PMEMctopool *pop = pmemcto_open(path, "");
		if (pop == NULL) {
			UT_FATAL("!%s: pmemcto_open failed", path);
		} else {
			UT_OUT("%s: pmemcto_open succeeded", path);
			pmemcto_close(pop);
		}
	} else {
		UT_FATAL("usage: %s <obj|blk|log|cto> <path>", argv[0]);
	}

	DONE(NULL);
}
