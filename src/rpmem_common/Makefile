# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2016, Intel Corporation

#
# src/rpmem_common/Makefile -- Makefile for rpmem_common
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../..
HEADERS = $(wildcard *.h)

include $(TOP)/src/common.inc

ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addsuffix tmp, $(HEADERS))
endif

all: $(TMP_HEADERS)

cstyle:
	$(CSTYLE) *.[ch]

%.htmp: %.h
	$(call check-cstyle, $<, $@)

clean:
	$(RM) $(TMP_HEADERS)

clobber: clean

sparse:
ifeq ($(BUILD_RPMEM),y)
	$(sparse-c)
endif
