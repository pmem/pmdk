# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

#
# src/deps/makefile -- build miniasync
#

OS_KERNEL_NAME := $(shell uname -s)
# XXX: miniasync works only on Linux (and Windows)
all clean clobber:
ifeq ($(OS_KERNEL_NAME),Linux)
	$(MAKE) -C miniasync $@
endif
cstyle check:
