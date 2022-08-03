# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2022, Intel Corporation
#
# Makefile -- top Makefile for tools
#

TOP = ../..

TESTCONFIG=$(TOP)/src/test/testconfig.sh

TARGETS = pmempool daxio pmreorder
SCOPEDIRS=$(TARGETS)
SCOPEFILES=$(foreach dir, $(SCOPEDIRS), $(shell find $(dir) -name *.[ch] ))

all    : TARGET = all
check  : TARGET = check
test   : TARGET = test
clean  : TARGET = clean
clobber: TARGET = clobber
cstyle : TARGET = cstyle
format : TARGET = format
install: TARGET = install
uninstall: TARGET = uninstall
sparse: TARGET = sparse

all clean clobber cstyle install uninstall check format test sparse: $(TARGETS)

$(TESTCONFIG):

$(TARGETS):
	$(MAKE) -C $@ $(TARGET)

clean:
	$(RM) TAGS cscope.in.out cscope.out cscope.po.out

clobber: clean

cscope:
	cscope -q -b $(SCOPEFILES)
	ctags -e $(SCOPEFILES)

.PHONY: all clean clobber cstyle format install uninstall common cscope $(TARGETS)
