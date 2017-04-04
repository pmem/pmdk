#
# Copyright 2014-2017, Intel Corporation
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
ALL_TARGETS = $(TARGETS) common librpmem examples benchmarks
SCOPE_DIRS = $(TARGETS) common librpmem rpmem_common

DEBUG_RELEASE_TARGETS = common libpmem libvmem libpmemblk libpmemlog libpmemobj\
	libpmempool libvmmalloc librpmem
RELEASE_TARGETS = tools examples benchmarks

CLEAN_NO_JE_TARGETS = $(ALL_TARGETS) rpmem_common test
CLEAN_TARGETS = $(CLEAN_NO_JE_TARGETS) jemalloc
CLOBBER_NO_JE_TARGETS = $(ALL_TARGETS) rpmem_common test
CLOBBER_TARGETS = $(CLOBBER_NO_JE_TARGETS) jemalloc
CSTYLE_TARGETS = $(ALL_TARGETS) test rpmem_common
INSTALL_TARGETS = $(TARGETS)
SPARSE_TARGETS = $(ALL_TARGETS) test rpmem_common

HEADERS_DESTDIR = $(DESTDIR)$(includedir)
HEADERS_INSTALL = include/libpmem.h include/libvmem.h\
		include/libpmemobj.h include/libpmempool.h\
		include/libpmemblk.h include/libpmemlog.h\
		include/libvmmalloc.h

OBJ_HEADERS_INSTALL = include/libpmemobj/*.h

CPP_HEADERS_DESTDIR = $(DESTDIR)$(includedir)/libpmemobj++

MAKE_PKG_CONFIG = $(TOP)/utils/make-pkg-config.sh
PKG_CONFIG_DESTDIR = $(DESTDIR)$(pkgconfigdir)
PKG_CONFIG_FILES = libpmem.pc libvmem.pc libvmmalloc.pc libpmemobj.pc\
		libpmemlog.pc libpmemblk.pc libpmempool.pc libpmemobj++.pc

ifeq ($(EXPERIMENTAL),y)
ifeq ($(BUILD_RPMEM),y)
	PKG_CONFIG_FILES += librpmem.pc
	HEADERS_INSTALL += include/librpmem.h
	INSTALL_TARGETS += librpmem
endif
endif

SCOPE_SRC_DIRS = $(SCOPE_DIRS) include jemalloc/src
SCOPE_HDR_DIRS = $(SCOPE_DIRS) include jemalloc/src\
		jemalloc/include/jemalloc\
		jemalloc/include/jemalloc/internal\
		debug/jemalloc/include/jemalloc\
		debug/jemalloc/include/jemalloc/internal\
		nondebug/jemalloc/include/jemalloc\
		nondebug/jemalloc/include/jemalloc/internal

SCOPE_SRC_FILES = $(foreach d, $(SCOPE_SRC_DIRS), $(wildcard $(d)/*.c))
SCOPE_HDR_FILES = $(foreach d, $(SCOPE_HDR_DIRS), $(wildcard $(D)/*.h))
SCOPEFILES = $(SCOPE_SRC_FILES) $(SCOPE_HDR_FILES)

# include/lib*.h - skip include/pmemcompat.h
HEADERS =\
	$(foreach f, $(wildcard\
		include/lib*.h\
		include/libpmemobj/*.h\
		include/libpmemobj++/*.hpp\
		include/libpmemobj++/detail/*.hpp\
		windows/include/*.h\
		windows/include/*/*.h\
		), $(f))

ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addprefix debug/, $(addsuffix tmp, $(HEADERS)))
endif

debug/%.htmp: %.h
	$(call check-cstyle, $<, $@)

debug/%.hpptmp: %.hpp
	$(call check-cstyle, $<, $@)

all: $(TMP_HEADERS) $(ALL_TARGETS) pkg-config
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
tools: libpmem libpmemblk libpmemlog libpmemobj
libpmemblk libpmemlog libpmemobj: libpmem
benchmarks test tools: common

pkg-config:
	@$(MAKE_PKG_CONFIG) "$(prefix)" "$(libdir)" "$(SRCVERSION)"

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
	$(MAKE) -C jemalloc -f Makefile.nvml $@
ifeq ($(custom_build),)
	$(MAKE) -C jemalloc -f Makefile.nvml $@ DEBUG=1
endif

install-cpp:
	$(call install_recursive_filter,include/libpmemobj++,*.hpp,0644,$(CPP_HEADERS_DESTDIR))

install: install-cpp

uninstall-cpp:
	$(foreach f, include/libpmemobj++/*.hpp, $(RM) $(HEADERS_DESTDIR)/libpmemobj++/$(notdir $(f)))
	$(foreach f, include/libpmemobj++/detail/*.hpp, $(RM) $(HEADERS_DESTDIR)/libpmemobj++/detail/$(notdir $(f)))

uninstall: uninstall-cpp

install: all
	install -d $(HEADERS_DESTDIR)
	install -p -m 0644 $(HEADERS_INSTALL) $(HEADERS_DESTDIR)
	install -d $(HEADERS_DESTDIR)/libpmemobj
	install -p -m 0644 $(OBJ_HEADERS_INSTALL) $(HEADERS_DESTDIR)/libpmemobj
	install -d $(PKG_CONFIG_DESTDIR)
	install -p -m 0644 $(PKG_CONFIG_FILES) $(PKG_CONFIG_DESTDIR)

uninstall:
	$(foreach f, $(HEADERS_INSTALL), $(RM) $(HEADERS_DESTDIR)/$(notdir $(f)))
	$(foreach f, $(OBJ_HEADERS_INSTALL), $(RM) $(HEADERS_DESTDIR)/libpmemobj/$(notdir $(f)))
	$(foreach f, $(PKG_CONFIG_FILES), $(RM) $(PKG_CONFIG_DESTDIR)/$(notdir $(f)))

cstyle:
	$(STYLE_CHECK) check $(HEADERS)

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

.PHONY: all install uninstall uninstall-cpp install-cpp clean clobber cstyle\
	format test check pcheck jemalloc jemalloc-clean jemalloc-test\
	jemalloc-check cscope $(ALL_TARGETS) pkg-config check-remote clean-here
