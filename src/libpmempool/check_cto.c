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
 * check_cto.c -- check pmemcto
 */

#include <inttypes.h>
#include <sys/param.h>
#include <endian.h>

#include "out.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check_util.h"

enum question {
	Q_CTO_CHECKSUM,
	Q_CTO_LAYOUT,
	Q_CTO_CONSISTENT,
	Q_CTO_ADDR,
	Q_CTO_SIZE,
	Q_CTO_ROOT
};

/*
 * cto_read -- (internal) read pmemcto header
 */
static int
cto_read(PMEMpoolcheck *ppc)
{
	/*
	 * Here we want to read the pmemcto header without the pool_hdr as we've
	 * already done it before.
	 *
	 * Take the pointer to fields right after pool_hdr, compute the size and
	 * offset of remaining fields.
	 */
	uint8_t *ptr = (uint8_t *)&ppc->pool->hdr.cto;
	ptr += sizeof(ppc->pool->hdr.cto.hdr);

	size_t size = sizeof(ppc->pool->hdr.cto) -
		sizeof(ppc->pool->hdr.cto.hdr);
	uint64_t offset = sizeof(ppc->pool->hdr.log.hdr);

	if (pool_read(ppc->pool, ptr, size, offset))
		return CHECK_ERR(ppc, "cannot read pmemcto structure");

	return 0;
}

/*
 * cto_hdr_check -- (internal) check pmemcto header
 */
static int
cto_hdr_check(PMEMpoolcheck *ppc, location *loc)
{
	LOG(3, NULL);

	CHECK_INFO(ppc, "checking pmemcto header");

	if (cto_read(ppc)) {
		ppc->result = CHECK_RESULT_ERROR;
		return -1;
	}

#if 0
	/* XXX: CTO */
	void *dscp = (void *)((uintptr_t)(&pcp->hdr) +
				sizeof(struct pool_hdr));

	if (!util_checksum(dscp, CTO_DSC_P_SIZE, &pcp->checksum, 0)) {
		if (CHECK_ASK(ppc, Q_CTO_CHECKSUM,
				"invalid pmemcto.checksum.|Do you want to ignore pmemcto.checksum?"))
			goto error;
	}

	if (ppc->pool->hdr.cto.layout) {
		if (CHECK_ASK(ppc, Q_CTO_LAYOUT,
				"invalid pmemcto.layout: %s.|Do you want to set pmemcto.layout to empty string?"))
			goto error;
	}
#endif

	if (ppc->pool->hdr.cto.consistent == 0) {
		if (CHECK_ASK(ppc, Q_CTO_CONSISTENT,
				"pmemcto.consistent flag is not set.|Do you want to set pmemcto.consistent flag?"))
			goto error;
	}

	if (ppc->pool->hdr.cto.addr == NULL) {
		if (CHECK_ASK(ppc, Q_CTO_ADDR,
				"invalid pmemcto.addr: %p.|Do you want to recover pmemcto.addr?",
				ppc->pool->hdr.cto.addr))
			goto error;
	}

	if (ppc->pool->hdr.cto.size < PMEMCTO_MIN_POOL) {
		CHECK_INFO(ppc,
			"pmemcto.size is less than minimum: %zu < %zu.",
			ppc->pool->hdr.cto.size,
			PMEMCTO_MIN_POOL);
	}

	if (ppc->pool->hdr.cto.size != ppc->pool->params.size) {
		if (CHECK_ASK(ppc, Q_CTO_SIZE,
				"pmemcto.size is different than pool size: %zu != %zu.|Do you want to set pmemlog.size to the actual pool size?",
				ppc->pool->hdr.cto.size,
				ppc->pool->params.size))
			goto error;
	}

	char *valid_addr_begin =
		(char *)ppc->pool->hdr.cto.addr + CTO_DSC_SIZE_ALIGNED;
	char *valid_addr_end =
		(char *)ppc->pool->hdr.cto.addr + ppc->pool->hdr.cto.size;

	if (ppc->pool->hdr.cto.root != NULL &&
	    ((char *)ppc->pool->hdr.cto.root < valid_addr_begin ||
	    (char *)ppc->pool->hdr.cto.root >= valid_addr_end)) {
		if (CHECK_ASK(ppc, Q_CTO_ROOT,
				"invalid pmemcto.root: %p.|Do you want to recover pmemcto.root?",
				ppc->pool->hdr.cto.root))
			goto error;
	}

	if (ppc->result == CHECK_RESULT_CONSISTENT ||
		ppc->result == CHECK_RESULT_REPAIRED)
		CHECK_INFO(ppc, "pmemcto header correct");

	return check_questions_sequence_validate(ppc);

error:
	ppc->result = CHECK_RESULT_NOT_CONSISTENT;
	check_end(ppc->data);
	return -1;
}

/*
 * cto_hdr_fix -- (internal) fix pmemcto header
 */
static int
cto_hdr_fix(PMEMpoolcheck *ppc, location *loc, uint32_t question, void *ctx)
{
	LOG(3, NULL);

	switch (question) {
#if 0
	case Q_CTO_CHECKSUM:
		CHECK_INFO(ppc, "resetting pmemcto.checksum");
		ppc->pool->hdr.cto.checksum = 0;
		break;
	case Q_CTO_LAYOUT:
		CHECK_INFO(ppc, "setting pmemcto.layout to %s",
				"xxx");
		memset(ppc->pool->hdr.cto.layout, 0,
				sizeof(ppc->pool->hdr.cto.layout));
		break;
#endif
	case Q_CTO_CONSISTENT:
		CHECK_INFO(ppc, "setting pmemcto.consistent flag");
		ppc->pool->hdr.cto.consistent = 1;
		break;
	case Q_CTO_ADDR:
		CHECK_INFO(ppc, "recovering pmemcto.addr");
		ppc->pool->hdr.cto.addr = 0;
		break;
	case Q_CTO_SIZE:
		CHECK_INFO(ppc,
				"setting pmemcto.size to the actual pool size %zu",
				ppc->pool->params.size);
		ppc->pool->hdr.cto.size = ppc->pool->params.size;
		break;
	case Q_CTO_ROOT:
		CHECK_INFO(ppc, "recovering pmemcto.root pointer");
		ppc->pool->hdr.cto.root = 0;
		break;
	default:
		ERR("not implemented question id: %u", question);
	}

	return 0;
}

struct step {
	int (*check)(PMEMpoolcheck *, location *);
	int (*fix)(PMEMpoolcheck *, location *, uint32_t, void *);
	enum pool_type type;
};

static const struct step steps[] = {
	{
		.check	= cto_hdr_check,
		.type	= POOL_TYPE_CTO
	},
	{
		.fix	= cto_hdr_fix,
		.type	= POOL_TYPE_CTO
	},
	{
		.check	= NULL,
		.fix	= NULL,
	},
};

/*
 * step_exe -- (internal) perform single step according to its parameters
 */
static inline int
step_exe(PMEMpoolcheck *ppc, location *loc)
{
	ASSERT(loc->step < ARRAY_SIZE(steps));
	ASSERTeq(ppc->pool->params.type, POOL_TYPE_CTO);

	const struct step *step = &steps[loc->step++];

	if (!(step->type & ppc->pool->params.type))
		return 0;

	if (!step->fix)
		return step->check(ppc, loc);

	if (cto_read(ppc)) {
		ppc->result = CHECK_RESULT_ERROR;
		return -1;
	}

	return check_answer_loop(ppc, loc, NULL, step->fix);
}

/*
 * check_ctok -- entry point for pmemcto checks
 */
void
check_cto(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);

	location *loc = check_get_step_data(ppc->data);

	/* do all checks */
	while (CHECK_NOT_COMPLETE(loc, steps)) {
		if (step_exe(ppc, loc))
			break;
	}
}
