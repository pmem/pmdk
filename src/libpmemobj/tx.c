/*
 * Copyright (c) 2015, Intel Corporation
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tx.c -- transactions implementation
 */

#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdarg.h>

#include "libpmem.h"
#include "libpmemobj.h"
#include "util.h"
#include "list.h"
#include "obj.h"
#include "lane.h"
#include "out.h"

struct tx_data {
	SLIST_ENTRY(tx_data) tx_entry;
	jmp_buf env;
	int errnum;
};

static __thread struct {
	enum pobj_tx_stage stage;
	struct lane_section *section;
} tx;

struct tx_lock_data {
	union {
		PMEMmutex *mutex;
		PMEMrwlock *rwlock;
	} lock;
	enum pobj_tx_lock lock_type;
	SLIST_ENTRY(tx_lock_data) tx_lock;
};

struct lane_tx_runtime {
	PMEMobjpool *pop;
	SLIST_HEAD(txd, tx_data) tx_entries;
	SLIST_HEAD(txl, tx_lock_data) tx_locks;
};

/*
 * add_to_tx_and_lock -- (internal) add lock to the transaction and acquire it
 */
static int
add_to_tx_and_lock(struct lane_tx_runtime *lane, enum pobj_tx_lock type,
	void *lock)
{
	LOG(15, NULL);
	int retval = 0;
	struct tx_lock_data *txl;
	/* check if the lock is already on the list */
	SLIST_FOREACH(txl, &(lane->tx_locks), tx_lock) {
		if (memcmp(&txl->lock, &lock, sizeof (lock)) == 0)
			return retval;
	}

	txl = Malloc(sizeof (*txl));
	if (txl == NULL)
		return ENOMEM;

	txl->lock_type = type;
	switch (txl->lock_type) {
		case TX_LOCK_MUTEX:
			txl->lock.mutex = lock;
			retval = pmemobj_mutex_lock(lane->pop,
				txl->lock.mutex);
			break;
		case TX_LOCK_RWLOCK:
			txl->lock.rwlock = lock;
			retval = pmemobj_rwlock_wrlock(lane->pop,
				txl->lock.rwlock);
			break;
		default:
			LOG(1, "Unrecognized lock type");
			ASSERT(0);
			break;
	}

	SLIST_INSERT_HEAD(&lane->tx_locks, txl, tx_lock);

	return retval;
}

/*
 * release_and_free_tx_locks -- (internal) release and remove all locks from the
 *				transaction
 */
static void
release_and_free_tx_locks(struct lane_tx_runtime *lane)
{
	LOG(15, NULL);
	/* no locks taken */
	if (SLIST_EMPTY(&lane->tx_locks))
		return;

	while (!SLIST_EMPTY(&lane->tx_locks)) {
		struct tx_lock_data *tx_lock = SLIST_FIRST(&lane->tx_locks);
		SLIST_REMOVE_HEAD(&lane->tx_locks, tx_lock);
		switch (tx_lock->lock_type) {
			case TX_LOCK_MUTEX:
				pmemobj_mutex_unlock(lane->pop,
					tx_lock->lock.mutex);
				break;
			case TX_LOCK_RWLOCK:
				pmemobj_rwlock_unlock(lane->pop,
					tx_lock->lock.rwlock);
				break;
			default:
				LOG(1, "Unrecognized lock type");
				ASSERT(0);
				break;
		}
		Free(tx_lock);
	}
}

/*
 * pmemobj_tx_begin -- initializes new transaction
 */
int
pmemobj_tx_begin(PMEMobjpool *pop, jmp_buf env, ...)
{
	LOG(3, NULL);

	int err = 0;

	struct lane_tx_runtime *lane = NULL;
	if (tx.stage == TX_STAGE_WORK) {
		lane = tx.section->runtime;
	} else if (tx.stage == TX_STAGE_NONE) {
		if ((err = lane_hold(pop, &tx.section,
			LANE_SECTION_TRANSACTION)) != 0)
			goto err_abort;

		lane = tx.section->runtime;
		SLIST_INIT(&lane->tx_entries);
		SLIST_INIT(&lane->tx_locks);

		lane->pop = pop;
	} else {
		err = EINVAL;
		goto err_abort;
	}

	struct tx_data *txd = Malloc(sizeof (*txd));
	if (txd == NULL) {
		err = ENOMEM;
		goto err_abort;
	}

	txd->errnum = 0;
	if (env != NULL)
		memcpy(txd->env, env, sizeof (jmp_buf));
	else
		memset(txd->env, 0, sizeof (jmp_buf));

	SLIST_INSERT_HEAD(&lane->tx_entries, txd, tx_entry);

	/* handle locks */
	va_list argp;
	va_start(argp, env);
	enum pobj_tx_lock lock_type;

	while ((lock_type = va_arg(argp, enum pobj_tx_lock)) != TX_LOCK_NONE) {
		if ((err = add_to_tx_and_lock(lane,
				lock_type, va_arg(argp, void *))) != 0) {
			va_end(argp);
			goto err_abort;
		}
	}
	va_end(argp);

	tx.stage = TX_STAGE_WORK;
	ASSERT(err == 0);
	return 0;

err_abort:
	tx.stage = TX_STAGE_ONABORT;
	return err;
}

/*
 * pmemobj_tx_stage -- returns current transaction stage
 */
enum pobj_tx_stage
pmemobj_tx_stage()
{
	LOG(3, NULL);

	return tx.stage;
}

/*
 * pmemobj_tx_abort -- aborts current transaction
 */
void
pmemobj_tx_abort(int errnum)
{
	LOG(3, NULL);

	ASSERT(tx.section != NULL);
	ASSERT(tx.stage == TX_STAGE_WORK);

	/* XXX undo log */

	tx.stage = TX_STAGE_ONABORT;
	struct lane_tx_runtime *lane = tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);
	txd->errnum = errnum;
	if (!util_is_zeroed(txd->env, sizeof (jmp_buf)))
		longjmp(txd->env, errnum);
}

/*
 * pmemobj_tx_commit -- commits current transaction
 */
int
pmemobj_tx_commit()
{
	LOG(3, NULL);

	ASSERT(tx.section != NULL);
	ASSERT(tx.stage == TX_STAGE_WORK);

	/* XXX undo log */

	tx.stage = TX_STAGE_ONCOMMIT;
	return 0;
}

/*
 * pmemobj_tx_end -- ends current transaction
 */
void
pmemobj_tx_end()
{
	LOG(3, NULL);
	ASSERT(tx.stage != TX_STAGE_WORK);

	if (tx.section == NULL) {
		tx.stage = TX_STAGE_NONE;
		return;
	}

	struct lane_tx_runtime *lane = tx.section->runtime;
	struct tx_data *txd = SLIST_FIRST(&lane->tx_entries);
	SLIST_REMOVE_HEAD(&lane->tx_entries, tx_entry);
	int errnum = txd->errnum;
	Free(txd);
	if (SLIST_EMPTY(&lane->tx_entries)) {
		/* this is the outermost transaction */

		tx.stage = TX_STAGE_NONE;
		release_and_free_tx_locks(lane);
		lane_release(lane->pop);
		tx.section = NULL;
	} else {
		/* resume the next transaction */
		tx.stage = TX_STAGE_WORK;

		/* abort called within inner transaction, waterfall the error */
		if (errnum)
			pmemobj_tx_abort(errnum);
	}
}

/*
 * pmemobj_tx_process -- processes current transaction stage
 */
int
pmemobj_tx_process()
{
	LOG(3, NULL);

	ASSERT(tx.section != NULL);
	ASSERT(tx.stage != TX_STAGE_NONE);

	switch (tx.stage) {
	case TX_STAGE_NONE:
		break;
	case TX_STAGE_WORK:
		return pmemobj_tx_commit();
	case TX_STAGE_ONABORT:
	case TX_STAGE_ONCOMMIT:
		tx.stage = TX_STAGE_FINALLY;
		break;
	case TX_STAGE_FINALLY:
		tx.stage = TX_STAGE_NONE;
		break;
	case MAX_TX_STAGE:
		ASSERT(1);
	}

	return 0;
}

/*
 * pmemobj_tx_add_range -- adds persistent memory range into the transaction
 */
int
pmemobj_tx_add_range(PMEMoid oid, uint64_t hoff, size_t size)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		LOG(1, "invalid stage");
		return EINVAL;
	}

	/* XXX */
	return 0;
}

/*
 * pmemobj_tx_alloc -- allocates a new object
 */
PMEMoid
pmemobj_tx_alloc(size_t size, int type_num)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		LOG(1, "invalid stage");
		errno = EINVAL;
		return OID_NULL;
	}

	PMEMoid poid = OID_NULL;

	/* XXX */

	return poid;
}

/*
 * pmemobj_tx_zalloc -- allocates a new zeroed object
 */
PMEMoid
pmemobj_tx_zalloc(size_t size, int type_num)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		LOG(1, "invalid stage");
		errno = EINVAL;
		return OID_NULL;
	}

	PMEMoid poid = OID_NULL;

	/* XXX */

	return poid;
}

/*
 * pmemobj_tx_realloc -- resizes an existing object
 */
PMEMoid
pmemobj_tx_realloc(PMEMoid oid, size_t size, int type_num)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		LOG(1, "invalid stage");
		errno = EINVAL;
		return OID_NULL;
	}

	PMEMoid poid = OID_NULL;

	/* XXX */

	return poid;
}


/*
 * pmemobj_zrealloc -- resizes an existing object, any new space is zeroed.
 */
PMEMoid
pmemobj_tx_zrealloc(PMEMoid oid, size_t size, int type_num)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		LOG(1, "invalid stage");
		errno = EINVAL;
		return OID_NULL;
	}

	PMEMoid poid = OID_NULL;

	/* XXX */

	return poid;
}

/*
 * pmemobj_tx_strdup -- allocates a new object with duplicate of the string s.
 */
PMEMoid
pmemobj_tx_strdup(const char *s, int type_num)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		LOG(1, "invalid stage");
		errno = EINVAL;
		return OID_NULL;
	}

	PMEMoid poid = OID_NULL;

	/* XXX */

	return poid;
}

/*
 * pmemobj_tx_free -- frees an existing object
 */
int
pmemobj_tx_free(PMEMoid oid)
{
	LOG(3, NULL);

	if (tx.stage != TX_STAGE_WORK) {
		LOG(1, "invalid stage");
		return EINVAL;
	}

	/* XXX */

	return 0;
}

/*
 * lane_transaction_construct -- create transaction lane section
 */
static int
lane_transaction_construct(struct lane_section *section)
{
	section->runtime = Malloc(sizeof (struct lane_tx_runtime));
	if (section->runtime == NULL)
		return ENOMEM;

	return 0;
}

/*
 * lane_transaction_destruct -- destroy transaction lane section
 */
static int
lane_transaction_destruct(struct lane_section *section)
{
	Free(section->runtime);

	return 0;
}

/*
 * lane_transaction_recovery -- recovery of transaction lane section
 */
static int
lane_transaction_recovery(PMEMobjpool *pop,
	struct lane_section_layout *section)
{
	/* XXX */

	return 0;
}

/*
 * lane_transaction_check -- consistency check of transaction lane section
 */
static int
lane_transaction_check(PMEMobjpool *pop,
	struct lane_section_layout *section)
{
	/* XXX */

	return 0;
}

struct section_operations transaction_ops = {
	.construct = lane_transaction_construct,
	.destruct = lane_transaction_destruct,
	.recover = lane_transaction_recovery,
	.check = lane_transaction_check
};

SECTION_PARM(LANE_SECTION_TRANSACTION, &transaction_ops);
