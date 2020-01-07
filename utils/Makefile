# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2017, Intel Corporation

rwildcard=$(strip $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2)\
	$(filter $(subst *,%,$2),$d)))

SCRIPTS = $(call rwildcard,,*.sh)

all:
	$(MAKE) -C check_license $@

check-license:
	$(MAKE) -C check_license $@

clean:
	$(MAKE) -C check_license $@

clobber:
	$(MAKE) -C check_license $@

cstyle:
	$(MAKE) -C check_license $@
	./check-shebang.sh $(SCRIPTS)

format:
	$(MAKE) -C check_license $@

.PHONY: all check-license clean clobber cstyle format
