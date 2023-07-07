# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2023, Intel Corporation

#
# Makefile -- top-level Makefile for PMDK
#
# Use "make" to build the library.
#
# Use "make doc" to build documentation.
#
# Use "make test" to build unit tests.
#
# Use "make check" to run unit tests.
#
# Use "make clean" to delete all intermediate files (*.o, etc).
#
# Use "make clobber" to delete everything re-buildable (binaries, etc.).
#
# Use "make gitclean" for a complete tree clean, save for test configs.
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
TEST_CONFIG_FILE ?= "$(CURDIR)"/src/test/testconfig.sh
DOC ?= y

rpm : override DESTDIR="$(CURDIR)/$(RPM_BUILDDIR)"
dpkg: override DESTDIR="$(CURDIR)/$(DPKG_BUILDDIR)"
rpm dpkg: override prefix=/usr

all: doc
	$(MAKE) -C src $@

doc:
ifeq ($(DOC),y)
	test -f .skip-doc || $(MAKE) -C doc all
endif

clean:
	$(MAKE) -C src $@
ifeq ($(DOC),y)
	test -f .skip-doc || $(MAKE) -C doc $@
endif
	$(RM) -r $(RPM_BUILDDIR) $(DPKG_BUILDDIR)
	$(RM) -f $(GIT_VERSION)

clobber:
	$(MAKE) -C src $@
ifeq ($(DOC),y)
	test -f .skip-doc || $(MAKE) -C doc $@
endif
	$(RM) -r $(RPM_BUILDDIR) $(DPKG_BUILDDIR) rpm dpkg
	$(RM) -f $(GIT_VERSION)

test check pcheck pycheck: all
	$(MAKE) -C src $@

check pcheck pycheck: check-doc

cstyle: check-commits check-whitespace check-license
	$(MAKE) -C src $@
	$(MAKE) -C utils $@

format:
	$(MAKE) -C src $@
	@echo Done.

check-whitespace:
	@echo Checking files for whitespace issues...
	@utils/check_whitespace -g
	@echo Done.

check-commits:
	@echo Checking commit messages...
	test -d .git && utils/check-commits.sh
	@echo Done.

check-license:
	@utils/check_license/check-headers.sh $(TOP) BSD-3-Clause
	@echo Done.

check-doc: doc
	./utils/check-manpages

sparse:
	$(MAKE) -C src sparse

gitclean:
	git clean -dfx -etestconfig.sh -etestconfig.py

source: clobber
	$(if "$(DESTDIR)", , $(error Please provide DESTDIR variable))
	+utils/copy-source.sh "$(DESTDIR)" $(SRCVERSION)

pkg-clean:
	$(RM) -r "$(DESTDIR)"

rpm dpkg: pkg-clean
	$(MAKE) source DESTDIR="$(DESTDIR)"
	+utils/build-$@.sh -t $(SRCVERSION) -s "$(DESTDIR)"/pmdk -w "$(DESTDIR)" -o $(CURDIR)/$@\
			-e $(EXPERIMENTAL) -c $(BUILD_PACKAGE_CHECK)\
			-f $(TEST_CONFIG_FILE) -n $(NDCTL_ENABLE)

install: all

install uninstall:
	$(MAKE) -C src $@
ifeq ($(DOC),y)
	$(MAKE) -C doc $@
endif

.PHONY: all clean clobber test check cstyle check-license install uninstall\
	source rpm dpkg pkg-clean pcheck format doc\
	$(SUBDIRS)
