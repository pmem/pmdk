/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * common.h -- declarations of common functions
 */

#include <stdint.h>
#include <sys/queue.h>
#include <stdarg.h>
#include "util.h"
#include "log.h"
#include "blk.h"
#include "btt_layout.h"

#define	min(a, b) ((a) < (b) ? (a) : (b))

/*
 * pmem_pool_type_t -- pool types
 */
typedef enum {
	PMEM_POOL_TYPE_NONE	= 0x00,
	PMEM_POOL_TYPE_LOG	= 0x01,
	PMEM_POOL_TYPE_BLK	= 0x02,
	PMEM_POOL_TYPE_OBJ	= 0x04,
	PMEM_POOL_TYPE_ALL	= 0x0f,
	PMEM_POOL_TYPE_UNKNWON	= 0x80,
} pmem_pool_type_t;

struct range {
	LIST_ENTRY(range) next;
	uint64_t first;
	uint64_t last;
};

struct ranges {
	LIST_HEAD(rangeshead, range) head;
};

pmem_pool_type_t pmem_pool_type_parse_hdr(struct pool_hdr *hdrp);
pmem_pool_type_t pmem_pool_type_parse_str(char *str);
uint64_t pmem_pool_get_min_size(pmem_pool_type_t type);
pmem_pool_type_t pmem_pool_parse_params(char *fname, uint64_t *sizep,
		uint64_t *bsizep);
int util_validate_checksum(void *addr, size_t len, uint64_t *csum);
int util_parse_size(char *str, uint64_t *sizep);
int util_parse_ranges(char *str, struct ranges *rangesp, struct range *entirep);
int util_ranges_add(struct ranges *rangesp, uint64_t first, uint64_t last);
void util_ranges_clear(struct ranges *rangesp);
void util_convert2h_pool_hdr(struct pool_hdr *hdrp);
void util_convert2h_btt_info(struct btt_info *bttp);
void util_convert2le_pool_hdr(struct pool_hdr *hdrp);
void util_convert2le_btt_info(struct btt_info *bttp);
void util_convert2h_btt_flog(struct btt_flog *flogp);
void util_convert2le_btt_flog(struct btt_flog *flogp);
void util_convert2h_pmemlog(struct pmemlog *plp);
void util_convert2le_pmemlog(struct pmemlog *plp);
int util_check_memory(const uint8_t *buff, size_t len, uint8_t val);
int util_check_bsize(uint32_t bsize, uint64_t fsize);
uint32_t util_get_max_bsize(uint64_t fsize);
char ask(char op, char *answers, char def_ans, const char *fmt, va_list ap);
char ask_yn(char op, char def_ans, const char *fmt, va_list ap);
char ask_Yn(char op, const char *fmt, ...);
char ask_yN(char op, const char *fmt, ...);
