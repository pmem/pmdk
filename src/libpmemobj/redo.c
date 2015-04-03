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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * redo.c -- redo log implementation
 */
#include <stdlib.h>
#include <stdint.h>
#include "libpmemobj.h"
#include "redo.h"

/*
 * redo_log_store -- (internal) store redo log entry at specified index
 */
void
redo_log_store(PMEMobjpool *pop, struct redo_log *redo, size_t index,
		uint64_t offset, uint64_t value)
{
	/* stub */
}

/*
 * redo_log_store_last -- (internal) store last entry at specified index
 */
void
redo_log_store_last(PMEMobjpool *pop, struct redo_log *redo, size_t index,
		uint64_t offset, uint64_t value)
{
	/* stub */
}

/*
 * redo_log_process -- (internal) process redo log entries
 */
void
redo_log_process(PMEMobjpool *pop, struct redo_log *redo,
		size_t nentries)
{
	/* stub */
}

/*
 * redo_log_recover -- (internal) recovery of redo log
 */
void
redo_log_recover(PMEMobjpool *pop, struct redo_log *redo,
		size_t nentries)
{
	/* stub */
}

/*
 * redo_log_check -- (internal) check consistency of redo log entries
 */
int
redo_log_check(PMEMobjpool *pop, struct redo_log *redo, size_t nentries)
{
	/* stub */
	return 0;
}
