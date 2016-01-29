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
 * check_util.h -- internal definitions check util
 */

#include <time.h>
#include <limits.h>
#include <sys/param.h>

#define CHECK_STEP_COMPLETE	UINT_MAX
#define CHECK_INVALID_QUESTION	UINT_MAX

#define min(a, b) ((a) < (b) ? (a) : (b))

/* check control context */
struct check_data;
struct arena;

/* queue of check statuses */
struct check_status;

/*
 * container storing check step state
 *
 * It's size is equal to the size of the biggest state structure it must store.
 */
#define CHECK_INSTEP_LOCATION_NUM 517

struct check_step_data {
	uint64_t location[CHECK_INSTEP_LOCATION_NUM];
};

/* check steps */
void check_backup(PMEMpoolcheck *ppc);
void check_pool_hdr(PMEMpoolcheck *ppc);
void check_log_blk(PMEMpoolcheck *ppc);
void check_btt_info(PMEMpoolcheck *ppc);
void check_btt_map_flog(PMEMpoolcheck *ppc);
void check_write(PMEMpoolcheck *ppc);

struct check_data *check_data_alloc(void);
void check_data_free(struct check_data *data);

uint32_t check_step_get(struct check_data *data);
void check_step_inc(struct check_data *data);
struct check_step_data *check_get_step_data(struct check_data *data);

void check_end(struct check_data *data);
int check_is_end_util(struct check_data *data);

int check_status_create(PMEMpoolcheck *ppc, enum pmempool_check_msg_type type,
	uint32_t question, const char *fmt, ...);
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

int check_answer_loop(PMEMpoolcheck *ppc, struct check_step_data *data,
	void *ctx, int (*callback)(PMEMpoolcheck *, struct check_step_data *,
	uint32_t, void *ctx));
int check_questions_sequence_validate(PMEMpoolcheck *ppc);

const char *check_get_time_str(time_t time);
const char *check_get_uuid_str(uuid_t uuid);
const char *check_get_pool_type_str(enum pool_type type);

void check_insert_arena(PMEMpoolcheck *ppc, struct arena *arenap);

#define CHECK_IS(ppc, flag)\
	util_flag_isset((ppc)->args.flags, PMEMPOOL_CHECK_ ## flag)

#define CHECK_IS_NOT(ppc, flag)\
	util_flag_isclr((ppc)->args.flags, PMEMPOOL_CHECK_ ## flag)

#define CHECK_WITHOUT_FIXING(ppc)\
	CHECK_IS_NOT(ppc, REPAIR) || CHECK_IS(ppc, DRY_RUN)
