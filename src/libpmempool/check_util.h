/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2023, Intel Corporation */

/*
 * check_util.h -- internal definitions check util
 */
#ifndef CHECK_UTIL_H
#define CHECK_UTIL_H

#include <time.h>
#include <limits.h>
#include <sys/param.h>

#ifdef __cplusplus
extern "C" {
#endif

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
	unsigned init_done;
	unsigned step;

	unsigned replica;
	unsigned part;

	int single_repl;
	int single_part;

	struct pool_set *set;
	int is_dev_dax;

	struct pool_hdr *hdrp;
	/* copy of the pool header in host byte order */
	struct pool_hdr hdr;
	int hdr_valid;
	/*
	 * If pool header has been modified this field indicates that
	 * the pool parameters structure requires refresh.
	 */
	int pool_hdr_modified;

	unsigned healthy_replicas;

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
} location;

/* check steps */
void check_bad_blocks(PMEMpoolcheck *ppc);
void check_backup(PMEMpoolcheck *ppc);
void check_pool_hdr(PMEMpoolcheck *ppc);
void check_pool_hdr_uuids(PMEMpoolcheck *ppc);
void check_sds(PMEMpoolcheck *ppc);

struct check_data *check_data_alloc(void);
void check_data_free(struct check_data *data);

uint32_t check_step_get(struct check_data *data);
void check_step_inc(struct check_data *data);
location *check_get_step_data(struct check_data *data);

void check_end(struct check_data *data);
int check_is_end_util(struct check_data *data);

int check_status_create(PMEMpoolcheck *ppc, enum pmempool_check_msg_type type,
		uint32_t arg, const char *fmt, ...) FORMAT_PRINTF(4, 5);
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

/* create info status and append error message based on errno */
#define CHECK_INFO_ERRNO(ppc, ...)\
	check_status_create(ppc, PMEMPOOL_CHECK_MSG_TYPE_INFO,\
			(uint32_t)errno, __VA_ARGS__)

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

int check_answer_loop(PMEMpoolcheck *ppc, location *data,
	void *ctx, int fail_on_no,
	int (*callback)(PMEMpoolcheck *, location *, uint32_t, void *ctx));
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

#ifdef __cplusplus
}
#endif

#endif
