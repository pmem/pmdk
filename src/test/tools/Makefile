# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020, Intel Corporation

#
# src/test/tools/Makefile -- build unit test helpers
#

TOP = ../../..

TESTCONFIG=$(TOP)/src/test/testconfig.sh

DIRS = \
       pmemspoil\
       pmemwrite\
       pmemalloc\
       pmemobjcli\
       pmemdetect\
       ctrld\
       bttcreate\
       gran_detecto\
       fip\
       ddmap\
       cmpmap\
       extents\
       fallocate_detect\
       obj_verify\
       usc_permission_check\
       anonymous_mmap

ifeq ($(ARCH), x86_64)
DIRS += \
	cpufd
endif

REMOTE_TOOLS = \
	ctrld\
	extents\
	fip\
	obj_verify\
	pmemobjcli\
	pmemspoil\
	pmemdetect

all     : TARGET = all
clean   : TARGET = clean
clobber : TARGET = clobber
cstyle  : TARGET = cstyle
format  : TARGET = format
sync-remotes : TARGET = sync-remotes
sparse  : TARGET = sparse

all test cstyle clean clobber format sparse: $(DIRS)

$(TESTCONFIG):

sync-remotes: $(REMOTE_TOOLS) $(TESTCONFIG)

$(DIRS):
	$(MAKE) -C $@ $(TARGET)

check pcheck pycheck: all

.PHONY: all clean clobber cstyle format check pcheck $(DIRS)
