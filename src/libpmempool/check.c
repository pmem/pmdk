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
 * check.c -- functions performing checks in proper order
 */

#include <stdint.h>

#include "out.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check.h"
#include "check_backup.h"
#include "check_pool_hdr.h"
#include "check_log_blk.h"
#include "check_btt_info.h"
#include "check_btt_map_flog.h"
#include "check_write.h"

#define CHECK_RESULT_IS_STOP(result)\
	((result) == CHECK_RESULT_ERROR ||\
		(result) == CHECK_RESULT_INTERNAL_ERROR ||\
		((result) == CHECK_RESULT_CANNOT_REPAIR) ||\
		((result) == CHECK_RESULT_NOT_CONSISTENT))

struct step {
	void (*func)(PMEMpoolcheck *);
	enum pool_type type;
	bool part;
	bool btt_dev;
};

static const struct step steps[] = {
	{
		.type		= POOL_TYPE_ALL | POOL_TYPE_UNKNOWN,
		.func		= check_backup,
		.part		= true,
		.btt_dev	= true
	},
	{
		.type		= POOL_TYPE_BLK | POOL_TYPE_LOG |
					POOL_TYPE_UNKNOWN,
		.func		= check_pool_hdr,
		.part		= true,
	},
	{
		.type		= POOL_TYPE_BLK | POOL_TYPE_LOG,
		.func		= check_log_blk,
		.part		= false,
	},
	{
		.type		= POOL_TYPE_BLK,
		.func		= check_btt_info,
		.part		= false,
		.btt_dev	= true
	},
	{
		.type		= POOL_TYPE_BLK,
		.func		= check_btt_map_flog,
		.part		= false,
		.btt_dev	= true
	},
	{
		.type		= POOL_TYPE_BLK | POOL_TYPE_LOG,
		.func		= check_write,
		.part		= false,
		.btt_dev	= true
	},
	{
		.func		= NULL,
	},
};

/*
 * check_init -- initialize check process
 */
int
check_init(PMEMpoolcheck *ppc)
{
	if (!(ppc->data = check_data_alloc()))
		goto error_data_malloc;
	if (!(ppc->pool = pool_data_alloc(ppc)))
		goto error_pool_malloc;

	return 0;

error_pool_malloc:
	check_data_free(ppc->data);
error_data_malloc:
	return -1;
}

/*
 * status_get -- (internal) get next check_status which should be presented
 *	to the user in assumed order: all info messages, error or question
 */
static struct check_status *
status_get(PMEMpoolcheck *ppc)
{
	struct check_status *status = NULL;

	/* clear cache if exists */
	check_clear_status_cache(ppc->data);

	/* return next info if exists */
	if ((status = check_pop_info(ppc->data)))
		return status;

	/* return error if exists */
	if ((status = check_pop_error(ppc->data)))
		return status;

	if (ppc->result == CHECK_RESULT_ASK_QUESTIONS) {
		/*
		 * push answer for previous question and return info if answer
		 * is not valid
		 */
		if (check_push_answer(ppc))
			if ((status = check_pop_info(ppc->data)))
				return status;

		/* if has next question ask it */
		if ((status = check_pop_question(ppc->data)))
			return status;

		/* process answers otherwise */
		ppc->result = CHECK_RESULT_PROCESS_ANSWERS;
	} else if (CHECK_RESULT_IS_STOP(ppc->result))
		check_end(ppc->data);

	return NULL;
}

/*
 * check_step -- perform single check step
 */
struct check_status *
check_step(PMEMpoolcheck *ppc)
{
	struct check_status *status = NULL;
	/* return if we have informations or questions to ask or check ended */
	if ((status = status_get(ppc)) || check_is_end(ppc->data))
		return status;

	/* get next step and check if exists */
	const struct step *step = &steps[check_step_get(ppc->data)];
	if (step->func == NULL) {
		check_end(ppc->data);
		return status;
	}

	/*
	 * Step would be performed if:
	 *	- it is required for BTT Device and pool is BTT Device
	 *	- pool type is one of the required pool type and it is not part
	 *		if parts are excluded from current step
	 */
	if (!(step->btt_dev && ppc->pool->params.is_btt_dev)) {
		if (!(step->type & ppc->pool->params.type) ||
				(ppc->pool->params.is_part && !step->part)) {
			/* skip test */
			check_step_inc(ppc->data);
			return NULL;
		}
	}

	/* perform step */
	step->func(ppc);

	/* move on to next step if no questions were generated */
	if (ppc->result != CHECK_RESULT_ASK_QUESTIONS)
		check_step_inc(ppc->data);

	/* get current status and return */
	return status_get(ppc);
}

/*
 * check_fini -- stop check process
 */
void
check_fini(PMEMpoolcheck *ppc)
{
	pool_data_free(ppc->pool);
	check_data_free(ppc->data);
}
