/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * rpmemd_db.h -- internal definitions for rpmemd database of pool set files
 */

struct rpmemd_db;
struct rpmem_pool_attr;

/*
 * struct rpmemd_db_pool -- remote pool context
 */
struct rpmemd_db_pool {
	void *pool_addr;
	size_t pool_size;
	struct pool_set *set;
};

struct rpmemd_db *rpmemd_db_init(const char *root_dir, mode_t mode);
struct rpmemd_db_pool *rpmemd_db_pool_create(struct rpmemd_db *db,
	const char *pool_desc, size_t pool_size,
	const struct rpmem_pool_attr *rattr);
struct rpmemd_db_pool *rpmemd_db_pool_open(struct rpmemd_db *db,
	const char *pool_desc, size_t pool_size, struct rpmem_pool_attr *rattr);
int rpmemd_db_pool_remove(struct rpmemd_db *db, const char *pool_desc,
		int force, int pool_set);
int rpmemd_db_pool_set_attr(struct rpmemd_db_pool *prp,
	const struct rpmem_pool_attr *rattr);
void rpmemd_db_pool_close(struct rpmemd_db *db, struct rpmemd_db_pool *prp);
void rpmemd_db_fini(struct rpmemd_db *db);
int rpmemd_db_check_dir(struct rpmemd_db *db);
int rpmemd_db_pool_is_pmem(struct rpmemd_db_pool *pool);
