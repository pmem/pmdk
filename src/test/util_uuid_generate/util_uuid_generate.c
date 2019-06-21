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
 * util_uuid_generate.c -- unit test for generating a uuid
 *
 * usage: util_uuid_generate [string] [valid|invalid]
 */

#include "unittest.h"
#include "uuid.h"
#include <unistd.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_uuid_generate");

	uuid_t uuid;
	uuid_t uuid1;
	int ret;
	char conv_uu[POOL_HDR_UUID_STR_LEN];
	char uu[POOL_HDR_UUID_STR_LEN];

	/*
	 * No string passed in.  Generate uuid.
	 */
	if (argc == 1) {
		/* generate a UUID string */
		ret = ut_get_uuid_str(uu);
		UT_ASSERTeq(ret, 0);

		/*
		 * Convert the string to a uuid, convert generated
		 * uuid back to a string and compare strings.
		 */
		ret = util_uuid_from_string(uu, (struct uuid *)&uuid);
		UT_ASSERTeq(ret, 0);

		ret = util_uuid_to_string(uuid, conv_uu);
		UT_ASSERTeq(ret, 0);

		UT_ASSERT(strncmp(uu, conv_uu, POOL_HDR_UUID_STR_LEN) == 0);

		/*
		 * Generate uuid from util_uuid_generate and translate to
		 * string then back to uuid to verify they match.
		 */
		memset(uuid, 0, sizeof(uuid_t));
		memset(uu, 0, POOL_HDR_UUID_STR_LEN);
		memset(conv_uu, 0, POOL_HDR_UUID_STR_LEN);

		ret = util_uuid_generate(uuid);
		UT_ASSERTeq(ret, 0);

		ret = util_uuid_to_string(uuid, uu);
		UT_ASSERTeq(ret, 0);

		ret  = util_uuid_from_string(uu, (struct uuid *)&uuid1);
		UT_ASSERTeq(ret, 0);
		UT_ASSERT(memcmp(&uuid, &uuid1, sizeof(uuid_t)) == 0);
	} else {
		/*
		 * Caller passed in string.
		 */
		if (strcmp(argv[2], "valid") == 0) {
			ret = util_uuid_from_string(argv[1],
				(struct uuid *)&uuid);
			UT_ASSERTeq(ret, 0);

			ret = util_uuid_to_string(uuid, conv_uu);
			UT_ASSERTeq(ret, 0);
		} else {
			ret = util_uuid_from_string(argv[1],
				(struct uuid *)&uuid);
			UT_ASSERT(ret < 0);
			UT_OUT("util_uuid_generate: invalid uuid string");
		}
	}
	DONE(NULL);
}
