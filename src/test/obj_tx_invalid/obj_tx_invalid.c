/*
 * Copyright 2016-2017, Intel Corporation
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
 * obj_tx_invalid.c -- tests which transactional functions are available in
 * which transaction stages
 */

#include <stddef.h>

#include "unittest.h"

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(tx_invalid);
POBJ_LAYOUT_ROOT(tx_invalid, struct dummy_root);
POBJ_LAYOUT_TOID(tx_invalid, struct dummy_node);
POBJ_LAYOUT_END(tx_invalid);

struct dummy_node {
	int value;
};

struct dummy_root {
	TOID(struct dummy_node) node;
};

int
main(int argc, char *argv[])
{
	if (argc != 3)
		UT_FATAL("usage: %s file-name op", argv[0]);

	START(argc, argv, "obj_tx_invalid %s", argv[2]);

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(tx_invalid) != 1);

	PMEMobjpool *pop;
	const char *path = argv[1];

	if (os_access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(tx_invalid),
			PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR)) == NULL) {
			UT_FATAL("!pmemobj_create %s", path);
		}
	} else {
		if ((pop = pmemobj_open(path, POBJ_LAYOUT_NAME(tx_invalid)))
			== NULL) {
			UT_FATAL("!pmemobj_open %s", path);
		}
	}

	PMEMoid oid = pmemobj_first(pop);

	if (OID_IS_NULL(oid)) {
		if (pmemobj_alloc(pop, &oid, 10, 1, NULL, NULL))
			UT_FATAL("!pmemobj_alloc");
	} else {
		UT_ASSERTeq(pmemobj_type_num(oid), 1);
	}

	/* suppress error msg by default, enable for non-abort cases */
	ut_suppress_errmsg();
	if (strcmp(argv[2], "alloc") == 0)
		pmemobj_tx_alloc(10, 1);
	else if (strcmp(argv[2], "alloc-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_alloc(10, 1);
		} TX_END
	} else if (strcmp(argv[2], "alloc-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_alloc(10, 1);
		} TX_END
	} else if (strcmp(argv[2], "alloc-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_alloc(10, 1);
		} TX_END
	} else if (strcmp(argv[2], "alloc-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_alloc(10, 1);
		} TX_END
	} else if (strcmp(argv[2], "alloc-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_alloc(10, 1);
	}

	else if (strcmp(argv[2], "zalloc") == 0)
		pmemobj_tx_zalloc(10, 1);
	else if (strcmp(argv[2], "zalloc-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_zalloc(10, 1);
		} TX_END
	} else if (strcmp(argv[2], "zalloc-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_zalloc(10, 1);
		} TX_END
	} else if (strcmp(argv[2], "zalloc-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_zalloc(10, 1);
		} TX_END
	} else if (strcmp(argv[2], "zalloc-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_zalloc(10, 1);
		} TX_END
	} else if (strcmp(argv[2], "zalloc-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_zalloc(10, 1);
	}

	else if (strcmp(argv[2], "strdup") == 0)
		pmemobj_tx_strdup("aaa", 1);
	else if (strcmp(argv[2], "strdup-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_strdup("aaa", 1);
		} TX_END
	} else if (strcmp(argv[2], "strdup-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_strdup("aaa", 1);
		} TX_END
	} else if (strcmp(argv[2], "strdup-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_strdup("aaa", 1);
		} TX_END
	} else if (strcmp(argv[2], "strdup-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_strdup("aaa", 1);
		} TX_END
	} else if (strcmp(argv[2], "strdup-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_strdup("aaa", 1);
	}

	else if (strcmp(argv[2], "realloc") == 0)
		pmemobj_tx_realloc(oid, 10, 1);
	else if (strcmp(argv[2], "realloc-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_realloc(oid, 10, 1);
		} TX_END
	} else if (strcmp(argv[2], "realloc-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_realloc(oid, 10, 1);
		} TX_END
	} else if (strcmp(argv[2], "realloc-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_realloc(oid, 10, 1);
		} TX_END
	} else if (strcmp(argv[2], "realloc-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_realloc(oid, 10, 1);
		} TX_END
	} else if (strcmp(argv[2], "realloc-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_realloc(oid, 10, 1);
	}

	else if (strcmp(argv[2], "zrealloc") == 0)
		pmemobj_tx_zrealloc(oid, 10, 1);
	else if (strcmp(argv[2], "zrealloc-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_zrealloc(oid, 10, 1);
		} TX_END
	} else if (strcmp(argv[2], "zrealloc-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_zrealloc(oid, 10, 1);
		} TX_END
	} else if (strcmp(argv[2], "zrealloc-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_zrealloc(oid, 10, 1);
		} TX_END
	} else if (strcmp(argv[2], "zrealloc-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_zrealloc(oid, 10, 1);
		} TX_END
	} else if (strcmp(argv[2], "zrealloc-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_zrealloc(oid, 10, 1);
	}

	else if (strcmp(argv[2], "free") == 0)
		pmemobj_tx_free(oid);
	else if (strcmp(argv[2], "free-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_free(oid);
		} TX_END
	} else if (strcmp(argv[2], "free-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_free(oid);
		} TX_END
	} else if (strcmp(argv[2], "free-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_free(oid);
		} TX_END
	} else if (strcmp(argv[2], "free-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_free(oid);
		} TX_END
	} else if (strcmp(argv[2], "free-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_free(oid);
	}

	else if (strcmp(argv[2], "add_range") == 0)
		pmemobj_tx_add_range(oid, 0, 10);
	else if (strcmp(argv[2], "add_range-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_add_range(oid, 0, 10);
		} TX_END
	} else if (strcmp(argv[2], "add_range-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_add_range(oid, 0, 10);
		} TX_END
	} else if (strcmp(argv[2], "add_range-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_add_range(oid, 0, 10);
		} TX_END
	} else if (strcmp(argv[2], "add_range-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_add_range(oid, 0, 10);
		} TX_END
	} else if (strcmp(argv[2], "add_range-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_add_range(oid, 0, 10);
	}

	else if (strcmp(argv[2], "add_range_direct") == 0)
		pmemobj_tx_add_range_direct(pmemobj_direct(oid), 10);
	else if (strcmp(argv[2], "add_range_direct-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_add_range_direct(pmemobj_direct(oid), 10);
		} TX_END
	} else if (strcmp(argv[2], "add_range_direct-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_add_range_direct(pmemobj_direct(oid), 10);
		} TX_END
	} else if (strcmp(argv[2], "add_range_direct-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_add_range_direct(pmemobj_direct(oid), 10);
		} TX_END
	} else if (strcmp(argv[2], "add_range_direct-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_add_range_direct(pmemobj_direct(oid), 10);
		} TX_END
	} else if (strcmp(argv[2], "add_range_direct-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_add_range_direct(pmemobj_direct(oid), 10);
	}

	else if (strcmp(argv[2], "abort") == 0)
		pmemobj_tx_abort(ENOMEM);
	else if (strcmp(argv[2], "abort-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_END
	} else if (strcmp(argv[2], "abort-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_abort(ENOMEM);
		} TX_END
	} else if (strcmp(argv[2], "abort-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_abort(ENOMEM);
		} TX_END
	} else if (strcmp(argv[2], "abort-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_abort(ENOMEM);
		} TX_END
	} else if (strcmp(argv[2], "abort-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_abort(ENOMEM);
	}

	else if (strcmp(argv[2], "commit") == 0)
		pmemobj_tx_commit();
	else if (strcmp(argv[2], "commit-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_commit();
		} TX_END
	} else if (strcmp(argv[2], "commit-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_commit();
		} TX_END
	} else if (strcmp(argv[2], "commit-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_commit();
		} TX_END
	} else if (strcmp(argv[2], "commit-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_commit();
		} TX_END
	} else if (strcmp(argv[2], "commit-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_commit();
	}

	else if (strcmp(argv[2], "end") == 0)
		pmemobj_tx_end();
	else if (strcmp(argv[2], "end-in-work") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_end();
		} TX_END
	} else if (strcmp(argv[2], "end-in-abort") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_end();
			pmemobj_close(pop);
			exit(0);
		} TX_END
	} else if (strcmp(argv[2], "end-in-commit") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_end();
			pmemobj_close(pop);
			exit(0);
		} TX_END
	} else if (strcmp(argv[2], "end-in-finally") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_end();
			pmemobj_close(pop);
			exit(0);
		} TX_END
	} else if (strcmp(argv[2], "end-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_end();
	}

	else if (strcmp(argv[2], "process") == 0)
		pmemobj_tx_process();
	else if (strcmp(argv[2], "process-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_process();
		} TX_END
	} else if (strcmp(argv[2], "process-in-abort") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			pmemobj_tx_process();
		} TX_END
	} else if (strcmp(argv[2], "process-in-commit") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			pmemobj_tx_process();
		} TX_END
	} else if (strcmp(argv[2], "process-in-finally") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
		} TX_FINALLY {
			pmemobj_tx_process();
			pmemobj_tx_end();
			pmemobj_close(pop);
			exit(0);
		} TX_END
	} else if (strcmp(argv[2], "process-after-tx") == 0) {
		TX_BEGIN(pop) {
		} TX_END
		pmemobj_tx_process();
	}

	else if (strcmp(argv[2], "begin") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
		} TX_END
	} else if (strcmp(argv[2], "begin-in-work") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
			TX_BEGIN(pop) {
			} TX_END
		} TX_END
	} else if (strcmp(argv[2], "begin-in-abort") == 0) {
		TX_BEGIN(pop) {
			pmemobj_tx_abort(ENOMEM);
		} TX_ONABORT {
			TX_BEGIN(pop) {
			} TX_END
		} TX_END
	} else if (strcmp(argv[2], "begin-in-commit") == 0) {
		TX_BEGIN(pop) {
		} TX_ONCOMMIT {
			TX_BEGIN(pop) {
			} TX_END
		} TX_END
	} else if (strcmp(argv[2], "begin-in-finally") == 0) {
		TX_BEGIN(pop) {
		} TX_FINALLY {
			TX_BEGIN(pop) {
			} TX_END
		} TX_END
	} else if (strcmp(argv[2], "begin-after-tx") == 0) {
		ut_unsuppress_errmsg();
		TX_BEGIN(pop) {
		} TX_END
		TX_BEGIN(pop) {
		} TX_END
	}

	pmemobj_close(pop);

	DONE(NULL);
}
