/*
 * Copyright 2016-2019, Intel Corporation
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
 * tx.h -- internal definitions for transactions
 */

#ifndef LIBPMEMOBJ_INTERNAL_TX_H
#define LIBPMEMOBJ_INTERNAL_TX_H 1

#include <stdint.h>
#include "obj.h"
#include "ulog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TX_DEFAULT_RANGE_CACHE_SIZE (1 << 15)
#define TX_DEFAULT_RANGE_CACHE_THRESHOLD (1 << 12)

#define TX_RANGE_MASK (8ULL - 1)
#define TX_RANGE_MASK_LEGACY (32ULL - 1)

#define TX_ALIGN_SIZE(s, amask) (((s) + (amask)) & ~(amask))

#define TX_SNAPSHOT_LOG_ENTRY_ALIGNMENT CACHELINE_SIZE
#define TX_SNAPSHOT_LOG_BUFFER_OVERHEAD sizeof(struct ulog)
#define TX_SNAPSHOT_LOG_ENTRY_OVERHEAD sizeof(struct ulog_entry_buf)

#define TX_INTENT_LOG_BUFFER_ALIGNMENT CACHELINE_SIZE
#define TX_INTENT_LOG_BUFFER_OVERHEAD sizeof(struct ulog)
#define TX_INTENT_LOG_ENTRY_OVERHEAD sizeof(struct ulog_entry_val)

struct tx_parameters {
	size_t cache_size;
};

/*
 * Returns the current transaction's pool handle, NULL if not within
 * a transaction.
 */
PMEMobjpool *tx_get_pop(void);

void tx_ctl_register(PMEMobjpool *pop);

struct tx_parameters *tx_params_new(void);
void tx_params_delete(struct tx_parameters *tx_params);

#ifdef __cplusplus
}
#endif

#endif
