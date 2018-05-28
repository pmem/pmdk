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
# src/common.inc -- common Makefile rules for PMDK
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))..

# import user variables
ifneq ($(wildcard $(TOP)/user.mk),)
include $(TOP)/user.mk
endif

LN = ln
OBJCOPY ?= objcopy
MKDIR = mkdir
INSTALL = install
CP = cp
CSTYLE = $(TOP)/utils/cstyle
CSTYLEON ?= 0
STYLE_CHECK = $(TOP)/utils/style_check.sh
CHECK_SHEBANG = $(TOP)/utils/check-shebang.sh
CHECK_OS = $(TOP)/utils/check-os.sh
OS_BANNED = $(TOP)/utils/os-banned
COVERAGE = 0

PKG_CONFIG ?= pkg-config
HEADERS = $(wildcard *.h) $(wildcard *.hpp)

ifeq ($(shell command -v clang-format-3.8 > /dev/null && echo y || echo n), y)
CLANG_FORMAT ?= clang-format-3.8
else
CLANG_FORMAT ?= clang-format
endif

GCOV_CFLAGS=-fprofile-arcs -ftest-coverage --coverage
GCOV_LDFLAGS=-fprofile-arcs -ftest-coverage
GCOV_LIBS=-lgcov

osdep = $(1)_$(shell uname -s | tr "[:upper:]" "[:lower:]")$(2)
get_arch = $(shell $(CC) -dumpmachine | awk -F'[/-]' '{print $$1}')

ifeq ($(PKG_CONFIG_CHECKED),)
ifeq ($(shell command -v $(PKG_CONFIG) && echo y || echo n), n)
$(error $(PKG_CONFIG) not found)
endif
endif
export PKG_CONFIG_CHECKED := y

check_package = $(shell $(PKG_CONFIG) $(1) && echo y || echo n)

check_flag = $(shell echo "int main(){return 0;}" |\
	$(CC) $(CFLAGS) -Werror $(1) -x c -o /dev/null - 2>/dev/null && echo y || echo n)

check_compiler = $(shell $(CC) --version | grep $(1) && echo y || echo n)

# Check for issues with older clang compilers which assert on delete persistent<[][]>.
check_clang_template_bug = $(shell echo "using namespace pmem::obj; int main() { delete_persistent<int[][3]>(make_persistent<int[][3]>(2), 2); return 0; }" |\
	$(CXX) --std=c++11 -x c++ -I$(TOP)/src/include/ -include libpmemobj++/make_persistent_array.hpp -L$(TOP)/src/debug/ -c -o /dev/null - 2>/dev/null && echo y || echo n)

# Check for issues with older gcc compilers which do not expand variadic template
# variables in lambda expressions.
check_gcc_variadic_template_bug = $(shell echo "void print() {} template<typename...Args, typename T> void print(const T&, const Args &...arg) {auto f = [&]{ print(arg...);};} int main() {print(1, 2, 3); return 0;}" |\
	$(CXX) --std=c++11 -x c++ -o /dev/null - 2>/dev/null && echo y || echo n)

check_cxx_flags = $(shell echo "int main(){return 0;}" |\
	$(CXX) $(1) -x c++ -o /dev/null - 2>/dev/null && echo y || echo n)

CXX_TESTS=$(check_gcc_variadic_template_bug)

cxx_ok=$(if $(findstring n,$(CXX_TESTS)),n,y)

# This is a workaround for older incompatible versions of libstdc++ and clang.
# Please see https://llvm.org/bugs/show_bug.cgi?id=15517 for more info.
check_cxx_chrono = $(shell echo "int main(){return 0;}" |\
	$(CXX) -std=c++11 -x c++ -include future -o /dev/null - 2>/dev/null && echo y || echo n)

check_Wconversion = $(shell echo "long random(void); char test(void); char test(void){char a = 0; char b = 'a'; char ret = random() == 1 ? a : b; return ret;}" |\
	$(CC) -c $(CFLAGS) -Wconversion -x c -o /dev/null - 2>/dev/null && echo y || echo n)

check_librt = $(shell echo "int main() { struct timespec t; return clock_gettime(CLOCK_MONOTONIC, &t); }" |\
	$(CC) $(CFLAGS) -x c -include time.h -o /dev/null - 2>/dev/null && echo n || echo y)

# XXX: required by clock_gettime(), if glibc version < 2.17
# The os_clock_gettime() function is now in OS abstraction layer,
# linked to all the librariess, unit tests and benchmarks.
ifeq ($(LIBRT_NEEDED),)
export LIBRT_NEEDED := $(call check_librt)
else
export LIBRT_NEEDED
endif

install_recursive = $(shell cd $(1) && find . -type f -exec install -m $(2) -D {} $(3)/{} \;)

install_recursive_filter = $(shell cd $(1) && find . -type f -name "$(2)" -exec install -m $(3) -D {} $(4)/{} \;)

define create-deps
	@cp $(objdir)/$*.d $(objdir)/.deps/$*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	    -e '/^$$/ d' -e 's/$$/ :/' < $(objdir)/$*.d >> $(objdir)/.deps/$*.P; \
	$(RM) -f $(objdir)/$*.d
endef

check_defined = \
    $(strip $(foreach 1,$1, \
        $(call __check_defined,$1,$(strip $(value 2)))))

__check_defined = \
    $(if $(value $1),, \
      $(error Undefined $1$(if $2, ($2))))

export prefix = /usr/local
export exec_prefix := $(prefix)
export sysconfdir := $(prefix)/etc
export datarootdir := $(prefix)/share
export mandir := $(datarootdir)/man
export docdir := $(datarootdir)/doc
export man1dir := $(mandir)/man1
export man3dir := $(mandir)/man3
export man5dir := $(mandir)/man5
export man7dir := $(mandir)/man7
export cstyle_bin := $(CSTYLE)
export clang_format_bin := $(CLANG_FORMAT)

ifneq ($(wildcard $(exec_prefix)/x86_64-linux-gnu),)
LIB_PREFIX ?= x86_64-linux-gnu/lib
endif

ifneq ($(wildcard $(exec_prefix)/lib64),)
LIB_PREFIX ?= lib64
endif

LIB_PREFIX ?= lib

all:

cstyle-%:
	$(STYLE_CHECK) $* $(wildcard *.[ch]) $(wildcard *.[ch]pp)

cstyle: cstyle-check

format: cstyle-format

ifeq ($(CSTYLEON),1)
define check-cstyle
	@$(STYLE_CHECK) check $1 && if [ "$2" != "" ]; then mkdir -p `dirname $2` && touch $2; fi
endef
else ifeq ($(CSTYLEON),2)
define check-cstyle
	@$(STYLE_CHECK) check $1 && if [ "$2" != "" ]; then mkdir -p `dirname $2` && touch $2; fi || true
endef
else
define check-cstyle
endef
endif

define check-os
$(CHECK_OS) $(OS_BANNED) $(1) $(2)
endef

# XXX: to allow gcov tool to connect coverage with source code, we have to
# use absolute path to source files
ifeq ($(COVERAGE),1)
define coverage-path
`readlink -f $(1)`
endef
else
define coverage-path
$(1)
endef
endif

define sub-target-foreach
$(1)-$(2):
	$$(MAKE) -C $1 $2
ifeq ($(3),y)
ifeq ($(custom_build),)
	$$(MAKE) -C $1 $2 DEBUG=1
endif
endif
endef

define sub-target
$(foreach f, $(1), $(eval $(call sub-target-foreach, $f,$(2),$(3))))
endef

ifneq ($(wildcard $(prefix)/x86_64-linux-gnu),)
INC_PREFIX ?= x86_64-linux-gnu/include
endif

INC_PREFIX ?= include

test_build=$(addprefix "-b ", $(TEST_BUILD))
test_type=$(addprefix " -t ", $(TEST_TYPE))
test_fs=$(addprefix " -f ", $(TEST_FS))
test_time=$(addprefix " -o ", $(TEST_TIME))
test_memcheck=$(addprefix " -m ", $(MEMCHECK))
test_pmemcheck=$(addprefix " -p ", $(PMEMCHECK))
test_helgrind=$(addprefix " -e ", $(HELGRIND))
test_drd=$(addprefix " -d ", $(DRD))
test_providers=$(addprefix " -q ", $(TEST_PROVIDERS))
test_pmethods=$(addprefix " -r ", $(TEST_PMETHODS))

ifeq ($(CHECK_POOL),y)
test_check_pool=" -c "
endif

RUNTEST_OPTIONS := "$(test_build)$(test_type)$(test_fs)$(test_time)"
RUNTEST_OPTIONS += "$(test_memcheck)$(test_pmemcheck)$(test_helgrind)$(test_drd)"
RUNTEST_OPTIONS += "$(test_providers)$(test_pmethods)$(test_check_pool)"

export libdir := $(exec_prefix)/$(LIB_PREFIX)
export includedir := $(prefix)/$(INC_PREFIX)
export pkgconfigdir := $(libdir)/pkgconfig
export bindir := $(exec_prefix)/bin
export bashcompdir := $(sysconfdir)/bash_completion.d

LIBFABRIC_MIN_VERSION := 1.4.2

# Keep in sync with requirements in src/test/unittest/unittest.sh and
# utils/docker/images/install-libfabric.sh.
ifeq ($(BUILD_RPMEM),)
export BUILD_RPMEM := $(call check_package, libfabric --atleast-version=$(LIBFABRIC_MIN_VERSION))
ifneq ($(BUILD_RPMEM),y)
export BUILD_RPMEM_INFO := libfabric (version >= $(LIBFABRIC_MIN_VERSION)) is missing -- \
see src/librpmem/README for details.
endif
endif

NDCTL_MIN_VERSION := 60.1

sparse-c = $(shell for c in *.c; do sparse -Wsparse-all -Wno-declaration-after-statement $(CFLAGS) $(INCS) $$c || true; done)

ifeq ($(shell uname -s),FreeBSD)

GLIBC_CXXFLAGS=-D_GLIBCXX_USE_C99
UNIX98_CFLAGS=
OS_INCS=-I$(TOP)/src/freebsd/include -I/usr/local/include
OS_LIBS=-L/usr/local/lib
LIBDL=
LIBUTIL=-lutil
LIBUUID=-luuid
LIBCXXABI=-lcxxrt
LIBNDCTL=
OS_DIMM=none

else

GLIBC_CXXFLAGS=
UNIX98_CFLAGS=-D__USE_UNIX98
OS_INCS=
OS_LIBS=
LIBDL=-ldl
LIBUTIL=
LIBUUID=
LIBCXXABI=-lc++abi

# XXX: disable building with ndctl, unless explicitly enabled
NDCTL_ENABLE ?= n

# Detect libndctl if not disabled.
ifneq ($(NDCTL_ENABLE),n)
HAS_NDCTL :=  $(shell echo "int main() {struct ndctl_cmd *cmd; ndctl_cmd_smart_get_shutdown_count(cmd);return 0;}" | \
 $(CC) -x c -include ndctl/libndctl.h -o /dev/null - -lndctl 2>/dev/null && echo y || echo n)
HAS_DAXCTL := $(call check_package, libdaxctl --atleast-version $(NDCTL_MIN_VERSION))
endif

# If neither disabled or enabled, set it based on libndctl presence.
ifeq ($(NDCTL_ENABLE),)
ifeq ($(HAS_NDCTL)$(HAS_DAXCTL),yy)
export NDCTL_ENABLE=y
else
export NDCTL_ENABLE=n
endif
endif

ifeq ($(NDCTL_ENABLE),y)
ifeq ($(HAS_NDCTL),n)
$(error libndctl(version >= $(NDCTL_MIN_VERSION)) is missing -- see README)
endif
ifeq ($(HAS_DAXCTL),n)
$(error libdaxclt(version >= $(NDCTL_MIN_VERSION)) is missing -- see README)
endif
LIBNDCTL = $(shell $(PKG_CONFIG) --libs libndctl libdaxctl)
OS_DIMM=ndctl
else
OS_DIMM=none
LIBNDCTL=
endif

ifeq ($(USE_LIBUNWIND),)
export USE_LIBUNWIND := $(call check_package, libunwind)
ifeq ($(USE_LIBUNWIND),y)
export LIBUNWIND_LIBS := $(shell $(PKG_CONFIG) --libs libunwind)
endif
else
export USE_LIBUNWIND
export LIBUNWIND_LIBS
endif

endif
