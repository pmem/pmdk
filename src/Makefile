# Copyright (c) 2014, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# src/Makefile -- Makefile for NVML
#

export CSTYLE = $(abspath ../utils/cstyle)

TARGETS = libpmem libvmem libpmemblk libpmemlog libpmemobj
ALL_TARGETS = $(TARGETS) examples benchmarks

HEADERS_DESTDIR = $(DESTDIR)/usr/include
HEADERS_INSTALL = include/libpmem.h include/libvmem.h include/libpmemobj.h include/libpmemblk.h include/libpmemlog.h

SCOPE_SRC_DIRS = $(TARGETS) include jemalloc/src
SCOPE_HDR_DIRS = $(TARGETS) include jemalloc/src\
		jemalloc/include/jemalloc\
		jemalloc/include/jemalloc/internal\
		debug/jemalloc/include/jemalloc\
		debug/jemalloc/include/jemalloc/internal\
		nondebug/jemalloc/include/jemalloc\
		nondebug/jemalloc/include/jemalloc/internal

SCOPE_SRC_FILES = $(foreach d, $(SCOPE_SRC_DIRS), $(wildcard $(d)/*.c))
SCOPE_HDR_FILES = $(foreach d, $(SCOPE_HDR_DIRS), $(wildcard $(D)/*.h))
SCOPEFILES = $(SCOPE_SRC_FILES) $(SCOPE_HDR_FILES)

all: $(ALL_TARGETS)
install: $(TARGETS:=-install)
clean: $(ALL_TARGETS:=-clean)
clobber: $(ALL_TARGETS:=-clobber)
cstyle: $(ALL_TARGETS:=-cstyle)
examples benchmarks: $(TARGETS)

custom_build = $(DEBUG)$(OBJDIR)


%-install:
	$(MAKE) -C $* install
ifeq ($(custom_build),)
	$(MAKE) -C $* install DEBUG=1
endif

%-clean:
	$(MAKE) -C $* clean
ifeq ($(custom_build),)
	$(MAKE) -C $* clean DEBUG=1
endif

%-clobber:
	$(MAKE) -C $* clobber
ifeq ($(custom_build),)
	$(MAKE) -C $* clobber DEBUG=1
endif

%-cstyle:
	$(MAKE) -C $* cstyle

$(ALL_TARGETS):
	$(MAKE) -C $@
ifeq ($(custom_build),)
	$(MAKE) -C $@ DEBUG=1
endif

jemalloc-check: jemalloc-test

test: all jemalloc-test
	$(MAKE) -C test all

check: test jemalloc-check
	cd test && ./RUNTESTS

jemalloc jemalloc-clean jemalloc-clobber jemalloc-test jemalloc-check:
	$(MAKE) -C libvmem $@
ifeq ($(custom_build),)
	$(MAKE) -C libvmem $@ DEBUG=1
endif

install:
	install -d $(HEADERS_DESTDIR)
	install -p -m 0644 $(HEADERS_INSTALL) $(HEADERS_DESTDIR)

cscope:
	cscope -q -b $(SCOPEFILES)
	ctags -e $(SCOPEFILES)

clean:
	$(RM) tags cscope.in.out cscope.out cscope.po.out

.PHONY: all install clean clobber cstyle test check\
	jemalloc jemalloc-clean jemalloc-test jemalloc-check cscope $(ALL_TARGETS)
