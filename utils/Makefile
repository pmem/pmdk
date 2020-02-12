# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

rwildcard=$(strip $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2)\
	$(filter $(subst *,%,$2),$d)))

SCRIPTS = $(call rwildcard,,*.sh)

cstyle:
	./check-shebang.sh $(SCRIPTS)

.PHONY: cstyle
