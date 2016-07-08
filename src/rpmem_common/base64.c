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
 * base64.c -- base64 source file
 */

#include <stdlib.h>
#include <errno.h>

#include "base64.h"

/*
 * base64_enc -- lookup table for encoding
 */
static uint8_t base64_enc[] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/',
};

/*
 * base64_dec -- lookup table for decoding
 */
static uint8_t Base64_dec[256];

/*
 * base64_pad -- number of padding bytes based on size % 4
 */
static uint8_t Base64_pad[] = {0, 2, 1};

#define BASE64_PAD	'='
#define base64_pack8(a, b, c) (\
	((a) << 2 * 8) |\
	((b) << 1 * 8) |\
	((c) << 0 * 8))

#define base64_pack6(a, b, c, d) (\
	((a) << 3 * 6) |\
	((b) << 2 * 6) |\
	((c) << 1 * 6) |\
	((d) << 0 * 6))

#define base64_unpack6(p, n) ((uint8_t)(((p) >> ((n) * 6)) & 0x3F))
#define base64_unpack8(p, n) ((uint8_t)(((p) >> ((n) * 8)) & 0xFF))

/*
 * base64_init -- initialize base64 lookup table for decoding
 */
void
base64_init(void)
{
	for (uint8_t i = 0; i < 64; i++)
		Base64_dec[base64_enc[i]] = i;
}

/*
 * base64_encode_len -- return buffer length for encoding
 */
static inline size_t
base64_encode_len(size_t len)
{
	return (4 * ((len + 2) / 3));
}

/*
 * base64_buff -- allocate buffer for enconfig and return its size
 */
uint8_t *
base64_buff(size_t len, size_t *out_len)
{
	*out_len = base64_encode_len(len);
	return malloc(*out_len);
}

/*
 * base64_encode -- base64 encoding
 */
int
base64_encode(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len)
{
	size_t j = 0;
	for (size_t i = 0; i < in_len; ) {
		uint32_t a = (uint32_t)(i < in_len ? in[i++] : 0);
		uint32_t b = (uint32_t)(i < in_len ? in[i++] : 0);
		uint32_t c = (uint32_t)(i < in_len ? in[i++] : 0);
		uint32_t p = (base64_pack8(a, b, c));

		out[j++] = base64_enc[base64_unpack6(p, 3)];
		out[j++] = base64_enc[base64_unpack6(p, 2)];
		out[j++] = base64_enc[base64_unpack6(p, 1)];
		out[j++] = base64_enc[base64_unpack6(p, 0)];
	}

	for (size_t i = 0; i < Base64_pad[in_len % 3]; i++)
		out[out_len - 1 - i] = BASE64_PAD;

	return 0;
}

/*
 * base64_decode -- base64 decoding
 */
int
base64_decode(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len)
{
	if (in_len % 4) {
		errno = EIO;
		return -1;
	}

	size_t len = in_len / 4 * 3;

	if (in[in_len - 1] == BASE64_PAD)
		len--;
	if (in[in_len - 2] == BASE64_PAD)
		len--;

	if (len != out_len) {
		errno = EIO;
		return -1;
	}

	size_t j = 0;
	for (size_t i = 0; i < in_len; ) {
		uint32_t a = in[i] == BASE64_PAD ? 0U : Base64_dec[in[i]];
		i++;
		uint32_t b = in[i] == BASE64_PAD ? 0U : Base64_dec[in[i]];
		i++;
		uint32_t c = in[i] == BASE64_PAD ? 0U : Base64_dec[in[i]];
		i++;
		uint32_t d = in[i] == BASE64_PAD ? 0U : Base64_dec[in[i]];
		i++;

		uint32_t p = base64_pack6(a, b, c, d);

		if (j < out_len)
			out[j++] = base64_unpack8(p, 2);
		if (j < out_len)
			out[j++] = base64_unpack8(p, 1);
		if (j < out_len)
			out[j++] = base64_unpack8(p, 0);
	}

	return 0;
}
