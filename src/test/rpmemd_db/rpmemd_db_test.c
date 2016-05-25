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
 * rpmemd_db_test.c -- unit test for pool set database
 *
 * usage: rpmemd_db <log-file> <root_dir> <pool_desc_1> <pool_desc_2>
 */

#include "unittest.h"
#include "librpmem.h"
#include "rpmemd_db.h"
#include "rpmemd_log.h"

#define POOL_MODE 0644

#define FAILED_FUNC(func_name) \
		UT_ERR("!%s(): %s() failed", __func__, func_name);

#define FAILED_FUNC_PARAM(func_name, param) \
		UT_ERR("!%s(): %s(%s) failed", __func__, func_name, param);

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
	ret = rpmemd_db_pool_remove(db, pool_desc);
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
	struct rpmemd_db_pool *prp1, *prp2;
	struct rpmemd_db *db;
	int ret = -1;

	memset(&attr1, 0, sizeof(attr1));
	attr1.major = 1;

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

	ret = rpmemd_db_pool_remove(db, pool_desc_2);
	if (ret) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_remove", pool_desc_2);
		goto err_remove_2;
	}
	ret = rpmemd_db_pool_remove(db, pool_desc_1);
	if (ret) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_remove", pool_desc_1);
	}
	goto fini;

err_create_2:
	rpmemd_db_pool_close(db, prp1);
err_remove_2:
	rpmemd_db_pool_remove(db, pool_desc_1);
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

	memset(&attr1, 0, sizeof(attr1));
	attr1.major = 1;

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
	compare_attr(&attr1, &attr2);
	rpmemd_db_pool_close(db, prp);
	ret = rpmemd_db_pool_remove(db, pool_desc);
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
	struct rpmem_pool_attr attr1, attr2;
	struct rpmemd_db_pool *prp1, *prp2;
	struct rpmemd_db *db;
	int ret = -1;

	memset(&attr1, 0, sizeof(attr1));
	attr1.major = 1;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
		return -1;
	}

	prp1 = rpmemd_db_pool_create(db, pool_desc_1, 0, &attr1);
	if (prp1 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_create", pool_desc_1);
		goto err_create_1;
	}
	rpmemd_db_pool_close(db, prp1);

	prp2 = rpmemd_db_pool_create(db, pool_desc_2, 0, &attr1);
	if (prp2 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_create", pool_desc_2);
		goto err_create_2;
	}
	rpmemd_db_pool_close(db, prp2);

	/* test dual open */
	prp1 = rpmemd_db_pool_open(db, pool_desc_1, 0, &attr2);
	if (prp1 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_open", pool_desc_1);
		goto err_open_1;
	}
	compare_attr(&attr1, &attr2);
	prp2 = rpmemd_db_pool_open(db, pool_desc_2, 0, &attr2);
	if (prp2 == NULL) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_open", pool_desc_2);
		goto err_open_2;
	}
	compare_attr(&attr1, &attr2);
	rpmemd_db_pool_close(db, prp1);
	rpmemd_db_pool_close(db, prp2);

	ret = rpmemd_db_pool_remove(db, pool_desc_2);
	if (ret) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_remove", pool_desc_2);
		goto err_remove_2;
	}
	ret = rpmemd_db_pool_remove(db, pool_desc_1);
	if (ret) {
		FAILED_FUNC_PARAM("rpmemd_db_pool_remove", pool_desc_1);
	}
	goto fini;

err_open_2:
	rpmemd_db_pool_close(db, prp1);
err_open_1:
	rpmemd_db_pool_remove(db, pool_desc_2);
err_create_2:
err_remove_2:
	rpmemd_db_pool_remove(db, pool_desc_1);
err_create_1:
fini:
	rpmemd_db_fini(db);
	return ret;
}

int
main(int argc, char *argv[])
{
	char *pool_desc[2], *log_file;
	char root_dir[PATH_MAX];

	START(argc, argv, "rpmemd_db");

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

	rpmemd_log_close();

	DONE(NULL);
}
