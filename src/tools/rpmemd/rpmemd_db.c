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
 * rpmemd_db.c -- rpmemd database of pool set files
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/file.h>

#include "util.h"
#include "out.h"
#include "sys_util.h"

#include "librpmem.h"
#include "rpmemd_db.h"
#include "rpmemd_log.h"

/*
 * struct rpmemd_db_pool -- remote pool context
 */
struct rpmemd_db_pool {
	void *pool_addr;
	size_t pool_size;
	struct pool_set *set;
};

/*
 * struct rpmemd_db -- pool set database structure
 */
struct rpmemd_db {
	pthread_mutex_t lock;
	char *root_dir;
	mode_t mode;
};

/*
 * declaration of the 'struct list_head' type
 */
LIST_HEAD(list_head, rpmemd_db_entry);

/*
 * struct rpmemd_db_entry -- entry in the pool set list
 */
struct rpmemd_db_entry {
	LIST_ENTRY(rpmemd_db_entry) next;
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

	util_mutex_init(&db->lock, NULL);

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
		errno = EINVAL;
		return NULL;
	}

	char *new_str = malloc(new_len);
	if (new_str == NULL) {
		RPMEMD_LOG(ERR, "!allocating path buffer");
		return NULL;
	}

	int ret = snprintf(new_str, new_len, "%s/%s", path1, path2);
	if (ret < 0 || (size_t)ret != new_len - 1) {
		RPMEMD_LOG(ERR, "snprintf error");
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
 * rpmemd_db_pool_create -- create a new pool set
 */
struct rpmemd_db_pool *
rpmemd_db_pool_create(struct rpmemd_db *db, const char *pool_desc,
			size_t pool_size, const struct rpmem_pool_attr *attr)
{
	RPMEMD_ASSERT(db != NULL);
	RPMEMD_ASSERT(attr != NULL);

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

	ret = util_pool_create_uuids(&set, path,
					0, pool_size,
					attr->signature,
					attr->major,
					attr->compat_features,
					attr->incompat_features,
					attr->ro_compat_features,
					attr->poolset_uuid,
					attr->uuid,
					attr->prev_uuid,
					attr->next_uuid,
					attr->user_flags);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot create pool set -- '%s'", path);
		goto err_free_path;
	}

	ret = util_poolset_chmod(set, db->mode);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot change pool set mode bits to 0%o",
				db->mode);
	}

	/* mark as opened */
	prp->pool_addr = set->replica[0]->part[0].addr;
	prp->pool_size = set->poolsize;
	prp->set = set;

	free(path);
	util_mutex_unlock(&db->lock);

	return prp;

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
			size_t pool_size, struct rpmem_pool_attr *attr)
{
	RPMEMD_ASSERT(db != NULL);
	RPMEMD_ASSERT(attr != NULL);

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

	ret = util_pool_open_remote(&set, path, 0, pool_size,
					attr->signature,
					&attr->major,
					&attr->compat_features,
					&attr->incompat_features,
					&attr->ro_compat_features,
					attr->poolset_uuid,
					attr->uuid,
					attr->prev_uuid,
					attr->next_uuid,
					attr->user_flags);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot open pool set -- '%s'", path);
		goto err_free_path;
	}

	/* mark as opened */
	prp->pool_addr = set->replica[0]->part[0].addr;
	prp->pool_size = set->poolsize;
	prp->set = set;

	free(path);
	util_mutex_unlock(&db->lock);

	return prp;

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

	util_poolset_close(prp->set, 0);
	free(prp);

	util_mutex_unlock(&db->lock);
}

/*
 * rpmemd_db_pool_remove -- remove a pool set
 */
int
rpmemd_db_pool_remove(struct rpmemd_db *db, const char *pool_desc)
{
	RPMEMD_ASSERT(db != NULL);
	RPMEMD_ASSERT(pool_desc != NULL);

	util_mutex_lock(&db->lock);

	struct pool_set *set;
	char *path;
	int ret = 0;

	path = rpmemd_db_get_path(db, pool_desc);
	if (!path) {
		ret = -1;
		goto err_unlock;
	}

	ret = util_pool_open_nocheck(&set, path, 0);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot open pool set -- '%s'", path);
		goto err_free_path;
	}

	for (unsigned r = 0; r < set->nreplicas; r++) {
		for (unsigned p = 0; p < set->replica[r]->nparts; p++) {
			const char *part_file = set->replica[r]->part[p].path;
			ret = unlink(part_file);
			if (ret) {
				RPMEMD_LOG(ERR, "!unlink -- '%s'", part_file);
			}
		}
	}

	util_poolset_close(set, 1);

err_free_path:
	free(path);
err_unlock:
	util_mutex_unlock(&db->lock);
	return ret;
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

	LIST_FOREACH(edb, head, next) {
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

	LIST_INSERT_HEAD(head, edb, next);

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
	struct dirent dentry;
	struct dirent *result;
	struct pool_set *set = NULL;
	DIR *dirp;
	int ret = 0;

	dirp = opendir(dir);
	if (dirp == NULL) {
		RPMEMD_LOG(ERR, "cannot open the directory -- %s", dir);
		return -1;
	}

	while (readdir_r(dirp, &dentry, &result) == 0 && result != NULL) {
		if (strcmp(dentry.d_name, ".") == 0 ||
		    strcmp(dentry.d_name, "..") == 0)
			continue;

		if (dentry.d_type == DT_DIR) { /* directory */
			if (new_paths(dir, dentry.d_name, pool_desc,
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

		if (new_paths(dir, dentry.d_name, pool_desc,
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
	free(set);
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
	LIST_INIT(&head);

	int ret = rpmemd_db_check_dir_r(&head, db, db->root_dir, "");

	while (!LIST_EMPTY(&head)) {
		struct rpmemd_db_entry *edb = LIST_FIRST(&head);
		LIST_REMOVE(edb, next);
		free(edb->pool_desc);
		free(edb);
	}

	util_mutex_unlock(&db->lock);

	return ret;
}
