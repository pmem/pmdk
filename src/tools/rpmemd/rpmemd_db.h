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
 * rpmemd_db.h -- internal definitions for rpmemd database of pool set files
 */

struct rpmemd_db;
struct rpmem_pool_attr;

struct rpmemd_db *rpmemd_db_init(const char *root_dir, mode_t mode);
struct rpmemd_db_pool *rpmemd_db_pool_create(struct rpmemd_db *db,
	const char *pool_desc, size_t pool_size,
	const struct rpmem_pool_attr *attr);
struct rpmemd_db_pool *rpmemd_db_pool_open(struct rpmemd_db *db,
	const char *pool_desc, size_t pool_size, struct rpmem_pool_attr *attr);
int rpmemd_db_pool_remove(struct rpmemd_db *db, const char *pool_desc);
void rpmemd_db_pool_close(struct rpmemd_db *db, struct rpmemd_db_pool *prp);
void rpmemd_db_fini(struct rpmemd_db *db);
int rpmemd_db_check_dir(struct rpmemd_db *db);
