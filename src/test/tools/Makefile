# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2022, Intel Corporation

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
       bttcreate\
       gran_detecto\
       ddmap\
       cmpmap\
       extents\
       fallocate_detect\
       obj_verify\
       usc_permission_check\
       anonymous_mmap\
       mapexec

ifeq ($(ARCH), x86_64)
DIRS += \
	cpufd
endif

all     : TARGET = all
clean   : TARGET = clean
clobber : TARGET = clobber
cstyle  : TARGET = cstyle
format  : TARGET = format
sparse  : TARGET = sparse

all test cstyle clean clobber format sparse: $(DIRS)

$(TESTCONFIG):

$(DIRS):
	$(MAKE) -C $@ $(TARGET)

check pcheck pycheck: all

.PHONY: all clean clobber cstyle format check pcheck $(DIRS)
