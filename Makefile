# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2020, Intel Corporation

#
# Makefile -- top-level Makefile for PMDK
#
# Use "make" to build the library.
#
# Use "make doc" to build documentation.
#
# Use "make test" to build unit tests. Add "SKIP_SYNC_REMOTES=y" to skip
# or "FORCE_SYNC_REMOTES=y" to force syncing remote nodes if any is defined.
#
# Use "make check" to run unit tests.
#
# Use "make check-remote" to run only remote unit tests.
#
# Use "make clean" to delete all intermediate files (*.o, etc).
#
# Use "make clobber" to delete everything re-buildable (binaries, etc.).
#
# Use "make cstyle" to run cstyle on all C source files
#
# Use "make check-license" to check copyright and license in all source files
#
# Use "make rpm" to build rpm packages
#
# Use "make dpkg" to build dpkg packages
#
# Use "make source DESTDIR=path_to_dir" to copy source files
# from HEAD to 'path_to_dir/pmdk' directory.
#
# As root, use "make install" to install the library in the usual
# locations (/usr/local/lib, /usr/local/include, and /usr/local/share/man).
# You can provide custom directory prefix for installation using
# DESTDIR variable e.g.: "make install DESTDIR=/opt"
# You can override the prefix within DESTDIR using prefix variable
# e.g.: "make install prefix=/usr"

include src/common.inc

RPM_BUILDDIR=rpmbuild
DPKG_BUILDDIR=dpkgbuild
EXPERIMENTAL ?= n
BUILD_PACKAGE_CHECK ?= y
BUILD_RPMEM ?= y
TEST_CONFIG_FILE ?= "$(CURDIR)"/src/test/testconfig.sh
PMEM2_INSTALL ?= n

rpm : override DESTDIR="$(CURDIR)/$(RPM_BUILDDIR)"
dpkg: override DESTDIR="$(CURDIR)/$(DPKG_BUILDDIR)"
rpm dpkg: override prefix=/usr

all: doc
	$(MAKE) -C src $@

doc:
	test -f .skip-doc || $(MAKE) -C doc all

clean:
	$(MAKE) -C src $@
	test -f .skip-doc || $(MAKE) -C doc $@
	$(RM) -r $(RPM_BUILDDIR) $(DPKG_BUILDDIR)
	$(RM) -f $(GIT_VERSION)

clobber:
	$(MAKE) -C src $@
	test -f .skip-doc || $(MAKE) -C doc $@
	$(RM) -r $(RPM_BUILDDIR) $(DPKG_BUILDDIR) rpm dpkg
	$(RM) -f $(GIT_VERSION)

require-rpmem:
ifneq ($(BUILD_RPMEM),y)
	$(error ERROR: cannot run remote tests because $(BUILD_RPMEM_INFO))
endif

check-remote: require-rpmem all
	$(MAKE) -C src $@

test check pcheck pycheck: all
	$(MAKE) -C src $@

cstyle:
	test -d .git && utils/check-commits.sh
	$(MAKE) -C src $@
	$(MAKE) -C utils $@
	@echo Checking files for whitespace issues...
	@utils/check_whitespace -g
	@echo Done.

format:
	$(MAKE) -C src $@
	@echo Done.

check-license:
	@utils/check_license/check-headers.sh $(TOP) BSD-3-Clause
	@echo Done.

sparse:
	$(MAKE) -C src sparse

source: clobber
	$(if "$(DESTDIR)", , $(error Please provide DESTDIR variable))
	+utils/copy-source.sh "$(DESTDIR)" $(SRCVERSION)

pkg-clean:
	$(RM) -r "$(DESTDIR)"

rpm dpkg: pkg-clean
	$(MAKE) source DESTDIR="$(DESTDIR)"
	+utils/build-$@.sh -t $(SRCVERSION) -s "$(DESTDIR)"/pmdk -w "$(DESTDIR)" -o $(CURDIR)/$@\
			-e $(EXPERIMENTAL) -c $(BUILD_PACKAGE_CHECK) -r $(BUILD_RPMEM)\
			-f $(TEST_CONFIG_FILE) -n $(NDCTL_ENABLE) -p $(PMEM2_INSTALL)

install: all

install uninstall:
	$(MAKE) -C src $@
	$(MAKE) -C doc $@

.PHONY: all clean clobber test check cstyle check-license install uninstall\
	source rpm dpkg pkg-clean pcheck check-remote format doc require-rpmem\
	$(SUBDIRS)
