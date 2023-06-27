# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2023, Intel Corporation

#
# src/test/libpmempool_api/Makefile.inc -- build libpmempool_* unittest
#

../libpmempool_api/libpmempool_test:
	$(MAKE) -C ../libpmempool_api all

all: ../libpmempool_api/libpmempool_test

USE_PMEMSPOIL=y

include ../Makefile.inc
