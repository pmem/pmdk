#
# Copyright 2014-2016, Intel Corporation
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
#     * Neither the name of the copyright holder nor the names of its
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
TOP := $(dir $(lastword $(MAKEFILE_LIST)))..
include $(TOP)/src/common.inc

TARGETS = libpmem libvmem libpmemblk libpmemlog libpmemobj libpmempool\
	libvmmalloc tools
ALL_TARGETS = $(TARGETS) librpmem examples benchmarks

DEBUG_RELEASE_TARGETS = libpmem libvmem libpmemblk libpmemlog libpmemobj\
	libpmempool libvmmalloc librpmem
RELEASE_TARGETS = tools examples benchmarks

CLEAN_NO_JE_TARGETS = $(ALL_TARGETS) test
CLEAN_TARGETS = $(CLEAN_NO_JE_TARGETS) jemalloc
CLOBBER_NO_JE_TARGETS = $(ALL_TARGETS) test
CLOBBER_TARGETS = $(CLOBBER_NO_JE_TARGETS) jemalloc
CSTYLE_TARGETS = $(ALL_TARGETS) common test rpmem_common
INSTALL_TARGETS = $(TARGETS)

HEADERS_DESTDIR = $(DESTDIR)$(includedir)
HEADERS_INSTALL = include/libpmem.h include/libvmem.h\
		include/libpmemobj.h include/libpmempool.h\
		include/libpmemblk.h include/libpmemlog.h\
		include/libvmmalloc.h

CPP_HEADERS_DESTDIR = $(DESTDIR)$(includedir)/libpmemobj

MAKE_PKG_CONFIG = $(TOP)/utils/make-pkg-config.sh
PKG_CONFIG_DESTDIR = $(DESTDIR)$(pkgconfigdir)
PKG_CONFIG_FILES = libpmem.pc libvmem.pc libvmmalloc.pc libpmemobj.pc\
		libpmemlog.pc libpmemblk.pc libpmempool.pc

ifeq ($(EXPERIMENTAL),y)
	PKG_CONFIG_FILES += libpmemobj++.pc
	PKG_CONFIG_FILES += librpmem.pc

	HEADERS_INSTALL += include/librpmem.h

	INSTALL_TARGETS += librpmem
endif

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

all: $(ALL_TARGETS) pkg-config
install: $(INSTALL_TARGETS:=-install)
uninstall: $(INSTALL_TARGETS:=-uninstall)
clean: $(CLEAN_TARGETS:=-clean)
clobber: $(CLOBBER_TARGETS:=-clobber)
cstyle: $(CSTYLE_TARGETS:=-cstyle)
format: $(CSTYLE_TARGETS:=-format)
examples benchmarks: $(TARGETS)
benchmarks: examples

custom_build = $(DEBUG)$(OBJDIR)

libvmmalloc libvmem: jemalloc
tools: libpmem libpmemblk libpmemlog libpmemobj
libpmemblk libpmemlog libpmemobj: libpmem

pkg-config:
	@$(MAKE_PKG_CONFIG) "$(prefix)" "$(libdir)" "$(SRCVERSION)"

$(eval $(call sub-target,$(INSTALL_TARGETS),install,y))
$(eval $(call sub-target,$(INSTALL_TARGETS),uninstall,y))
$(eval $(call sub-target,$(CLEAN_NO_JE_TARGETS),clean,y))
$(eval $(call sub-target,$(CLOBBER_NO_JE_TARGETS),clobber,y))
$(eval $(call sub-target,$(CSTYLE_TARGETS),cstyle,n))
$(eval $(call sub-target,$(CSTYLE_TARGETS),format,n))

$(DEBUG_RELEASE_TARGETS):
	$(MAKE) -C $@
ifeq ($(custom_build),)
	$(MAKE) -C $@ DEBUG=1
endif

$(RELEASE_TARGETS):
	$(MAKE) -C $@

jemalloc-check: jemalloc-test

test: all jemalloc-test
	$(MAKE) -C test all

check pcheck: test jemalloc-check
	$(MAKE) -C test $@

check-remote: test
	$(MAKE) -C test $@

jemalloc jemalloc-clean jemalloc-clobber jemalloc-test jemalloc-check:
	$(MAKE) -C jemalloc -f Makefile.nvml $@
ifeq ($(custom_build),)
	$(MAKE) -C jemalloc -f Makefile.nvml $@ DEBUG=1
endif

ifeq ($(EXPERIMENTAL),y)
install-cpp:
	$(call install_recursive_filter,include/libpmemobj,*.hpp,0644,$(CPP_HEADERS_DESTDIR))

install: install-cpp

uninstall-cpp:
	$(RM) -r $(CPP_HEADERS_DESTDIR)

uninstall: uninstall-cpp
endif

install: all
	install -d $(HEADERS_DESTDIR)
	install -p -m 0644 $(HEADERS_INSTALL) $(HEADERS_DESTDIR)
	install -d $(PKG_CONFIG_DESTDIR)
	install -p -m 0644 $(PKG_CONFIG_FILES) $(PKG_CONFIG_DESTDIR)

uninstall:
	$(foreach f, $(HEADERS_INSTALL), $(RM) $(HEADERS_DESTDIR)/$(notdir $(f)))
	$(foreach f, $(PKG_CONFIG_FILES), $(RM) $(PKG_CONFIG_DESTDIR)/$(notdir $(f)))


# [!q] to skip windows/include/sys/queue.h
cstyle:
	$(STYLE_CHECK) check include/*.h
	$(STYLE_CHECK) check include/libpmemobj/*.hpp
	$(STYLE_CHECK) check include/libpmemobj/detail/*.hpp
	$(STYLE_CHECK) check windows/include/*.h
	$(STYLE_CHECK) check windows/include/*/[!q]*.h

format:
	$(STYLE_CHECK) format include/libpmemobj/*.hpp
	$(STYLE_CHECK) format include/libpmemobj/detail/*.hpp

cscope:
	cscope -q -b $(SCOPEFILES)
	ctags -e $(SCOPEFILES)

clean:
	$(RM) tags cscope.in.out cscope.out cscope.po.out

.NOTPARALLEL: libvmem libvmmalloc

.PHONY: all install uninstall unistall-cpp install-cpp clean clobber cstyle format test check pcheck\
	jemalloc jemalloc-clean jemalloc-test jemalloc-check cscope $(ALL_TARGETS)\
	pkg-config check-remote
