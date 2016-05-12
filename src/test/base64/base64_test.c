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
 * base64_test.c -- unit test for base64 encoder/decoder
 */
#include <string.h>

#include "base64.h"
#include "unittest.h"

static void
check_pair(char *data, char *b64)
{
	int ret;

	uint8_t *buff_in = (uint8_t *)data;
	uint8_t *buff_out = (uint8_t *)b64;
	size_t d_len = strlen(data);
	size_t b_len = strlen(b64);

	size_t len_dec;
	uint8_t *buff_enc = base64_buff(d_len, &len_dec);
	UT_ASSERTne(buff_enc, NULL);
	UT_ASSERTeq(len_dec, b_len);

	uint8_t *buff_dec = MALLOC(d_len);
	UT_ASSERTne(buff_dec, NULL);

	ret = base64_encode(buff_in, d_len, buff_enc, len_dec);
	UT_ASSERTeq(ret, 0);

	ret = memcmp(buff_enc, b64, len_dec);
	UT_ASSERTeq(ret, 0);

	ret = base64_decode(buff_out, b_len, buff_dec, d_len);
	UT_ASSERTeq(ret, 0);

	ret = memcmp(buff_dec, data, d_len);
	UT_ASSERTeq(ret, 0);

	FREE(buff_enc);
	FREE(buff_dec);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "base64");

	base64_init();

	if (argc < 2)
		UT_FATAL("usage: %s <data>:<base64>...", argv[0]);

	for (int i = 1; i < argc; i++) {
		char *pair = argv[i];
		char *colon = strchr(pair, ':');
		UT_ASSERTne(colon, NULL);

		*colon = '\0';
		colon++;

		check_pair(pair, colon);
	}

	DONE(NULL);
}
