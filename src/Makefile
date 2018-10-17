#
# Copyright 2014-2018, Intel Corporation
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
# src/Makefile -- Makefile for PMDK
#
TOP := $(dir $(lastword $(MAKEFILE_LIST)))..
include $(TOP)/src/common.inc

TARGETS = libvmem libvmmalloc
ALL_TARGETS = $(TARGETS) common examples

SCOPE_DIRS = $(TARGETS) common

DEBUG_RELEASE_TARGETS = common libvmem libvmmalloc
RELEASE_TARGETS = examples

CLEAN_NO_JE_TARGETS = $(ALL_TARGETS) test
CLEAN_TARGETS = $(CLEAN_NO_JE_TARGETS) jemalloc
CLOBBER_NO_JE_TARGETS = $(ALL_TARGETS) test
CLOBBER_TARGETS = $(CLOBBER_NO_JE_TARGETS) jemalloc
CSTYLE_TARGETS = $(ALL_TARGETS) test
INSTALL_TARGETS = $(TARGETS)
SPARSE_TARGETS = $(ALL_TARGETS) test

HEADERS_DESTDIR = $(DESTDIR)$(includedir)
HEADERS_INSTALL = include/libvmem.h include/libvmmalloc.h

OBJ_HEADERS_INSTALL =

PKG_CONFIG_DESTDIR = $(DESTDIR)$(pkgconfigdir)
PKG_CONFIG_COMMON = common.pc
PKG_CONFIG_FILES = libvmem.pc libvmmalloc.pc
PMREORDER_BIN = $(DESTDIR)$(bindir)
PMREORDER_SRC =
PMREORDER_FILES =
PMREORDER_DESTDIR = $(DESTDIR)$(datarootdir)/pmreorder
PMREORDER_ROOTDIR = $(datarootdir)/pmreorder

rwildcard=$(strip $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2)\
	$(filter $(subst *,%,$2),$d)))

SCOPE_SRC_DIRS = $(SCOPE_DIRS) include jemalloc/src
SCOPE_HDR_DIRS = $(SCOPE_DIRS) include jemalloc/src\
		jemalloc/include/jemalloc\
		jemalloc/include/jemalloc/internal\
		debug/libvmem/jemalloc/include/jemalloc\
		debug/libvmmalloc/jemalloc/include/jemalloc\
		debug/libvmem/jemalloc/include/jemalloc/internal\
		debug/libvmmalloc/jemalloc/include/jemalloc/internal\
		nondebug/libvmem/jemalloc/include/jemalloc\
		nondebug/libvmmalloc/jemalloc/include/jemalloc\
		nondebug/libvmem/jemalloc/include/jemalloc/internal\
		nondebug/libvmmalloc/jemalloc/include/jemalloc/internal

SCOPE_SRC_FILES = $(foreach d, $(SCOPE_SRC_DIRS), $(wildcard $(d)/*.c))
SCOPE_HDR_FILES = $(foreach d, $(SCOPE_HDR_DIRS), $(wildcard $(D)/*.h))
SCOPEFILES = $(SCOPE_SRC_FILES) $(SCOPE_HDR_FILES)

# include/lib*.h - skip include/pmemcompat.h
HEADERS =\
	$(foreach f, $(wildcard\
		freebsd/include/*.h\
		freebsd/include/*/*.h\
		include/lib*.h\
		windows/include/*.h\
		windows/include/*/*.h\
		), $(f))

ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addprefix debug/, $(addsuffix tmp, $(HEADERS)))
endif

SCRIPTS = $(call rwildcard,,*.sh)

debug/%.htmp: %.h
	$(call check-cstyle, $<, $@)

debug/%.hpptmp: %.hpp
	$(call check-cstyle, $<, $@)

all: $(TMP_HEADERS) $(ALL_TARGETS)
install: $(INSTALL_TARGETS:=-install)
uninstall: $(INSTALL_TARGETS:=-uninstall)
clean: $(CLEAN_TARGETS:=-clean)
clobber: $(CLOBBER_TARGETS:=-clobber)
cstyle: $(CSTYLE_TARGETS:=-cstyle)
format: $(CSTYLE_TARGETS:=-format)
examples benchmarks: $(TARGETS)
benchmarks: examples
sparse: $(SPARSE_TARGETS:=-sparse)

custom_build = $(DEBUG)$(OBJDIR)

libvmmalloc libvmem: jemalloc
test: common

pkg-cfg-common:
	@printf "version=%s\nlibdir=%s\nprefix=%s\n" "$(SRCVERSION)" "$(libdir)" "$(prefix)" > $(PKG_CONFIG_COMMON)

$(PKG_CONFIG_COMMON): pkg-cfg-common

%.pc: $(PKG_CONFIG_COMMON) $(TOP)/utils/%.pc.in
	@echo Generating $@
	@cat $(PKG_CONFIG_COMMON) > $@
	@cat $(TOP)/utils/$@.in  >> $@

pkg-config: $(PKG_CONFIG_FILES)

$(eval $(call sub-target,$(INSTALL_TARGETS),install,y))
$(eval $(call sub-target,$(INSTALL_TARGETS),uninstall,y))
$(eval $(call sub-target,$(CLEAN_NO_JE_TARGETS),clean,y))
$(eval $(call sub-target,$(CLOBBER_NO_JE_TARGETS),clobber,y))
$(eval $(call sub-target,$(CSTYLE_TARGETS),cstyle,n))
$(eval $(call sub-target,$(CSTYLE_TARGETS),format,n))
$(eval $(call sub-target,$(SPARSE_TARGETS),sparse,n))

$(DEBUG_RELEASE_TARGETS):
	$(MAKE) -C $@
ifeq ($(custom_build),)
	$(MAKE) -C $@ DEBUG=1
endif

$(RELEASE_TARGETS):
	$(MAKE) -C $@

jemalloc-check: jemalloc-test

test: all jemalloc-test
	$(MAKE) -C test test

check pcheck: test jemalloc-check
	$(MAKE) -C test $@

check-remote: test
	$(MAKE) -C test $@

jemalloc jemalloc-clean jemalloc-clobber jemalloc-test jemalloc-check:
	$(MAKE) -C jemalloc -f Makefile.libvmem     $@ EXTRA_CFLAGS="$(EXTRA_CFLAGS) -I$(abspath $(TOP))/src/common"
	$(MAKE) -C jemalloc -f Makefile.libvmmalloc $@ EXTRA_CFLAGS="$(EXTRA_CFLAGS) -I$(abspath $(TOP))/src/common"
ifeq ($(custom_build),)
	$(MAKE) -C jemalloc -f Makefile.libvmem     $@ DEBUG=1 EXTRA_CFLAGS="$(EXTRA_CFLAGS) -I$(abspath $(TOP))/src/common"
	$(MAKE) -C jemalloc -f Makefile.libvmmalloc $@ DEBUG=1 EXTRA_CFLAGS="$(EXTRA_CFLAGS) -I$(abspath $(TOP))/src/common"
endif

# Re-generate pkg-config files on 'make install' (not on 'make all'),
# to handle the case when prefix is specified only for 'install'.
# Clean up generated files when done.
install: all pkg-config
	install -d $(HEADERS_DESTDIR)
	install -p -m 0644 $(HEADERS_INSTALL) $(HEADERS_DESTDIR)
	install -d $(PKG_CONFIG_DESTDIR)
	install -p -m 0644 $(PKG_CONFIG_FILES) $(PKG_CONFIG_DESTDIR)
	$(RM) $(PKG_CONFIG_FILES)

uninstall:
	$(foreach f, $(HEADERS_INSTALL), $(RM) $(HEADERS_DESTDIR)/$(notdir $(f)))
	$(foreach f, $(PKG_CONFIG_FILES), $(RM) $(PKG_CONFIG_DESTDIR)/$(notdir $(f)))

cstyle:
	$(STYLE_CHECK) check $(HEADERS)
	$(CHECK_SHEBANG) $(SCRIPTS)

format:
	$(STYLE_CHECK) format $(HEADERS)

cscope:
	cscope -q -b $(SCOPEFILES)
	ctags -e $(SCOPEFILES)

clean-here:
	$(RM) tags cscope.in.out cscope.out cscope.po.out *.pc $(TMP_HEADERS)

clean: clean-here

clobber: clean-here

.NOTPARALLEL: libvmem libvmmalloc

.PHONY: all install uninstall clean clobber cstyle format test check pcheck\
	jemalloc jemalloc-clean jemalloc-test jemalloc-check cscope $(ALL_TARGETS)\
	pkg-config check-remote clean-here pkg-cfg-common
