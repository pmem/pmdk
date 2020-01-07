// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * rpmemd_db.c -- rpmemd database of pool set files
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/mman.h>

#include "queue.h"
#include "set.h"
#include "os.h"
#include "out.h"
#include "file.h"
#include "sys_util.h"

#include "librpmem.h"
#include "rpmemd_db.h"
#include "rpmemd_log.h"

/*
 * struct rpmemd_db -- pool set database structure
 */
struct rpmemd_db {
	os_mutex_t lock;
	char *root_dir;
	mode_t mode;
};

/*
 * declaration of the 'struct list_head' type
 */
PMDK_LIST_HEAD(list_head, rpmemd_db_entry);

/*
 * struct rpmemd_db_entry -- entry in the pool set list
 */
struct rpmemd_db_entry {
	PMDK_LIST_ENTRY(rpmemd_db_entry) next;
	char *pool_desc;
	struct pool_set *set;
};

/*
 * rpmemd_db_init -- initialize the rpmem database of pool set files
 */
struct rpmemd_db *
rpmemd_db_init(const char *root_dir, mode_t mode)
{
	if (root_dir[0] != '/') {
		RPMEMD_LOG(ERR, "root directory is not an absolute path"
				" -- '%s'", root_dir);
		errno = EINVAL;
		return NULL;
	}
	struct rpmemd_db *db = calloc(1, sizeof(*db));
	if (!db) {
		RPMEMD_LOG(ERR, "!allocating the rpmem database structure");
		return NULL;
	}

	db->root_dir = strdup(root_dir);
	if (!db->root_dir) {
		RPMEMD_LOG(ERR, "!allocating the root dir path");
		free(db);
		return NULL;
	}

	db->mode = mode;

	util_mutex_init(&db->lock);

	return db;
}

/*
 * rpmemd_db_concat -- (internal) concatenate two paths
 */
static char *
rpmemd_db_concat(const char *path1, const char *path2)
{
	size_t len1 = strlen(path1);
	size_t len2 = strlen(path2);
	size_t new_len = len1 + len2 + 2; /* +1 for '/' in snprintf() */

	if (path1[0] != '/') {
		RPMEMD_LOG(ERR, "the first path is not an absolute one -- '%s'",
				path1);
		errno = EINVAL;
		return NULL;
	}
	if (path2[0] == '/') {
		RPMEMD_LOG(ERR, "the second path is not a relative one -- '%s'",
				path2);
		/* set to EBADF to distinguish this case from other errors */
		errno = EBADF;
		return NULL;
	}

	char *new_str = malloc(new_len);
	if (new_str == NULL) {
		RPMEMD_LOG(ERR, "!allocating path buffer");
		return NULL;
	}

	int ret = snprintf(new_str, new_len, "%s/%s", path1, path2);
	if (ret < 0 || (size_t)ret != new_len - 1) {
		RPMEMD_LOG(ERR, "snprintf error: %d", ret);
		free(new_str);
		errno = EINVAL;
		return NULL;
	}

	return new_str;
}

/*
 * rpmemd_db_get_path -- (internal) get the full path of the pool set file
 */
static char *
rpmemd_db_get_path(struct rpmemd_db *db, const char *pool_desc)
{
	return rpmemd_db_concat(db->root_dir, pool_desc);
}

/*
 * rpmemd_db_pool_madvise -- (internal) workaround device dax alignment issue
 */
static int
rpmemd_db_pool_madvise(struct pool_set *set)
{
	/*
	 * This is a workaround for an issue with using device dax with
	 * libibverbs. The problem is that we use ibv_fork_init(3) which
	 * makes all registered memory being madvised with MADV_DONTFORK
	 * flag. In libpmemobj the remote replication is performed without
	 * pool header (first 4k). In such case the address passed to
	 * madvise(2) is aligned to 4k, but device dax can require different
	 * alignment (default is 2MB). This workaround madvises the entire
	 * memory region before registering it by ibv_reg_mr(3).
	 */
	const struct pool_set_part *part = &set->replica[0]->part[0];
	if (part->is_dev_dax) {
		int ret = os_madvise(part->addr, part->filesize,
			MADV_DONTFORK);
		if (ret) {
			ERR("!madvise");
			return -1;
		}
	}
	return 0;
}

/*
 * rpmemd_get_attr -- (internal) get pool attributes from remote pool attributes
 */
static void
rpmemd_get_attr(struct pool_attr *attr, const struct rpmem_pool_attr *rattr)
{
	LOG(3, "attr %p, rattr %p", attr, rattr);
	memcpy(attr->signature, rattr->signature, POOL_HDR_SIG_LEN);
	attr->major = rattr->major;
	attr->features.compat = rattr->compat_features;
	attr->features.incompat = rattr->incompat_features;
	attr->features.ro_compat = rattr->ro_compat_features;
	memcpy(attr->poolset_uuid, rattr->poolset_uuid, POOL_HDR_UUID_LEN);
	memcpy(attr->first_part_uuid, rattr->uuid, POOL_HDR_UUID_LEN);
	memcpy(attr->prev_repl_uuid, rattr->prev_uuid, POOL_HDR_UUID_LEN);
	memcpy(attr->next_repl_uuid, rattr->next_uuid, POOL_HDR_UUID_LEN);
	memcpy(attr->arch_flags, rattr->user_flags, POOL_HDR_ARCH_LEN);
}

/*
 * rpmemd_db_pool_create -- create a new pool set
 */
struct rpmemd_db_pool *
rpmemd_db_pool_create(struct rpmemd_db *db, const char *pool_desc,
			size_t pool_size, const struct rpmem_pool_attr *rattr)
{
	RPMEMD_ASSERT(db != NULL);

	util_mutex_lock(&db->lock);

	struct rpmemd_db_pool *prp = NULL;
	struct pool_set *set;
	char *path;
	int ret;

	prp = malloc(sizeof(struct rpmemd_db_pool));
	if (!prp) {
		RPMEMD_LOG(ERR, "!allocating pool set db entry");
		goto err_unlock;
	}

	path = rpmemd_db_get_path(db, pool_desc);
	if (!path) {
		goto err_free_prp;
	}

	struct pool_attr attr;
	struct pool_attr *pattr = NULL;
	if (rattr != NULL) {
		rpmemd_get_attr(&attr, rattr);
		pattr = &attr;
	}

	ret = util_pool_create_uuids(&set, path, 0, RPMEM_MIN_POOL,
			RPMEM_MIN_PART, pattr, NULL, REPLICAS_DISABLED,
			POOL_REMOTE);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot create pool set -- '%s'", path);
		goto err_free_path;
	}

	ret = util_poolset_chmod(set, db->mode);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot change pool set mode bits to 0%o",
				db->mode);
	}

	if (rpmemd_db_pool_madvise(set))
		goto err_poolset_close;

	/* mark as opened */
	prp->pool_addr = set->replica[0]->part[0].addr;
	prp->pool_size = set->poolsize;
	prp->set = set;

	free(path);
	util_mutex_unlock(&db->lock);

	return prp;

err_poolset_close:
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
err_free_path:
	free(path);
err_free_prp:
	free(prp);
err_unlock:
	util_mutex_unlock(&db->lock);
	return NULL;
}

/*
 * rpmemd_db_pool_open -- open a pool set
 */
struct rpmemd_db_pool *
rpmemd_db_pool_open(struct rpmemd_db *db, const char *pool_desc,
			size_t pool_size, struct rpmem_pool_attr *rattr)
{
	RPMEMD_ASSERT(db != NULL);
	RPMEMD_ASSERT(rattr != NULL);

	util_mutex_lock(&db->lock);

	struct rpmemd_db_pool *prp = NULL;
	struct pool_set *set;
	char *path;
	int ret;

	prp = malloc(sizeof(struct rpmemd_db_pool));
	if (!prp) {
		RPMEMD_LOG(ERR, "!allocating pool set db entry");
		goto err_unlock;
	}

	path = rpmemd_db_get_path(db, pool_desc);
	if (!path) {
		goto err_free_prp;
	}

	ret = util_pool_open_remote(&set, path, 0, RPMEM_MIN_PART, rattr);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot open pool set -- '%s'", path);
		goto err_free_path;
	}

	if (rpmemd_db_pool_madvise(set))
		goto err_poolset_close;

	/* mark as opened */
	prp->pool_addr = set->replica[0]->part[0].addr;
	prp->pool_size = set->poolsize;
	prp->set = set;

	free(path);
	util_mutex_unlock(&db->lock);

	return prp;

err_poolset_close:
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
err_free_path:
	free(path);
err_free_prp:
	free(prp);
err_unlock:
	util_mutex_unlock(&db->lock);
	return NULL;
}

/*
 * rpmemd_db_pool_close -- close a pool set
 */
void
rpmemd_db_pool_close(struct rpmemd_db *db, struct rpmemd_db_pool *prp)
{
	RPMEMD_ASSERT(db != NULL);

	util_mutex_lock(&db->lock);

	util_poolset_close(prp->set, DO_NOT_DELETE_PARTS);
	free(prp);

	util_mutex_unlock(&db->lock);
}

/*
 * rpmemd_db_pool_set_attr -- overwrite pool attributes
 */
int
rpmemd_db_pool_set_attr(struct rpmemd_db_pool *prp,
	const struct rpmem_pool_attr *rattr)
{
	RPMEMD_ASSERT(prp != NULL);
	RPMEMD_ASSERT(prp->set != NULL);
	RPMEMD_ASSERT(prp->set->nreplicas == 1);

	return util_replica_set_attr(prp->set->replica[0], rattr);
}

struct rm_cb_args {
	int force;
	int ret;
};

/*
 * rm_poolset_cb -- (internal) callback for removing part files
 */
static int
rm_poolset_cb(struct part_file *pf, void *arg)
{
	struct rm_cb_args *args = (struct rm_cb_args *)arg;
	if (pf->is_remote) {
		RPMEMD_LOG(ERR, "removing remote replica not supported");
		return -1;
	}

	int ret = util_unlink_flock(pf->part->path);
	if (!args->force && ret) {
		RPMEMD_LOG(ERR, "!unlink -- '%s'", pf->part->path);
		args->ret = ret;
	}

	return 0;
}

/*
 * rpmemd_db_pool_remove -- remove a pool set
 */
int
rpmemd_db_pool_remove(struct rpmemd_db *db, const char *pool_desc,
	int force, int pool_set)
{
	RPMEMD_ASSERT(db != NULL);
	RPMEMD_ASSERT(pool_desc != NULL);

	util_mutex_lock(&db->lock);

	struct rm_cb_args args;
	args.force = force;
	args.ret = 0;
	char *path;

	path = rpmemd_db_get_path(db, pool_desc);
	if (!path) {
		args.ret = -1;
		goto err_unlock;
	}

	int ret = util_poolset_foreach_part(path, rm_poolset_cb, &args);
	if (!force && ret) {
		RPMEMD_LOG(ERR, "!removing '%s' failed", path);
		args.ret = ret;
		goto err_free_path;
	}

	if (pool_set)
		os_unlink(path);

err_free_path:
	free(path);
err_unlock:
	util_mutex_unlock(&db->lock);
	return args.ret;
}

/*
 * rpmemd_db_fini -- deinitialize the rpmem database of pool set files
 */
void
rpmemd_db_fini(struct rpmemd_db *db)
{
	RPMEMD_ASSERT(db != NULL);

	util_mutex_destroy(&db->lock);
	free(db->root_dir);
	free(db);
}

/*
 * rpmemd_db_check_dups_set -- (internal) check for duplicates in the database
 */
static inline int
rpmemd_db_check_dups_set(struct pool_set *set, const char *path)
{
	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		for (unsigned p = 0; p < rep->nparts; p++) {
			if (strcmp(path, rep->part[p].path) == 0)
				return -1;
		}
	}
	return 0;
}

/*
 * rpmemd_db_check_dups -- (internal) check for duplicates in the database
 */
static int
rpmemd_db_check_dups(struct list_head *head, struct rpmemd_db *db,
			const char *pool_desc, struct pool_set *set)
{
	struct rpmemd_db_entry *edb;

	PMDK_LIST_FOREACH(edb, head, next) {
		for (unsigned r = 0; r < edb->set->nreplicas; r++) {
			struct pool_replica *rep = edb->set->replica[r];
			for (unsigned p = 0; p < rep->nparts; p++) {
				if (rpmemd_db_check_dups_set(set,
							rep->part[p].path)) {
					RPMEMD_LOG(ERR, "part file '%s' from "
						"pool set '%s' duplicated in "
						"pool set '%s'",
						rep->part[p].path,
						pool_desc,
						edb->pool_desc);
					errno = EEXIST;
					return -1;
				}

			}
		}
	}
	return 0;
}

/*
 * rpmemd_db_add -- (internal) add an entry for a given set to the database
 */
static struct rpmemd_db_entry *
rpmemd_db_add(struct list_head *head, struct rpmemd_db *db,
			const char *pool_desc, struct pool_set *set)
{
	struct rpmemd_db_entry *edb;

	edb = calloc(1, sizeof(*edb));
	if (!edb) {
		RPMEMD_LOG(ERR, "!allocating database entry");
		goto err_calloc;
	}

	edb->set = set;
	edb->pool_desc = strdup(pool_desc);
	if (!edb->pool_desc) {
		RPMEMD_LOG(ERR, "!allocating path for database entry");
		goto err_strdup;
	}

	PMDK_LIST_INSERT_HEAD(head, edb, next);

	return edb;

err_strdup:
	free(edb);
err_calloc:
	return NULL;
}

/*
 * new_paths -- (internal) create two new paths
 */
static int
new_paths(const char *dir, const char *name, const char *old_desc,
		char **path, char **new_desc)
{
	*path = rpmemd_db_concat(dir, name);
	if (!(*path))
		return -1;

	if (old_desc[0] != 0)
		*new_desc = rpmemd_db_concat(old_desc, name);
	else {
		*new_desc = strdup(name);
		if (!(*new_desc)) {
			RPMEMD_LOG(ERR, "!allocating new descriptor");
		}
	}
	if (!(*new_desc)) {
		free(*path);
		return -1;
	}
	return 0;
}

/*
 * rpmemd_db_check_dir_r -- (internal) recursively check given directory
 *                          for duplicates
 */
static int
rpmemd_db_check_dir_r(struct list_head *head, struct rpmemd_db *db,
			const char *dir, char *pool_desc)
{
	char *new_dir, *new_desc, *full_path;
	struct dirent *dentry;
	struct pool_set *set = NULL;
	DIR *dirp;
	int ret = 0;

	dirp = opendir(dir);
	if (dirp == NULL) {
		RPMEMD_LOG(ERR, "cannot open the directory -- %s", dir);
		return -1;
	}

	while ((dentry = readdir(dirp)) != NULL) {
		if (strcmp(dentry->d_name, ".") == 0 ||
		    strcmp(dentry->d_name, "..") == 0)
			continue;

		if (dentry->d_type == DT_DIR) { /* directory */
			if (new_paths(dir, dentry->d_name, pool_desc,
					&new_dir, &new_desc))
				goto err_closedir;

			/* call recursively for a new directory */
			ret = rpmemd_db_check_dir_r(head, db, new_dir,
							new_desc);
			free(new_dir);
			free(new_desc);
			if (ret)
				goto err_closedir;
			continue;

		}

		if (new_paths(dir, dentry->d_name, pool_desc,
				&full_path, &new_desc)) {
			goto err_closedir;
		}
		if (util_poolset_read(&set, full_path)) {
			RPMEMD_LOG(ERR, "!error reading pool set file -- %s",
					full_path);
			goto err_free_paths;
		}
		if (rpmemd_db_check_dups(head, db, new_desc, set)) {
			RPMEMD_LOG(ERR, "!duplicate found in pool set file"
					" -- %s", full_path);
			goto err_free_set;
		}
		if (rpmemd_db_add(head, db, new_desc, set) == NULL) {
			goto err_free_set;
		}

		free(new_desc);
		free(full_path);
	}

	closedir(dirp);
	return 0;

err_free_set:
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
err_free_paths:
	free(new_desc);
	free(full_path);
err_closedir:
	closedir(dirp);
	return -1;
}

/*
 * rpmemd_db_check_dir -- check given directory for duplicates
 */
int
rpmemd_db_check_dir(struct rpmemd_db *db)
{
	RPMEMD_ASSERT(db != NULL);

	util_mutex_lock(&db->lock);

	struct list_head head;
	PMDK_LIST_INIT(&head);

	int ret = rpmemd_db_check_dir_r(&head, db, db->root_dir, "");

	while (!PMDK_LIST_EMPTY(&head)) {
		struct rpmemd_db_entry *edb = PMDK_LIST_FIRST(&head);
		PMDK_LIST_REMOVE(edb, next);
		util_poolset_close(edb->set, DO_NOT_DELETE_PARTS);
		free(edb->pool_desc);
		free(edb);
	}

	util_mutex_unlock(&db->lock);

	return ret;
}

/*
 * rpmemd_db_pool_is_pmem -- true if pool is in PMEM
 */
int
rpmemd_db_pool_is_pmem(struct rpmemd_db_pool *pool)
{
	return REP(pool->set, 0)->is_pmem;
}
