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
 * usage: rpmemd_db <root_dir> <pool_desc>
 */

#include "unittest.h"
#include "librpmem.h"
#include "rpmemd_db.h"
#include "rpmemd_log.h"

#define POOL_MODE 0644

#define FAILED_FUNC(func_name)\
	do {\
		UT_ERR("!%s(): %s() failed", __func__, func_name);\
		exit(0);\
	} while (0)

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
	}
	ret = rpmemd_db_check_dir(db);
	if (ret) {
		FAILED_FUNC("rpmemd_db_check_dir");
	}
	rpmemd_db_fini(db);
	return 0;
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
	int ret;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
	}
	prp = rpmemd_db_pool_create(db, pool_desc, 0, &attr);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_create");
	}
	rpmemd_db_pool_close(db, prp);
	ret = rpmemd_db_pool_remove(db, pool_desc);
	if (ret) {
		FAILED_FUNC("rpmemd_db_pool_remove");
	}
	rpmemd_db_fini(db);
	return 0;
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
	int ret;

	memset(&attr1, 0, sizeof(attr1));
	attr1.major = 1;

	db = rpmemd_db_init(root_dir, POOL_MODE);
	if (db == NULL) {
		FAILED_FUNC("rpmemd_db_init");
	}
	prp = rpmemd_db_pool_create(db, pool_desc, 0, &attr1);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_create");
	}
	rpmemd_db_pool_close(db, prp);
	prp = rpmemd_db_pool_open(db, pool_desc, 0, &attr2);
	if (prp == NULL) {
		FAILED_FUNC("rpmemd_db_pool_open");
	}
	rpmemd_db_pool_close(db, prp);
	ret = rpmemd_db_pool_remove(db, pool_desc);
	if (ret) {
		FAILED_FUNC("rpmemd_db_pool_remove");
	}
	rpmemd_db_fini(db);

	compare_attr(&attr1, &attr2);

	return 0;
}

int
main(int argc, char *argv[])
{
	char *pool_desc, *log_file;
	char root_dir[PATH_MAX];

	START(argc, argv, "rpmemd_db");

	if (argc != 4)
		UT_FATAL("usage: %s <root_dir> <pool_desc> <log-file>",
				argv[0]);

	if (realpath(argv[1], root_dir) == NULL)
		UT_FATAL("!realpath(%s)", argv[1]);

	pool_desc = argv[2];
	log_file = argv[3];

	if (rpmemd_log_init("rpmemd error: ", log_file, 0))
		FAILED_FUNC("rpmemd_log_init");

	test_init(root_dir);
	test_check_dir(root_dir);
	test_create(root_dir, pool_desc);
	test_open(root_dir, pool_desc);

	rpmemd_log_close();

	DONE(NULL);
}
