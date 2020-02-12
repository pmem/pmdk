# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# examples/libpmemobj/pmemobjfs/Makefile -- build simple fuse base on libpmemobj
#
TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../../../
include $(TOP)/src/common.inc

vpath %.c ../tree_map/
vpath %.c ../map/

FUSE_REQ_VER = 2.9.1

FUSE := $(call check_package, fuse --atleast-version $(FUSE_REQ_VER))
ifeq ($(FUSE), y)
LINKS = mkfs.pmemobjfs\
       pmemobjfs.tx_begin\
       pmemobjfs.tx_commit\
       pmemobjfs.tx_abort
PROGS = pmemobjfs
else
$(info NOTE: Skipping pmemobjfs because fuse (version >= $(FUSE_REQ_VER)) is missing \
-- see src/examples/libpmemobj/pmemobjfs/README for details.)
endif

all-progs: $(LINKS)
clean-progs: clean-links

include ../../Makefile.inc

LIBS += -lpmemobj -lpmem -pthread

ifeq ($(FUSE), y)
LIBS += $(shell $(PKG_CONFIG) --libs fuse)
CFLAGS += $(shell $(PKG_CONFIG) --cflags fuse)
endif

CFLAGS += -I../tree_map
CFLAGS += -I../map
CFLAGS += -DFUSE_USE_VERSION=28
CFLAGS += -DUSE_CTREE

$(LINKS): pmemobjfs
	$(LN) -sf pmemobjfs $@

clean-links:
	$(RM) $(LINKS)

pmemobjfs: pmemobjfs.o ../map/libmap_ctree.a
../map/libmap_ctree.a:
	$(MAKE) -C ../map map_ctree

.PHONY: clean-links
