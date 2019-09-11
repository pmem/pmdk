#
# Copyright 2014-2019, Intel Corporation
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

TARGETS = libpmem libpmemblk libpmemlog libpmemobj libpmempool tools\
	libpmem2
ALL_TARGETS = $(TARGETS) common librpmem
POSSIBLE_TARGETS = $(TARGETS) common librpmem rpmem_common test\
	examples benchmarks

SCOPE_DIRS = $(TARGETS) common librpmem rpmem_common

DEBUG_RELEASE_TARGETS = common libpmem libpmemblk libpmemlog libpmemobj\
	libpmempool librpmem libpmem2
RELEASE_TARGETS = tools

ifneq ($(BUILD_EXAMPLES),n)
	ALL_TARGETS += examples
	RELEASE_TARGETS += examples
endif

ifneq ($(BUILD_BENCHMARKS),n)
	ALL_TARGETS += benchmarks
	RELEASE_TARGETS += benchmarks
endif

CLEAN_TARGETS = $(POSSIBLE_TARGETS)
CLOBBER_TARGETS = $(POSSIBLE_TARGETS)
CSTYLE_TARGETS = $(POSSIBLE_TARGETS)
INSTALL_TARGETS = $(TARGETS)
SPARSE_TARGETS = $(POSSIBLE_TARGETS)

HEADERS_DESTDIR = $(DESTDIR)$(includedir)
HEADERS_INSTALL = include/libpmem.h\
		include/libpmemobj.h include/libpmempool.h\
		include/libpmemblk.h include/libpmemlog.h\
		include/libpmem2.h

OBJ_HEADERS_INSTALL = include/libpmemobj/*.h

PKG_CONFIG_DESTDIR = $(DESTDIR)$(pkgconfigdir)
PKG_CONFIG_COMMON = common.pc
PKG_CONFIG_FILES = libpmem.pc libpmemobj.pc libpmemlog.pc libpmemblk.pc\
		libpmempool.pc libpmem2.pc
PMREORDER_BIN = $(DESTDIR)$(bindir)
PMREORDER_SRC = tools/pmreorder
PMREORDER_FILES = $(PMREORDER_SRC)/*.py
PMREORDER_DESTDIR = $(DESTDIR)$(datarootdir)/pmreorder
PMREORDER_ROOTDIR = $(datarootdir)/pmreorder

ifeq ($(BUILD_RPMEM),y)
	PKG_CONFIG_FILES += librpmem.pc
	HEADERS_INSTALL += include/librpmem.h
	INSTALL_TARGETS += librpmem
endif

rwildcard=$(strip $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2)\
	$(filter $(subst *,%,$2),$d)))

SCOPE_SRC_DIRS = $(SCOPE_DIRS) include
SCOPE_HDR_DIRS = $(SCOPE_DIRS) include

SCOPE_SRC_FILES = $(foreach d, $(SCOPE_SRC_DIRS), $(wildcard $(d)/*.c))
SCOPE_HDR_FILES = $(foreach d, $(SCOPE_HDR_DIRS), $(wildcard $(D)/*.h))
SCOPEFILES = $(SCOPE_SRC_FILES) $(SCOPE_HDR_FILES)

# include/lib*.h - skip include/pmemcompat.h
HEADERS =\
	$(foreach f, $(wildcard\
		freebsd/include/*.h\
		freebsd/include/*/*.h\
		include/lib*.h\
		include/libpmemobj/*.h\
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

tools: libpmem libpmemblk libpmemlog libpmemobj libpmempool
libpmemblk libpmemlog libpmemobj: libpmem
libpmempool: libpmemblk
benchmarks test tools: common

pkg-cfg-common:
	@printf "version=%s\nlibdir=%s\nprefix=%s\nrasdeps=%s\n" "$(SRCVERSION)" "$(libdir)" "$(prefix)" "$(LIBNDCTL_PKG_CONFIG_DEPS_VAR)" > $(PKG_CONFIG_COMMON)

$(PKG_CONFIG_COMMON): pkg-cfg-common

%.pc: $(PKG_CONFIG_COMMON) $(TOP)/utils/%.pc.in
	@echo Generating $@
	@cat $(PKG_CONFIG_COMMON) > $@
	@cat $(TOP)/utils/$@.in  >> $@

pkg-config: $(PKG_CONFIG_FILES)

$(eval $(call sub-target,$(INSTALL_TARGETS),install,y))
$(eval $(call sub-target,$(INSTALL_TARGETS),uninstall,y))
$(eval $(call sub-target,$(CLEAN_TARGETS),clean,y))
$(eval $(call sub-target,$(CLOBBER_TARGETS),clobber,y))
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

test: all
	$(MAKE) -C test test

check pcheck pycheck: test
	$(MAKE) -C test $@

require-rpmem:
ifneq ($(BUILD_RPMEM),y)
	$(error ERROR: cannot run remote tests because $(BUILD_RPMEM_INFO))
endif

check-remote: require-rpmem test
	$(MAKE) -C test $@

# Re-generate pkg-config files on 'make install' (not on 'make all'),
# to handle the case when prefix is specified only for 'install'.
# Clean up generated files when done.
install: all pkg-config
	install -d $(HEADERS_DESTDIR)
	install -p -m 0644 $(HEADERS_INSTALL) $(HEADERS_DESTDIR)
	install -d $(HEADERS_DESTDIR)/libpmemobj
	install -p -m 0644 $(OBJ_HEADERS_INSTALL) $(HEADERS_DESTDIR)/libpmemobj
	install -d $(PKG_CONFIG_DESTDIR)
	install -p -m 0644 $(PKG_CONFIG_FILES) $(PKG_CONFIG_DESTDIR)
	install -d $(PMREORDER_DESTDIR)
	install -p -m 0644 $(PMREORDER_FILES) $(PMREORDER_DESTDIR)
	@echo "#!/bin/sh" > $(PMREORDER_SRC)/pmreorder
	@echo "exec python3 $(PMREORDER_ROOTDIR)/pmreorder.py \$$*" >> $(PMREORDER_SRC)/pmreorder
	install -d $(PMREORDER_BIN)
	install -p $(PMREORDER_SRC)/pmreorder $(PMREORDER_BIN)
	$(RM) $(PKG_CONFIG_FILES)

uninstall:
	$(foreach f, $(HEADERS_INSTALL), $(RM) $(HEADERS_DESTDIR)/$(notdir $(f)))
	$(foreach f, $(OBJ_HEADERS_INSTALL), $(RM) $(HEADERS_DESTDIR)/libpmemobj/$(notdir $(f)))
	$(foreach f, $(PKG_CONFIG_FILES), $(RM) $(PKG_CONFIG_DESTDIR)/$(notdir $(f)))
	$(foreach f, $(PMREORDER_FILES), $(RM) $(PMREORDER_DESTDIR)/$(notdir $(f)))
	$(RM) $(PMREORDER_BIN)/pmreorder

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

.PHONY: all install uninstall clean clobber cstyle format test check pcheck\
	cscope $(ALL_TARGETS)\
	pkg-config check-remote clean-here pkg-cfg-common require-rpmem
