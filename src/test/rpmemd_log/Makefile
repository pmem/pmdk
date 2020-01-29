# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation

#
# src/test/rpmemd_log/Makefile -- build rpmemd_log test
#
TOP = ../../..
vpath %.c $(TOP)/src/tools/rpmemd

TARGET = rpmemd_log
OBJS = rpmemd_log_test.o rpmemd_log.o

BUILD_STATIC_DEBUG=n
BUILD_STATIC_NONDEBUG=n
LIBPMEMCOMMON=y

include ../Makefile.inc

# The _FORTIFY_SOURCE flag invalidates the mock functions
# as it will change the syslog call to __syslog_chk. So
# we need to disable it.
CFLAGS += -I../../tools/rpmemd -U_FORTIFY_SOURCE
LDFLAGS += $(call extract_funcs, rpmemd_log_test.c)
