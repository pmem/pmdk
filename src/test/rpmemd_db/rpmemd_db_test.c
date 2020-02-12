// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmemd_db_test.c -- unit test for pool set database
 *
 * usage: rpmemd_db <log-file> <root_dir> <pool_desc_1> <pool_desc_2>
 */

#include "file.h"
#include "unittest.h"
#include "librpmem.h"
#include "rpmemd_db.h"
#include "rpmemd_log.h"
#include "util_pmem.h"
#include "set.h"
#include "out.h"
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define POOL_MODE 0644

#define FAILED_FUNC(func_name) \
		UT_ERR("!%s(): %s() failed", __func__, func_name);

#define FAILED_FUNC_PARAM(func_name, param) \
		UT_ERR("!%s(): %s(%s) failed", __func__, func_name, param);

#define NPOOLS_DUAL	2

#define POOL_ATTR_CREATE	0
#define POOL_ATTR_OPEN		1
#define POOL_ATTR_SET_ATTR	2

#define POOL_STATE_INITIAL	0
#define POOL_STATE_CREATED	1
#define POOL_STATE_OPENED	2
#define POOL_STATE_CLOSED	POOL_STATE_CREATED
#define POOL_STATE_REMOVED	POOL_STATE_INITIAL

/*
 * fill_rand -- fill a buffer with random values
 */
static void
fill_rand(void *addr, size_t len)
{
	unsigned char *buff = addr;

	srand(time(NULL));
	for (unsigned i = 0; i < len; i++)
		buff[i] = (rand() % ('z' - 'a')) + 'a';

}

/*
 * test_init -- test rpmemd_db_init() and rpmemd_db_fini()
 */
static int
test_init(const char *root_dir)
{
	struct rpmemd_db *db;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}
	rpmemd_db_fini(db);
	return 0;
}

/*
 * test_check_dir -- test rpmemd_db_check_dir()
 */
static int
test_check_dir(const char *root_dir)
{
	struct rpmemd_db *db;
	int ret;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}
	ret = rpmemd_db_check_dir(db);
	if (ret) {
		FAILED_FUNC("rpmemd_db_check_dir");
	}
	rpmemd_db_fini(db);
	return ret;
}

/*
 * test_create -- test rpmemd_db_pool_create()
 */
static int
test_create(const char *root_dir, const char *pool_desc)
{
	struct rpmem_pool_attr attr;
	memset(&attr, 0, sizeof(attr));
	attr.incompat_features = 2;
	struct rpmemd_db_pool *prp;
	struct rpmemd_db *db;
	int ret = -1;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}
	prp = rpmemd_db_pool_create(db, pool_desc, 0, &attr);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_create");
		goto fini;
	}
	rpmemd_db_pool_close(db, prp);
	ret = rpmemd_db_pool_remove(db, pool_desc, 0, 0);
	if (ret) {
		FAILED_FUNC("rpmemd_db_pool_remove");
	}
fini:
	rpmemd_db_fini(db);
	return ret;
}

/*
 * test_create_dual -- dual test for rpmemd_db_pool_create()
 */
static int
test_create_dual(const char *root_dir, const char *pool_desc_1,
			const char *pool_desc_2)
{
	struct rpmem_pool_attr attr1;
	memset(&attr1, 0, sizeof(attr1));
	attr1.incompat_features = 2;
	struct rpmemd_db_pool *prp1, *prp2;
	struct rpmemd_db *db;
	int ret = -1;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}

	/* test dual create */
	prp1 = rpmemd_db_pool_create(db, pool_desc_1, 0, &attr1);
	if (prp1 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_create", pool_desc_1);
		goto err_create_1;
	}
	prp2 = rpmemd_db_pool_create(db, pool_desc_2, 0, &attr1);
	if (prp2 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_create", pool_desc_2);
		goto err_create_2;
	}
	rpmemd_db_pool_close(db, prp2);
	rpmemd_db_pool_close(db, prp1);

	ret = rpmemd_db_pool_remove(db, pool_desc_2, 0, 0);
	if (ret) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_remove", pool_desc_2);
		goto err_remove_2;
	}
	ret = rpmemd_db_pool_remove(db, pool_desc_1, 0, 0);
	if (ret) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_remove", pool_desc_1);
	}
	goto fini;

err_create_2:
	rpmemd_db_pool_close(db, prp1);
err_remove_2:
	rpmemd_db_pool_remove(db, pool_desc_1, 0, 0);
err_create_1:
fini:
	rpmemd_db_fini(db);
	return ret;
}

/*
 * compare_attr -- compare pool's attributes
 */
static void
compare_attr(struct rpmem_pool_attr *a1, struct rpmem_pool_attr *a2)
{
	char *msg;

	if (a1->major != a2->major) {
		msg = "major";
		goto err_mismatch;
	}
	if (a1->compat_features != a2->compat_features) {
		msg = "compat_features";
		goto err_mismatch;
	}
	if (a1->incompat_features != a2->incompat_features) {
		msg = "incompat_features";
		goto err_mismatch;
	}
	if (a1->ro_compat_features != a2->ro_compat_features) {
		msg = "ro_compat_features";
		goto err_mismatch;
	}
	if (memcmp(a1->signature, a2->signature, RPMEM_POOL_HDR_SIG_LEN)) {
		msg = "signature";
		goto err_mismatch;
	}
	if (memcmp(a1->poolset_uuid, a2->poolset_uuid,
						RPMEM_POOL_HDR_UUID_LEN)) {
		msg = "poolset_uuid";
		goto err_mismatch;
	}
	if (memcmp(a1->uuid, a2->uuid, RPMEM_POOL_HDR_UUID_LEN)) {
		msg = "uuid";
		goto err_mismatch;
	}
	if (memcmp(a1->next_uuid, a2->next_uuid, RPMEM_POOL_HDR_UUID_LEN)) {
		msg = "next_uuid";
		goto err_mismatch;
	}
	if (memcmp(a1->prev_uuid, a2->prev_uuid, RPMEM_POOL_HDR_UUID_LEN)) {
		msg = "prev_uuid";
		goto err_mismatch;
	}
	return;

err_mismatch:
	errno = EINVAL;
	UT_FATAL("%s(): pool attributes mismatch (%s)", __func__, msg);
}

/*
 * test_open -- test rpmemd_db_pool_open()
 */
static int
test_open(const char *root_dir, const char *pool_desc)
{
	struct rpmem_pool_attr attr1, attr2;
	struct rpmemd_db_pool *prp;
	struct rpmemd_db *db;
	int ret = -1;

	fill_rand(&attr1, sizeof(attr1));
	attr1.major = 1;
	attr1.incompat_features = 2;
	attr1.compat_features = 0;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}

	prp = rpmemd_db_pool_create(db, pool_desc, 0, &attr1);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_create");
		goto fini;
	}
	rpmemd_db_pool_close(db, prp);

	prp = rpmemd_db_pool_open(db, pool_desc, 0, &attr2);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_open");
		goto fini;
	}
	rpmemd_db_pool_close(db, prp);

	compare_attr(&attr1, &attr2);

	ret = rpmemd_db_pool_remove(db, pool_desc, 0, 0);
	if (ret) {
		FAILED_FUNC("rpmemd_db_pool_remove");
	}
fini:
	rpmemd_db_fini(db);
	return ret;
}

/*
 * test_open_dual -- dual test for rpmemd_db_pool_open()
 */
static int
test_open_dual(const char *root_dir, const char *pool_desc_1,
		const char *pool_desc_2)
{
	struct rpmem_pool_attr attr1a, attr2a, attr1b, attr2b;
	struct rpmemd_db_pool *prp1, *prp2;
	struct rpmemd_db *db;
	int ret = -1;

	fill_rand(&attr1a, sizeof(attr1a));
	fill_rand(&attr1b, sizeof(attr1b));
	attr1a.major = 1;
	attr1a.incompat_features = 2;
	attr1a.compat_features = 0;
	attr1b.major = 1;
	attr1b.incompat_features = 2;
	attr1b.compat_features = 0;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}

	prp1 = rpmemd_db_pool_create(db, pool_desc_1, 0, &attr1a);
	if (prp1 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_create", pool_desc_1);
		goto err_create_1;
	}
	rpmemd_db_pool_close(db, prp1);
	prp2 = rpmemd_db_pool_create(db, pool_desc_2, 0, &attr1b);
	if (prp2 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_create", pool_desc_2);
		goto err_create_2;
	}
	rpmemd_db_pool_close(db, prp2);

	/* test dual open */
	prp1 = rpmemd_db_pool_open(db, pool_desc_1, 0, &attr2a);
	if (prp1 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_open", pool_desc_1);
		goto err_open_1;
	}
	prp2 = rpmemd_db_pool_open(db, pool_desc_2, 0, &attr2b);
	if (prp2 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_open", pool_desc_2);
		goto err_open_2;
	}
	rpmemd_db_pool_close(db, prp1);
	rpmemd_db_pool_close(db, prp2);

	compare_attr(&attr1a, &attr2a);
	compare_attr(&attr1b, &attr2b);

	ret = rpmemd_db_pool_remove(db, pool_desc_2, 0, 0);
	if (ret) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_remove", pool_desc_2);
		goto err_remove_2;
	}
	ret = rpmemd_db_pool_remove(db, pool_desc_1, 0, 0);
	if (ret) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_remove", pool_desc_1);
	}
	goto fini;

err_open_2:
	rpmemd_db_pool_close(db, prp1);
err_open_1:
	rpmemd_db_pool_remove(db, pool_desc_2, 0, 0);
err_create_2:
err_remove_2:
	rpmemd_db_pool_remove(db, pool_desc_1, 0, 0);
err_create_1:
fini:
	rpmemd_db_fini(db);
	return ret;
}

/*
 * test_set_attr -- test rpmemd_db_pool_set_attr()
 */
static int
test_set_attr(const char *root_dir, const char *pool_desc)
{
	struct rpmem_pool_attr attr[3];
	struct rpmemd_db_pool *prp;
	struct rpmemd_db *db;
	int ret = -1;

	fill_rand(&attr[POOL_ATTR_CREATE], sizeof(attr[POOL_ATTR_CREATE]));
	fill_rand(&attr[POOL_ATTR_SET_ATTR], sizeof(attr[POOL_ATTR_SET_ATTR]));
	attr[POOL_ATTR_CREATE].major = 1;
	attr[POOL_ATTR_CREATE].incompat_features = 2;
	attr[POOL_ATTR_CREATE].compat_features = 0;
	attr[POOL_ATTR_SET_ATTR].major = 1;
	attr[POOL_ATTR_SET_ATTR].incompat_features = 2;
	attr[POOL_ATTR_SET_ATTR].compat_features = 0;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}

	prp = rpmemd_db_pool_create(db, pool_desc, 0, &attr[POOL_ATTR_CREATE]);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_create");
		goto err_create;
	}
	rpmemd_db_pool_close(db, prp);

	prp = rpmemd_db_pool_open(db, pool_desc, 0, &attr[POOL_ATTR_OPEN]);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_open");
		goto err_open;
	}
	compare_attr(&attr[POOL_ATTR_CREATE], &attr[POOL_ATTR_OPEN]);

	ret = rpmemd_db_pool_set_attr(prp, &attr[POOL_ATTR_SET_ATTR]);
	if (ret) {
		FAILED_FUNC("rpmemd_db_pool_set_attr");
		goto err_set_attr;
	}

	rpmemd_db_pool_close(db, prp);

	prp = rpmemd_db_pool_open(db, pool_desc, 0, &attr[POOL_ATTR_OPEN]);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_open");
		goto err_open;
	}
	compare_attr(&attr[POOL_ATTR_SET_ATTR], &attr[POOL_ATTR_OPEN]);

	rpmemd_db_pool_close(db, prp);

	ret = rpmemd_db_pool_remove(db, pool_desc, 0, 0);
	if (ret) {
		FAILED_FUNC("rpmemd_db_pool_remove");
	}
	goto fini;

err_set_attr:
	rpmemd_db_pool_close(db, prp);
err_open:
	rpmemd_db_pool_remove(db, pool_desc, 0, 0);
err_create:
fini:
	rpmemd_db_fini(db);
	return ret;
}

/*
 * test_set_attr_dual -- dual test for rpmemd_db_pool_set_attr()
 */
static int
test_set_attr_dual(const char *root_dir, const char *pool_desc_1,
		const char *pool_desc_2)
{
	struct rpmem_pool_attr attr[NPOOLS_DUAL][3];
	struct rpmemd_db_pool *prp[NPOOLS_DUAL];
	const char *pool_desc[NPOOLS_DUAL] = {pool_desc_1, pool_desc_2};
	unsigned pool_state[NPOOLS_DUAL] = {POOL_STATE_INITIAL};
	struct rpmemd_db *db;
	int ret = -1;

	/* initialize rpmem database */
	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}

	for (unsigned p = 0; p < NPOOLS_DUAL; ++p) {
		/*
		 * generate random pool attributes for create and set
		 * attributes operations
		 */
		fill_rand(&attr[p][POOL_ATTR_CREATE],
			sizeof(attr[p][POOL_ATTR_CREATE]));
		fill_rand(&attr[p][POOL_ATTR_SET_ATTR],
			sizeof(attr[p][POOL_ATTR_SET_ATTR]));

		attr[p][POOL_ATTR_CREATE].major = 1;
		attr[p][POOL_ATTR_CREATE].incompat_features = 2;
		attr[p][POOL_ATTR_CREATE].compat_features = 0;
		attr[p][POOL_ATTR_SET_ATTR].major = 1;
		attr[p][POOL_ATTR_SET_ATTR].incompat_features = 2;
		attr[p][POOL_ATTR_SET_ATTR].compat_features = 0;

		/* create pool */
		prp[p] = rpmemd_db_pool_create(db, pool_desc[p], 0,
			&attr[p][POOL_ATTR_CREATE]);
		if (prp[p] == NULL) {
			FAILED_FUNC_PARAM("rpmemd_db_pool_create",
				pool_desc[p]);
			goto err;
		}
		rpmemd_db_pool_close(db, prp[p]);
		pool_state[p] = POOL_STATE_CREATED;
	}

	/* open pools and check pool attributes */
	for (unsigned p = 0; p < NPOOLS_DUAL; ++p) {
		prp[p] = rpmemd_db_pool_open(db, pool_desc[p], 0,
			&attr[p][POOL_ATTR_OPEN]);
		if (prp[p] == NULL) {
			FAILED_FUNC_PARAM("rpmemd_db_pool_open", pool_desc[p]);
			goto err;
		}
		pool_state[p] = POOL_STATE_OPENED;
		compare_attr(&attr[p][POOL_ATTR_CREATE],
			&attr[p][POOL_ATTR_OPEN]);
	}

	/* set attributes and close pools */
	for (unsigned p = 0; p < NPOOLS_DUAL; ++p) {
		ret = rpmemd_db_pool_set_attr(prp[p],
			&attr[p][POOL_ATTR_SET_ATTR]);
		if (ret) {
			FAILED_FUNC_PARAM("rpmemd_db_pool_set_attr",
				pool_desc[p]);
			goto err;
		}
		rpmemd_db_pool_close(db, prp[p]);
		pool_state[p] = POOL_STATE_CLOSED;
	}

	/* open pools and check attributes */
	for (unsigned p = 0; p < NPOOLS_DUAL; ++p) {
		prp[p] = rpmemd_db_pool_open(db, pool_desc[p], 0,
			&attr[p][POOL_ATTR_OPEN]);
		if (prp[p] == NULL) {
			FAILED_FUNC_PARAM("rpmemd_db_pool_open", pool_desc[p]);
			goto err;
		}
		pool_state[p] = POOL_STATE_OPENED;
		compare_attr(&attr[p][POOL_ATTR_SET_ATTR],
			&attr[p][POOL_ATTR_OPEN]);
	}

err:
	for (unsigned p = 0; p < NPOOLS_DUAL; ++p) {
		if (pool_state[p] == POOL_STATE_OPENED) {
			rpmemd_db_pool_close(db, prp[p]);
			pool_state[p] = POOL_STATE_CLOSED;
		}
		if (pool_state[p] == POOL_STATE_CREATED) {
			ret = rpmemd_db_pool_remove(db, pool_desc[p], 0, 0);
			if (ret) {
				FAILED_FUNC_PARAM("rpmemd_db_pool_remove",
					pool_desc[p]);
			}
			pool_state[p] = POOL_STATE_REMOVED;
		}
	}

	rpmemd_db_fini(db);
	return ret;
}

static int
exists_cb(struct part_file *pf, void *arg)
{
	return util_file_exists(pf->part->path);
}

static int
noexists_cb(struct part_file *pf, void *arg)
{
	int exists = util_file_exists(pf->part->path);
	if (exists < 0)
		return -1;
	else
		return !exists;
}

/*
 * test_remove -- test for rpmemd_db_pool_remove()
 */
static void
test_remove(const char *root_dir, const char *pool_desc)
{
	struct rpmem_pool_attr attr;
	struct rpmemd_db_pool *prp;
	struct rpmemd_db *db;
	int ret;
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s/%s", root_dir, pool_desc);

	fill_rand(&attr, sizeof(attr));
	strncpy((char *)attr.poolset_uuid, "TEST", sizeof(attr.poolset_uuid));
	attr.incompat_features = 2;
	attr.compat_features = 0;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	UT_ASSERTne(db, NULL);

	prp = rpmemd_db_pool_create(db, pool_desc, 0, &attr);
	UT_ASSERTne(prp, NULL);
	rpmemd_db_pool_close(db, prp);

	ret = util_poolset_foreach_part(path, exists_cb, NULL);
	UT_ASSERTeq(ret, 1);

	ret = rpmemd_db_pool_remove(db, pool_desc, 0, 0);
	UT_ASSERTeq(ret, 0);

	ret = util_poolset_foreach_part(path, noexists_cb, NULL);
	UT_ASSERTeq(ret, 1);

	prp = rpmemd_db_pool_create(db, pool_desc, 0, &attr);
	UT_ASSERTne(prp, NULL);
	rpmemd_db_pool_close(db, prp);

	ret = rpmemd_db_pool_remove(db, pool_desc, 0, 1);
	UT_ASSERTeq(ret, 0);

	ret = util_file_exists(path);
	UT_ASSERTne(ret, 1);

	rpmemd_db_fini(db);
}

int
main(int argc, char *argv[])
{
	char *pool_desc[2], *log_file;
	char root_dir[PATH_MAX];

	START(argc, argv, "rpmemd_db");

	util_init();
	out_init("rpmemd_db", "RPMEM_LOG_LEVEL", "RPMEM_LOG_FILE", 0, 0);

	if (argc != 5)
		UT_FATAL("usage: %s <log-file> <root_dir> <pool_desc_1>"
				" <pool_desc_2>", argv[0]);

	log_file = argv[1];
	if (realpath(argv[2], root_dir) == NULL)
		UT_FATAL("!realpath(%s)", argv[1]);

	pool_desc[0] = argv[3];
	pool_desc[1] = argv[4];

	if (rpmemd_log_init("rpmemd error: ", log_file, 0))
		FAILED_FUNC("rpmemd_log_init");

	test_init(root_dir);
	test_check_dir(root_dir);
	test_create(root_dir, pool_desc[0]);
	test_create_dual(root_dir, pool_desc[0], pool_desc[1]);
	test_open(root_dir, pool_desc[0]);
	test_open_dual(root_dir, pool_desc[0], pool_desc[1]);
	test_set_attr(root_dir, pool_desc[0]);
	test_set_attr_dual(root_dir, pool_desc[0], pool_desc[1]);
	test_remove(root_dir, pool_desc[0]);

	rpmemd_log_close();

	out_fini();
	DONE(NULL);
}
