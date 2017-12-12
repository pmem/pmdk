/*
 * Copyright 2016-2017, Intel Corporation
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
 * check_util.h -- internal definitions check util
 */

#include <time.h>
#include <limits.h>
#include <sys/param.h>

#define CHECK_STEP_COMPLETE	UINT_MAX
#define CHECK_INVALID_QUESTION	UINT_MAX

#define REQUIRE_ADVANCED	"the following error can be fixed using " \
				"PMEMPOOL_CHECK_ADVANCED flag"

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

/* check control context */
struct check_data;
struct arena;

/* queue of check statuses */
struct check_status;

/* container storing state of all check steps */
#define PREFIX_MAX_SIZE 30
typedef struct {
	unsigned step;

	unsigned replica;
	unsigned part;

	int single_repl;
	int single_part;

	struct pool_set *set;

	struct pool_hdr *hdrp;
	/* copy of the pool header in host byte order */
	struct pool_hdr hdr;
	int hdr_valid;
	/*
	 * If pool header has been modified this field indicates that
	 * the pool parameters structure requires refresh.
	 */
	int pool_hdr_modified;

	struct pool_hdr *next_part_hdrp;
	struct pool_hdr *prev_part_hdrp;
	struct pool_hdr *next_repl_hdrp;
	struct pool_hdr *prev_repl_hdrp;

	int next_part_hdr_valid;
	int prev_part_hdr_valid;
	int next_repl_hdr_valid;
	int prev_repl_hdr_valid;

	/* valid poolset uuid */
	uuid_t *valid_puuid;
	/* valid part uuid */
	uuid_t *valid_uuid;

	/* valid part pool header */
	struct pool_hdr *valid_part_hdrp;
	int valid_part_done;
	unsigned valid_part_replica;

	char prefix[PREFIX_MAX_SIZE];

	struct arena *arenap;
	uint64_t offset;
	uint32_t narena;

	uint8_t *bitmap;
	uint8_t *dup_bitmap;
	uint8_t *fbitmap;

	struct list *list_inval;
	struct list *list_flog_inval;
	struct list *list_unmap;

	struct {
		int btti_header;
		int btti_backup;
	} valid;

	struct {
		struct btt_info btti;
		uint64_t btti_offset;
	} pool_valid;
} location;

/* check steps */
void check_backup(PMEMpoolcheck *ppc);
void check_pool_hdr(PMEMpoolcheck *ppc);
void check_pool_hdr_uuids(PMEMpoolcheck *ppc);
void check_log(PMEMpoolcheck *ppc);
void check_blk(PMEMpoolcheck *ppc);
void check_cto(PMEMpoolcheck *ppc);
void check_btt_info(PMEMpoolcheck *ppc);
void check_btt_map_flog(PMEMpoolcheck *ppc);
void check_write(PMEMpoolcheck *ppc);

struct check_data *check_data_alloc(void);
void check_data_free(struct check_data *data);

uint32_t check_step_get(struct check_data *data);
void check_step_inc(struct check_data *data);
location *check_get_step_data(struct check_data *data);

void check_end(struct check_data *data);
int check_is_end_util(struct check_data *data);

int check_status_create(PMEMpoolcheck *ppc, enum pmempool_check_msg_type type,
		uint32_t question, const char *fmt, ...) FORMAT_PRINTF(4, 5);
void check_status_release(PMEMpoolcheck *ppc, struct check_status *status);
void check_clear_status_cache(struct check_data *data);
struct check_status *check_pop_question(struct check_data *data);
struct check_status *check_pop_error(struct check_data *data);
struct check_status *check_pop_info(struct check_data *data);
bool check_has_error(struct check_data *data);
bool check_has_answer(struct check_data *data);
int check_push_answer(PMEMpoolcheck *ppc);

struct pmempool_check_status *check_status_get_util(
	struct check_status *status);
int check_status_is(struct check_status *status,
	enum pmempool_check_msg_type type);

/* create info status */
#define CHECK_INFO(ppc, ...)\
	check_status_create(ppc, PMEMPOOL_CHECK_MSG_TYPE_INFO, 0, __VA_ARGS__)

/* create error status */
#define CHECK_ERR(ppc, ...)\
	check_status_create(ppc, PMEMPOOL_CHECK_MSG_TYPE_ERROR, 0, __VA_ARGS__)

/* create question status */
#define CHECK_ASK(ppc, question, ...)\
	check_status_create(ppc, PMEMPOOL_CHECK_MSG_TYPE_QUESTION, question,\
		__VA_ARGS__)

#define CHECK_NOT_COMPLETE(loc, steps)\
	((loc)->step != CHECK_STEP_COMPLETE &&\
		((steps)[(loc)->step].check != NULL ||\
		(steps)[(loc)->step].fix != NULL))

int check_answer_loop(PMEMpoolcheck *ppc, location *data, void *ctx,
	int (*callback)(PMEMpoolcheck *, location *, uint32_t, void *ctx));
int check_questions_sequence_validate(PMEMpoolcheck *ppc);

const char *check_get_time_str(time_t time);
const char *check_get_uuid_str(uuid_t uuid);
const char *check_get_pool_type_str(enum pool_type type);

void check_insert_arena(PMEMpoolcheck *ppc, struct arena *arenap);

#ifdef _WIN32
void cache_to_utf8(struct check_data *data, char *buf, size_t size);
#endif

#define CHECK_IS(ppc, flag)\
	util_flag_isset((ppc)->args.flags, PMEMPOOL_CHECK_ ## flag)

#define CHECK_IS_NOT(ppc, flag)\
	util_flag_isclr((ppc)->args.flags, PMEMPOOL_CHECK_ ## flag)

#define CHECK_WITHOUT_FIXING(ppc)\
	CHECK_IS_NOT(ppc, REPAIR) || CHECK_IS(ppc, DRY_RUN)
