// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * check_util.c -- check utility functions
 */

#include <stdio.h>
#include <stdint.h>

#include "out.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

#define CHECK_END UINT_MAX

/* separate info part of message from question part of message */
#define MSG_SEPARATOR	'|'

/* error part of message must have '.' at the end */
#define MSG_PLACE_OF_SEPARATION	'.'
#define MAX_MSG_STR_SIZE 8192

#define CHECK_ANSWER_YES	"yes"
#define CHECK_ANSWER_NO		"no"

#define STR_MAX 256
#define TIME_STR_FMT "%a %b %d %Y %H:%M:%S"

#define UUID_STR_MAX 37

enum check_answer {
	PMEMPOOL_CHECK_ANSWER_EMPTY,
	PMEMPOOL_CHECK_ANSWER_YES,
	PMEMPOOL_CHECK_ANSWER_NO,
	PMEMPOOL_CHECK_ANSWER_DEFAULT,
};

/* queue of check statuses */
struct check_status {
	PMDK_TAILQ_ENTRY(check_status) next;
	struct pmempool_check_status status;
	unsigned question;
	enum check_answer answer;
	char *msg;
};

PMDK_TAILQ_HEAD(check_status_head, check_status);

/* check control context */
struct check_data {
	unsigned step;
	location step_data;

	struct check_status *error;
	struct check_status_head infos;
	struct check_status_head questions;
	struct check_status_head answers;

	struct check_status *check_status_cache;
};

/*
 * check_data_alloc --  allocate and initialize check_data structure
 */
struct check_data *
check_data_alloc(void)
{
	LOG(3, NULL);

	struct check_data *data = calloc(1, sizeof(*data));
	if (data == NULL) {
		ERR("!calloc");
		return NULL;
	}

	PMDK_TAILQ_INIT(&data->infos);
	PMDK_TAILQ_INIT(&data->questions);
	PMDK_TAILQ_INIT(&data->answers);

	return data;
}

/*
 * check_data_free -- clean and deallocate check_data
 */
void
check_data_free(struct check_data *data)
{
	LOG(3, NULL);

	if (data->error != NULL) {
		free(data->error);
		data->error = NULL;
	}

	if (data->check_status_cache != NULL) {
		free(data->check_status_cache);
		data->check_status_cache = NULL;
	}

	while (!PMDK_TAILQ_EMPTY(&data->infos)) {
		struct check_status *statp = PMDK_TAILQ_FIRST(&data->infos);
		PMDK_TAILQ_REMOVE(&data->infos, statp, next);
		free(statp);
	}

	while (!PMDK_TAILQ_EMPTY(&data->questions)) {
		struct check_status *statp = PMDK_TAILQ_FIRST(&data->questions);
		PMDK_TAILQ_REMOVE(&data->questions, statp, next);
		free(statp);
	}

	while (!PMDK_TAILQ_EMPTY(&data->answers)) {
		struct check_status *statp = PMDK_TAILQ_FIRST(&data->answers);
		PMDK_TAILQ_REMOVE(&data->answers, statp, next);
		free(statp);
	}

	free(data);
}

/*
 * check_step_get - return current check step number
 */
uint32_t
check_step_get(struct check_data *data)
{
	return data->step;
}

/*
 * check_step_inc -- move to next step number
 */
void
check_step_inc(struct check_data *data)
{
	if (check_is_end_util(data))
		return;

	++data->step;
	memset(&data->step_data, 0, sizeof(location));
}

/*
 * check_get_step_data -- return pointer to check step data
 */
location *
check_get_step_data(struct check_data *data)
{
	return &data->step_data;
}

/*
 * check_end -- mark check as ended
 */
void
check_end(struct check_data *data)
{
	LOG(3, NULL);

	data->step = CHECK_END;
}

/*
 * check_is_end_util -- return if check has ended
 */
int
check_is_end_util(struct check_data *data)
{
	return data->step == CHECK_END;
}

/*
 * status_alloc -- (internal) allocate and initialize check_status
 */
static inline struct check_status *
status_alloc(void)
{
	struct check_status *status = malloc(sizeof(*status));
	if (!status)
		FATAL("!malloc");
	status->msg = malloc(sizeof(char) * MAX_MSG_STR_SIZE);
	if (!status->msg) {
		free(status);
		FATAL("!malloc");
	}
	status->status.str.msg = status->msg;
	status->answer = PMEMPOOL_CHECK_ANSWER_EMPTY;
	status->question = CHECK_INVALID_QUESTION;
	return status;
}

/*
 * status_release -- (internal) release check_status
 */
static void
status_release(struct check_status *status)
{
#ifdef _WIN32
	/* dealloc duplicate string after conversion */
	if (status->status.str.msg != status->msg)
		free((void *)status->status.str.msg);
#endif
	free(status->msg);
	free(status);
}

/*
 * status_msg_info_only -- (internal) separate info part of the message
 *
 * If message is in form of "info.|question" it modifies it as follows
 * "info\0|question"
 */
static inline int
status_msg_info_only(const char *msg)
{
	char *sep = strchr(msg, MSG_SEPARATOR);
	if (sep) {
		ASSERTne(sep, msg);
		--sep;
		ASSERTeq(*sep, MSG_PLACE_OF_SEPARATION);
		*sep = '\0';
		return 0;
	}
	return -1;
}

/*
 * status_msg_info_and_question -- (internal) join info and question
 *
 * If message is in form "info.|question" it will replace MSG_SEPARATOR '|' with
 * space to get "info. question"
 */
static inline int
status_msg_info_and_question(const char *msg)
{
	char *sep = strchr(msg, MSG_SEPARATOR);
	if (sep) {
		*sep = ' ';
		return 0;
	}
	return -1;
}

/*
 * status_push -- (internal) push single status object
 */
static int
status_push(PMEMpoolcheck *ppc, struct check_status *st, uint32_t question)
{
	if (st->status.type == PMEMPOOL_CHECK_MSG_TYPE_ERROR) {
		ASSERTeq(ppc->data->error, NULL);
		ppc->data->error = st;
		return -1;
	} else if (st->status.type == PMEMPOOL_CHECK_MSG_TYPE_INFO) {
		if (CHECK_IS(ppc, VERBOSE))
			PMDK_TAILQ_INSERT_TAIL(&ppc->data->infos, st, next);
		else
			check_status_release(ppc, st);
		return 0;
	}

	/* st->status.type == PMEMPOOL_CHECK_MSG_TYPE_QUESTION */
	if (CHECK_IS_NOT(ppc, REPAIR)) {
		/* error status */
		if (status_msg_info_only(st->msg)) {
			ERR("no error message for the user");
			st->msg[0] = '\0';
		}
		st->status.type = PMEMPOOL_CHECK_MSG_TYPE_ERROR;
		return status_push(ppc, st, question);
	}

	if (CHECK_IS(ppc, ALWAYS_YES)) {
		if (!status_msg_info_only(st->msg)) {
			/* information status */
			st->status.type = PMEMPOOL_CHECK_MSG_TYPE_INFO;
			status_push(ppc, st, question);
			st = status_alloc();
		}

		/* answer status */
		ppc->result = CHECK_RESULT_PROCESS_ANSWERS;
		st->question = question;
		st->answer = PMEMPOOL_CHECK_ANSWER_YES;
		st->status.type = PMEMPOOL_CHECK_MSG_TYPE_QUESTION;
		PMDK_TAILQ_INSERT_TAIL(&ppc->data->answers, st, next);
	} else {
		/* question message */
		status_msg_info_and_question(st->msg);
		st->question = question;
		ppc->result = CHECK_RESULT_ASK_QUESTIONS;
		st->answer = PMEMPOOL_CHECK_ANSWER_EMPTY;
		PMDK_TAILQ_INSERT_TAIL(&ppc->data->questions, st, next);
	}

	return 0;
}

/*
 * check_status_create -- create single status, push it to proper queue
 *
 * MSG_SEPARATOR character in fmt is treated as message separator. If creating
 * question but check arguments do not allow to make any changes (asking any
 * question is pointless) it takes part of message before MSG_SEPARATOR
 * character and use it to create error message. Character just before separator
 * must be a MSG_PLACE_OF_SEPARATION character. Return non 0 value if error
 * status would be created.
 *
 * The arg is an additional argument for specified type of status.
 */
int
check_status_create(PMEMpoolcheck *ppc, enum pmempool_check_msg_type type,
	uint32_t arg, const char *fmt, ...)
{
	if (CHECK_IS_NOT(ppc, VERBOSE) && type == PMEMPOOL_CHECK_MSG_TYPE_INFO)
		return 0;

	struct check_status *st = status_alloc();
	ASSERT(CHECK_IS(ppc, FORMAT_STR));

	va_list ap;
	va_start(ap, fmt);
	int p = vsnprintf(st->msg, MAX_MSG_STR_SIZE, fmt, ap);
	va_end(ap);

	/* append possible strerror at the end of the message */
	if (type != PMEMPOOL_CHECK_MSG_TYPE_QUESTION && arg && p > 0) {
		char buff[UTIL_MAX_ERR_MSG];
		util_strerror((int)arg, buff, UTIL_MAX_ERR_MSG);
		int ret = util_snprintf(st->msg + p,
				MAX_MSG_STR_SIZE - (size_t)p, ": %s", buff);
		if (ret < 0) {
			ERR("!snprintf");
			status_release(st);
			return -1;
		}
	}

	st->status.type = type;

	return status_push(ppc, st, arg);
}

/*
 * check_status_release -- release single status object
 */
void
check_status_release(PMEMpoolcheck *ppc, struct check_status *status)
{
	if (status->status.type == PMEMPOOL_CHECK_MSG_TYPE_ERROR)
		ppc->data->error = NULL;

	status_release(status);
}

/*
 * pop_status -- (internal) pop single message from check_status queue
 */
static struct check_status *
pop_status(struct check_data *data, struct check_status_head *queue)
{
	if (!PMDK_TAILQ_EMPTY(queue)) {
		ASSERTeq(data->check_status_cache, NULL);
		data->check_status_cache = PMDK_TAILQ_FIRST(queue);
		PMDK_TAILQ_REMOVE(queue, data->check_status_cache, next);
		return data->check_status_cache;
	}

	return NULL;
}

/*
 * check_pop_question -- pop single question from questions queue
 */
struct check_status *
check_pop_question(struct check_data *data)
{
	return pop_status(data, &data->questions);
}

/*
 * check_pop_info -- pop single info from information queue
 */
struct check_status *
check_pop_info(struct check_data *data)
{
	return pop_status(data, &data->infos);
}

/*
 * check_pop_error -- pop error from state
 */
struct check_status *
check_pop_error(struct check_data *data)
{
	if (data->error) {
		ASSERTeq(data->check_status_cache, NULL);

		data->check_status_cache = data->error;
		data->error = NULL;
		return data->check_status_cache;
	}

	return NULL;
}

#ifdef _WIN32
void
cache_to_utf8(struct check_data *data, char *buf, size_t size)
{
	if (data->check_status_cache == NULL)
		return;

	struct check_status *status = data->check_status_cache;

	/* if it was a question, convert it and the answer to utf8 */
	if (status->status.type == PMEMPOOL_CHECK_MSG_TYPE_QUESTION) {
		struct pmempool_check_statusW *wstatus =
			(struct pmempool_check_statusW *)&status->status;
		wchar_t *wstring = (wchar_t *)wstatus->str.msg;
		status->status.str.msg = util_toUTF8(wstring);
		if (status->status.str.msg == NULL)
			FATAL("!malloc");
		util_free_UTF16(wstring);

		if (util_toUTF8_buff(wstatus->str.answer, buf, size) != 0)
			FATAL("Invalid answer conversion %s",
				out_get_errormsg());
		status->status.str.answer = buf;
	}
}
#endif

/*
 * check_clear_status_cache -- release check_status from cache
 */
void
check_clear_status_cache(struct check_data *data)
{
	if (data->check_status_cache) {
		switch (data->check_status_cache->status.type) {
		case PMEMPOOL_CHECK_MSG_TYPE_INFO:
		case PMEMPOOL_CHECK_MSG_TYPE_ERROR:
			/*
			 * Info and error statuses are disposable. After showing
			 * them to the user we have to release them.
			 */
			status_release(data->check_status_cache);
			data->check_status_cache = NULL;
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_QUESTION:
			/*
			 * Question status after being showed to the user carry
			 * users answer. It must be kept till answer would be
			 * processed so it can not be released from cache. It
			 * has to be pushed to the answers queue, processed and
			 * released after that.
			 */
			break;
		default:
			ASSERT(0);
		}
	}
}

/*
 * status_answer_push -- (internal) push single answer to answers queue
 */
static void
status_answer_push(struct check_data *data, struct check_status *st)
{
	ASSERTeq(st->status.type, PMEMPOOL_CHECK_MSG_TYPE_QUESTION);
	PMDK_TAILQ_INSERT_TAIL(&data->answers, st, next);
}

/*
 * check_push_answer -- process answer and push it to answers queue
 */
int
check_push_answer(PMEMpoolcheck *ppc)
{
	if (ppc->data->check_status_cache == NULL)
		return 0;

	/* check if answer is "yes" or "no" */
	struct check_status *status = ppc->data->check_status_cache;
	if (status->status.str.answer != NULL) {
		if (strcmp(status->status.str.answer, CHECK_ANSWER_YES) == 0)
			status->answer = PMEMPOOL_CHECK_ANSWER_YES;
		else if (strcmp(status->status.str.answer, CHECK_ANSWER_NO)
				== 0)
			status->answer = PMEMPOOL_CHECK_ANSWER_NO;
	}

	if (status->answer == PMEMPOOL_CHECK_ANSWER_EMPTY) {
		/* invalid answer provided */
		status_answer_push(ppc->data, ppc->data->check_status_cache);
		ppc->data->check_status_cache = NULL;
		CHECK_INFO(ppc, "Answer must be either %s or %s",
			CHECK_ANSWER_YES, CHECK_ANSWER_NO);
		return -1;
	}

	/* push answer */
	PMDK_TAILQ_INSERT_TAIL(&ppc->data->answers,
		ppc->data->check_status_cache, next);
	ppc->data->check_status_cache = NULL;

	return 0;
}
/*
 * check_has_error - check if error exists
 */
bool
check_has_error(struct check_data *data)
{
	return data->error != NULL;
}

/*
 * check_has_answer - check if any answer exists
 */
bool
check_has_answer(struct check_data *data)
{
	return !PMDK_TAILQ_EMPTY(&data->answers);
}

/*
 * pop_answer -- (internal) pop single answer from answers queue
 */
static struct check_status *
pop_answer(struct check_data *data)
{
	struct check_status *ret = NULL;
	if (!PMDK_TAILQ_EMPTY(&data->answers)) {
		ret = PMDK_TAILQ_FIRST(&data->answers);
		PMDK_TAILQ_REMOVE(&data->answers, ret, next);
	}
	return ret;
}

/*
 * check_status_get_util -- extract pmempool_check_status from check_status
 */
struct pmempool_check_status *
check_status_get_util(struct check_status *status)
{
	return &status->status;
}

/*
 * check_answer_loop -- loop through all available answers and process them
 */
int
check_answer_loop(PMEMpoolcheck *ppc, location *data, void *ctx, int fail_on_no,
	int (*callback)(PMEMpoolcheck *, location *, uint32_t, void *ctx))
{
	struct check_status *answer;

	while ((answer = pop_answer(ppc->data)) != NULL) {
		/* if answer is "no" we cannot fix an issue */
		if (answer->answer != PMEMPOOL_CHECK_ANSWER_YES) {
			if (fail_on_no ||
				answer->answer != PMEMPOOL_CHECK_ANSWER_NO) {
				CHECK_ERR(ppc,
					"cannot complete repair, reverting changes");
				ppc->result = CHECK_RESULT_NOT_CONSISTENT;
				goto error;
			}

			ppc->result = CHECK_RESULT_REPAIRED;
			check_status_release(ppc, answer);
			continue;
		}

		/* perform fix */
		if (callback(ppc, data, answer->question, ctx)) {
			ppc->result = CHECK_RESULT_CANNOT_REPAIR;
			goto error;
		}

		if (ppc->result == CHECK_RESULT_ERROR)
			goto error;

		/* fix succeeded */
		ppc->result = CHECK_RESULT_REPAIRED;
		check_status_release(ppc, answer);
	}

	return 0;

error:
	check_status_release(ppc, answer);
	return -1;
}

/*
 * check_questions_sequence_validate -- generate return value from result
 *
 * Sequence of questions can result in one of the following results: CONSISTENT,
 * REPAIRED, ASK_QUESTIONS of PROCESS_ANSWERS. If result == ASK_QUESTIONS it
 * returns -1 to indicate existence of unanswered questions.
 */
int
check_questions_sequence_validate(PMEMpoolcheck *ppc)
{
	ASSERT(ppc->result == CHECK_RESULT_CONSISTENT ||
		ppc->result == CHECK_RESULT_ASK_QUESTIONS ||
		ppc->result == CHECK_RESULT_PROCESS_ANSWERS ||
		ppc->result == CHECK_RESULT_REPAIRED);
	if (ppc->result == CHECK_RESULT_ASK_QUESTIONS) {
		ASSERT(!PMDK_TAILQ_EMPTY(&ppc->data->questions));
		return -1;
	}

	return 0;
}

/*
 * check_get_time_str -- returns time in human-readable format
 */
const char *
check_get_time_str(time_t time)
{
	static char str_buff[STR_MAX] = {0, };
	struct tm tm;

	if (util_localtime(&time, &tm))
		strftime(str_buff, STR_MAX, TIME_STR_FMT, &tm);
	else {
		int ret = util_snprintf(str_buff, STR_MAX, "unknown");
		if (ret < 0) {
			ERR("!snprintf");
			return "";
		}
	}
	return str_buff;
}

/*
 * check_get_uuid_str -- returns uuid in human readable format
 */
const char *
check_get_uuid_str(uuid_t uuid)
{
	static char uuid_str[UUID_STR_MAX] = {0, };

	int ret = util_uuid_to_string(uuid, uuid_str);
	if (ret != 0) {
		ERR("failed to covert uuid to string");
		return "";
	}
	return uuid_str;
}

/*
 * pmempool_check_insert_arena -- insert arena to list
 */
void
check_insert_arena(PMEMpoolcheck *ppc, struct arena *arenap)
{
	PMDK_TAILQ_INSERT_TAIL(&ppc->pool->arenas, arenap, next);
	ppc->pool->narenas++;
}
