# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation

check_package = $(shell pkg-config $(1) && echo y || echo n)

HAS_LIBPMEMOBJ := $(call check_package, libpmemobj --atleast-version $(LIBPMEMOBJ_MIN_VERSION) --print-errors)

LIBS += $(shell pkg-config --libs libpmemobj)
CFLAGS += $(shell pkg-config --cflags libpmemobj)

ifeq ($(HAS_LIBPMEMOBJ),n)
$(error libpmemobj(version >= $(LIBPMEMOBJ_MIN_VERSION)) is missing)
endif

test_package: test_package.c
	$(CC) test_package.c $(LIBS) $(CFLAGS) -o $@

clean:
	rm -f test_package

.PHONY: clean
