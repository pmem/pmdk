/*
 * Copyright 2014-2017, Intel Corporation
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
 * vmem_strdup.c -- unit test for vmem_strdup
 *
 * usage: vmem_strdup [directory]
 */

#include "unittest.h"
#include <wchar.h>

int
main(int argc, char *argv[])
{
	const char *text = "Some test text";
	const char *text_empty = "";
	const wchar_t *wtext = L"Some test text";
	const wchar_t *wtext_empty = L"";
	char *dir = NULL;
	void *mem_pool = NULL;
	VMEM *vmp;

	START(argc, argv, "vmem_strdup");

	if (argc == 2) {
		dir = argv[1];
	} else if (argc > 2) {
		UT_FATAL("usage: %s [directory]", argv[0]);
	}

	if (dir == NULL) {
		/* allocate memory for function vmem_create_in_region() */
		mem_pool = MMAP_ANON_ALIGNED(VMEM_MIN_POOL, 4 << 20);

		vmp = vmem_create_in_region(mem_pool, VMEM_MIN_POOL);
		if (vmp == NULL)
			UT_FATAL("!vmem_create_in_region");
	} else {
		vmp = vmem_create(dir, VMEM_MIN_POOL);
		if (vmp == NULL)
			UT_FATAL("!vmem_create");
	}

	char *str1 = vmem_strdup(vmp, text);
	wchar_t *wcs1 = vmem_wcsdup(vmp, wtext);
	UT_ASSERTne(str1, NULL);
	UT_ASSERTne(wcs1, NULL);
	UT_ASSERTeq(strcmp(text, str1), 0);
	UT_ASSERTeq(wcscmp(wtext, wcs1), 0);

	/* check that pointer came from mem_pool */
	if (dir == NULL) {
		UT_ASSERTrange(str1, mem_pool, VMEM_MIN_POOL);
		UT_ASSERTrange(wcs1, mem_pool, VMEM_MIN_POOL);
	}

	char *str2 = vmem_strdup(vmp, text_empty);
	wchar_t *wcs2 = vmem_wcsdup(vmp, wtext_empty);
	UT_ASSERTne(str2, NULL);
	UT_ASSERTne(wcs2, NULL);
	UT_ASSERTeq(strcmp(text_empty, str2), 0);
	UT_ASSERTeq(wcscmp(wtext_empty, wcs2), 0);

	/* check that pointer came from mem_pool */
	if (dir == NULL) {
		UT_ASSERTrange(str2, mem_pool, VMEM_MIN_POOL);
		UT_ASSERTrange(wcs2, mem_pool, VMEM_MIN_POOL);
	}

	vmem_free(vmp, str1);
	vmem_free(vmp, wcs1);
	vmem_free(vmp, str2);
	vmem_free(vmp, wcs2);

	vmem_delete(vmp);

	DONE(NULL);
}
