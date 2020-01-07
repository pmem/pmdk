# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017, Intel Corporation

#
# src/test/libpmempool_rm/Makefile.inc -- build libpmempool_rm test
#

include ../Makefile.inc

../libpmempool_rm/libpmempool_rm:
	$(MAKE) -C ../libpmempool_rm all

all: ../libpmempool_rm/libpmempool_rm
