# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2020, Intel Corporation
#

#
# src/test/Makefile.inc -- common Makefile defs for unit tests
#
# These defaults apply to most unit tests.  The individual Makefile
# for each unit test overrides the defaults as necessary.
#

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../..

include $(TOP)/src/common.inc

INCS += $(OS_INCS)
LDFLAGS += $(OS_LIBS)
LIBS_DIR=$(TOP)/src

EXAMPLES_DIR=$(TOP)/src/examples
EX_LIBPMEM=$(EXAMPLES_DIR)/libpmem
EX_LIBPMEM2=$(EXAMPLES_DIR)/libpmem2
EX_LIBPMEMBLK=$(EXAMPLES_DIR)/libpmemblk
EX_LIBPMEMLOG=$(EXAMPLES_DIR)/libpmemlog
EX_LIBPMEMOBJ=$(EXAMPLES_DIR)/libpmemobj
EX_PMREORDER=$(EXAMPLES_DIR)/pmreorder

UT = ../unittest/libut.a
LIBS += $(UT) $(LIBUUID)

ifeq ($(USE_LIBUNWIND),y)
LIBS += $(LIBDL) $(LIBUNWIND_LIBS)
endif

LIBS += -L$(LIBS_DIR)/debug
LIBS += -pthread $(LIBUTIL)

ifeq ($(LIBRT_NEEDED), y)
LIBS += -lrt
endif

ifeq ($(LIBPMEMPOOL), y)
LIBPMEM=y
DYNAMIC_LIBS += -lpmempool
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmempool.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmempool.a
CFLAGS += -DUSE_LIBDL
endif

ifeq ($(LIBPMEMBLK), y)
LIBPMEM=y
DYNAMIC_LIBS += -lpmemblk
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemblk.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemblk.a
endif

ifeq ($(LIBPMEMLOG), y)
LIBPMEM=y
DYNAMIC_LIBS += -lpmemlog
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemlog.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemlog.a
endif

ifeq ($(LIBPMEMOBJ), y)
LIBPMEM=y
DYNAMIC_LIBS += -lpmemobj
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemobj.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemobj.a
endif

ifeq ($(LIBPMEMOBJ), internal-debug)
LIBPMEM=y
LIBPMEMCOMMON=internal-debug
OBJS += $(TOP)/src/debug/common/ravl.o\
	$(TOP)/src/debug/libpmemobj/alloc_class.o\
	$(TOP)/src/debug/libpmemobj/bucket.o\
	$(TOP)/src/debug/libpmemobj/container_ravl.o\
	$(TOP)/src/debug/libpmemobj/container_seglists.o\
	$(TOP)/src/debug/libpmemobj/critnib.o\
	$(TOP)/src/debug/libpmemobj/ctl_debug.o\
	$(TOP)/src/debug/libpmemobj/heap.o\
	$(TOP)/src/debug/libpmemobj/lane.o\
	$(TOP)/src/debug/libpmemobj/libpmemobj.o\
	$(TOP)/src/debug/libpmemobj/list.o\
	$(TOP)/src/debug/libpmemobj/memblock.o\
	$(TOP)/src/debug/libpmemobj/memops.o\
	$(TOP)/src/debug/libpmemobj/obj.o\
	$(TOP)/src/debug/libpmemobj/palloc.o\
	$(TOP)/src/debug/libpmemobj/pmalloc.o\
	$(TOP)/src/debug/libpmemobj/recycler.o\
	$(TOP)/src/debug/libpmemobj/ulog.o\
	$(TOP)/src/debug/libpmemobj/sync.o\
	$(TOP)/src/debug/libpmemobj/tx.o\
	$(TOP)/src/debug/libpmemobj/stats.o

INCS += -I$(TOP)/src/libpmemobj
endif

ifeq ($(LIBPMEMOBJ), internal-nondebug)
LIBPMEM=y
LIBPMEMCOMMON=internal-nondebug
OBJS +=	$(TOP)/src/nondebug/common/ravl.o\
	$(TOP)/src/nondebug/libpmemobj/alloc_class.o\
	$(TOP)/src/nondebug/libpmemobj/bucket.o\
	$(TOP)/src/nondebug/libpmemobj/container_ravl.o\
	$(TOP)/src/nondebug/libpmemobj/container_seglists.o\
	$(TOP)/src/nondebug/libpmemobj/critnib.o\
	$(TOP)/src/nondebug/libpmemobj/ctl_debug.o\
	$(TOP)/src/nondebug/libpmemobj/heap.o\
	$(TOP)/src/nondebug/libpmemobj/lane.o\
	$(TOP)/src/nondebug/libpmemobj/libpmemobj.o\
	$(TOP)/src/nondebug/libpmemobj/list.o\
	$(TOP)/src/nondebug/libpmemobj/memblock.o\
	$(TOP)/src/nondebug/libpmemobj/memops.o\
	$(TOP)/src/nondebug/libpmemobj/obj.o\
	$(TOP)/src/nondebug/libpmemobj/palloc.o\
	$(TOP)/src/nondebug/libpmemobj/pmalloc.o\
	$(TOP)/src/nondebug/libpmemobj/recycler.o\
	$(TOP)/src/nondebug/libpmemobj/ulog.o\
	$(TOP)/src/nondebug/libpmemobj/sync.o\
	$(TOP)/src/nondebug/libpmemobj/tx.o\
	$(TOP)/src/nondebug/libpmemobj/stats.o

INCS += -I$(TOP)/src/libpmemobj
endif

ifeq ($(LIBPMEM), internal-nondebug)
OBJS +=\
	$(TOP)/src/nondebug/libpmem/libpmem.o\
	$(TOP)/src/nondebug/libpmem/memops_generic.o\
	$(TOP)/src/nondebug/libpmem/pmem.o\
	$(TOP)/src/nondebug/libpmem/pmem_posix.o

include $(TOP)/src/libpmem2/$(ARCH)/sources.inc
OBJS_MEM = $(LIBPMEM2_ARCH_SOURCE:.c=.o)
OBJS += $(addprefix $(TOP)/src/nondebug/libpmem/, ${OBJS_MEM})

INCS += -I$(TOP)/src/libpmem
INCS += -I$(TOP)/src/libpmem2
INCS += -I$(TOP)/src/libpmem2/$(ARCH)
endif

ifeq ($(LIBPMEM), internal-debug)
OBJS +=\
	$(TOP)/src/debug/libpmem/libpmem.o\
	$(TOP)/src/debug/libpmem/memops_generic.o\
	$(TOP)/src/debug/libpmem/pmem.o\
	$(TOP)/src/debug/libpmem/pmem_posix.o

include $(TOP)/src/libpmem2/$(ARCH)/sources.inc
OBJS_MEM = $(LIBPMEM2_ARCH_SOURCE:.c=.o)
OBJS += $(addprefix $(TOP)/src/debug/libpmem/, ${OBJS_MEM})

INCS += -I$(TOP)/src/libpmem
INCS += -I$(TOP)/src/libpmem2
INCS += -I$(TOP)/src/libpmem2/$(ARCH)
endif

ifeq ($(LIBPMEM2), internal-debug)
LIBPMEMCORE=internal-debug
OBJS +=\
	$(TOP)/src/debug/libpmem2/badblocks.o\
	$(TOP)/src/debug/libpmem2/badblocks_$(OS_DIMM).o\
	$(TOP)/src/debug/libpmem2/config.o\
	$(TOP)/src/debug/libpmem2/errormsg.o\
	$(TOP)/src/debug/libpmem2/libpmem2.o\
	$(TOP)/src/debug/libpmem2/map.o\
	$(TOP)/src/debug/libpmem2/map_posix.o\
	$(TOP)/src/debug/libpmem2/memops_generic.o\
	$(TOP)/src/debug/libpmem2/persist.o\
	$(TOP)/src/debug/libpmem2/persist_posix.o\
	$(TOP)/src/debug/libpmem2/pmem2_utils.o\
	$(TOP)/src/debug/libpmem2/pmem2_utils_$(OS_DIMM).o\
	$(TOP)/src/debug/libpmem2/ravl.o\
	$(TOP)/src/debug/libpmem2/source.o\
	$(TOP)/src/debug/libpmem2/source_posix.o\
	$(TOP)/src/debug/libpmem2/usc_$(OS_DIMM).o\
	$(TOP)/src/debug/libpmem2/vm_reservation.o\
	$(TOP)/src/debug/libpmem2/vm_reservation_posix.o\
	$(TOP)/src/debug/libpmem2/ravl_interval.o

ifeq ($(OS_KERNEL_NAME),Linux)
OBJS +=\
	$(TOP)/src/debug/libpmem2/auto_flush_linux.o\
	$(TOP)/src/debug/libpmem2/deep_flush_linux.o\
	$(TOP)/src/debug/libpmem2/extent_linux.o\
	$(TOP)/src/debug/libpmem2/pmem2_utils_linux.o
else
OBJS +=\
	$(TOP)/src/debug/libpmem2/auto_flush_none.o\
	$(TOP)/src/debug/libpmem2/extent_none.o\
	$(TOP)/src/debug/libpmem2/pmem2_utils_other.o\
	$(TOP)/src/debug/libpmem2/deep_flush_other.o
endif

ifeq ($(OS_DIMM),ndctl)
OBJS += $(TOP)/src/debug/libpmem2/region_namespace_ndctl.o
else
OBJS += $(TOP)/src/debug/libpmem2/region_namespace_none.o
endif

include $(TOP)/src/libpmem2/$(ARCH)/sources.inc
OBJS_MEM = $(LIBPMEM2_ARCH_SOURCE:.c=.o)
OBJS += $(addprefix $(TOP)/src/nondebug/libpmem2/, ${OBJS_MEM})

INCS += -I$(TOP)/src/common
INCS += -I$(TOP)/src/libpmem2
INCS += -I$(TOP)/src/libpmem2/$(ARCH)
endif

ifeq ($(LIBPMEM2), internal-nondebug)
LIBPMEMCORE=internal-nondebug
OBJS +=\
	$(TOP)/src/nondebug/libpmem2/libpmem2.o\
	$(TOP)/src/nondebug/libpmem2/badblocks.o\
	$(TOP)/src/nondebug/libpmem2/badblocks_$(OS_DIMM).o\
	$(TOP)/src/nondebug/libpmem2/config.o\
	$(TOP)/src/nondebug/libpmem2/source.o\
	$(TOP)/src/nondebug/libpmem2/source_posix.o\
	$(TOP)/src/nondebug/libpmem2/errormsg.o\
	$(TOP)/src/nondebug/libpmem2/map.o\
	$(TOP)/src/nondebug/libpmem2/map_posix.o\
	$(TOP)/src/nondebug/libpmem2/memops_generic.o\
	$(TOP)/src/nondebug/libpmem2/persist.o\
	$(TOP)/src/nondebug/libpmem2/persist_posix.o\
	$(TOP)/src/nondebug/libpmem2/pmem2_utils.o\
	$(TOP)/src/nondebug/libpmem2/pmem2_utils_$(OS_DIMM).o\
	$(TOP)/src/nondebug/libpmem2/ravl.o\
	$(TOP)/src/nondebug/libpmem2/usc_$(OS_DIMM).o\
	$(TOP)/src/nondebug/libpmem2/vm_reservation.o\
	$(TOP)/src/nondebug/libpmem2/vm_reservation_posix.o\
	$(TOP)/src/nondebug/libpmem2/ravl_interval.o

ifeq ($(OS_KERNEL_NAME),Linux)
OBJS +=\
	$(TOP)/src/nondebug/libpmem2/auto_flush_linux.o\
	$(TOP)/src/nondebug/libpmem2/deep_flush_linuc.o\
	$(TOP)/src/nondebug/libpmem2/extent_linux.o\
	$(TOP)/src/nondebug/libpmem2/pmem2_utils_linux.o
else
OBJS +=\
	$(TOP)/src/nondebug/libpmem2/auto_flush_none.o\
	$(TOP)/src/nondebug/libpmem2/extent_none.o\
	$(TOP)/src/nondebug/libpmem2/pmem2_utils_other.o
	$(TOP)/src/nondebug/libpmem2/deep_flush_other.o
endif

ifeq ($(OS_DIMM),ndctl)
OBJS += $(TOP)/src/nondebug/libpmem2/region_namespace_ndctl.o
else
OBJS += $(TOP)/src/nondebug/libpmem2/region_namespace_none.o
endif

include $(TOP)/src/libpmem2/$(ARCH)/sources.inc
OBJS_MEM = $(LIBPMEM2_ARCH_SOURCE:.c=.o)
OBJS += $(addprefix $(TOP)/src/debug/libpmem2/, ${OBJS_MEM})

INCS += -I$(TOP)/src/common
INCS += -I$(TOP)/src/libpmem2
INCS += -I$(TOP)/src/libpmem2/$(ARCH)
endif

ifeq ($(LIBPMEM2),y)
DYNAMIC_LIBS += -lpmem2
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmem2.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmem2.a
endif

ifeq ($(LIBPMEMCOMMON), y)
LIBPMEM=y
LIBPMEMCORE=y
OBJS += $(LIBS_DIR)/debug/libpmemcommon.a
INCS += -I$(TOP)/src/common
endif

ifeq ($(LIBPMEMCOMMON), internal-nondebug)
LIBPMEMCORE=internal-nondebug
OBJS +=\
	$(TOP)/src/nondebug/common/bad_blocks.o\
	$(TOP)/src/nondebug/common/set_badblocks.o\
	$(TOP)/src/nondebug/common/ctl.o\
	$(TOP)/src/nondebug/common/ctl_prefault.o\
	$(TOP)/src/nondebug/common/ctl_sds.o\
	$(TOP)/src/nondebug/common/ctl_fallocate.o\
	$(TOP)/src/nondebug/common/ctl_cow.o\
	$(TOP)/src/nondebug/common/file.o\
	$(TOP)/src/nondebug/common/file_posix.o\
	$(TOP)/src/nondebug/common/mmap.o\
	$(TOP)/src/nondebug/common/mmap_posix.o\
	$(TOP)/src/nondebug/common/os_deep_linux.o\
	$(TOP)/src/nondebug/common/pool_hdr.o\
	$(TOP)/src/nondebug/common/set.o\
	$(TOP)/src/nondebug/common/shutdown_state.o\
	$(TOP)/src/nondebug/common/util.o\
	$(TOP)/src/nondebug/common/util_posix.o\
	$(TOP)/src/nondebug/common/uuid.o\
	$(call osdep, $(TOP)/src/nondebug/common/uuid,.o)\
	$(TOP)/src/nondebug/libpmem2/pmem2_utils.o\
	$(TOP)/src/nondebug/libpmem2/pmem2_utils_$(OS_DIMM).o\
	$(TOP)/src/nondebug/libpmem2/config.o\
	$(TOP)/src/nondebug/libpmem2/persist_posix.o\
	$(TOP)/src/nondebug/libpmem2/badblocks.o\
	$(TOP)/src/nondebug/libpmem2/badblocks_$(OS_DIMM).o\
	$(TOP)/src/nondebug/libpmem2/usc_$(OS_DIMM).o\
	$(TOP)/src/nondebug/libpmem2/source.o\
	$(TOP)/src/nondebug/libpmem2/source_posix.o

ifeq ($(OS_KERNEL_NAME),Linux)
OBJS +=\
	$(TOP)/src/nondebug/libpmem2/auto_flush_linux.o\
	$(TOP)/src/nondebug/libpmem2/deep_flush_linux.o\
	$(TOP)/src/nondebug/libpmem2/extent_linux.o\
	$(TOP)/src/nondebug/libpmem2/pmem2_utils_linux.o
else
OBJS +=\
	$(TOP)/src/nondebug/libpmem2/auto_flush_none.o\
	$(TOP)/src/nondebug/libpmem2/extent_none.o\
	$(TOP)/src/nondebug/libpmem2/pmem2_utils_other.o\
	$(TOP)/src/nondebug/libpmem2/deep_flush_other.o
endif

ifeq ($(OS_DIMM),ndctl)
OBJS += $(TOP)/src/nondebug/libpmem2/region_namespace_ndctl.o
else
OBJS += $(TOP)/src/nondebug/libpmem2/region_namespace_none.o
endif

INCS += -I$(TOP)/src/common
endif

ifeq ($(LIBPMEMCOMMON), internal-debug)
LIBPMEMCORE=internal-debug
OBJS +=\
	$(TOP)/src/debug/common/bad_blocks.o\
	$(TOP)/src/debug/common/set_badblocks.o\
	$(TOP)/src/debug/common/ctl.o\
	$(TOP)/src/debug/common/ctl_prefault.o\
	$(TOP)/src/debug/common/ctl_sds.o\
	$(TOP)/src/debug/common/ctl_fallocate.o\
	$(TOP)/src/debug/common/ctl_cow.o\
	$(TOP)/src/debug/common/file.o\
	$(TOP)/src/debug/common/file_posix.o\
	$(TOP)/src/debug/common/mmap.o\
	$(TOP)/src/debug/common/mmap_posix.o\
	$(TOP)/src/debug/common/os_deep_linux.o\
	$(TOP)/src/debug/common/pool_hdr.o\
	$(TOP)/src/debug/common/set.o\
	$(TOP)/src/debug/common/shutdown_state.o\
	$(TOP)/src/debug/common/uuid.o\
	$(call osdep, $(TOP)/src/debug/common/uuid,.o)\
	$(TOP)/src/debug/libpmem2/pmem2_utils.o\
	$(TOP)/src/debug/libpmem2/pmem2_utils_$(OS_DIMM).o\
	$(TOP)/src/debug/libpmem2/config.o\
	$(TOP)/src/debug/libpmem2/persist_posix.o\
	$(TOP)/src/debug/libpmem2/badblocks.o\
	$(TOP)/src/debug/libpmem2/badblocks_$(OS_DIMM).o\
	$(TOP)/src/debug/libpmem2/source.o\
	$(TOP)/src/debug/libpmem2/source_posix.o\
	$(TOP)/src/debug/libpmem2/usc_$(OS_DIMM).o

ifeq ($(OS_KERNEL_NAME),Linux)
OBJS +=\
	$(TOP)/src/debug/libpmem2/auto_flush_linux.o\
	$(TOP)/src/debug/libpmem2/deep_flush_linux.o\
	$(TOP)/src/debug/libpmem2/extent_linux.o\
	$(TOP)/src/debug/libpmem2/pmem2_utils_linux.o
else
OBJS +=\
	$(TOP)/src/debug/libpmem2/auto_flush_none.o\
	$(TOP)/src/debug/libpmem2/extent_none.o\
	$(TOP)/src/debug/libpmem2/pmem2_utils_other.o\
	$(TOP)/src/debug/libpmem2/deep_flush_other.o
	endif

ifeq ($(OS_DIMM),ndctl)
OBJS += $(TOP)/src/debug/libpmem2/region_namespace_ndctl.o
else
OBJS += $(TOP)/src/debug/libpmem2/region_namespace_none.o
endif

INCS += -I$(TOP)/src/common
endif

ifeq ($(LIBPMEMCORE), y)
LIBPMEM=y
OBJS += $(LIBS_DIR)/debug/libpmemcore.a
INCS += -I$(TOP)/src/core
endif

ifeq ($(LIBPMEMCORE), internal-nondebug)
OBJS +=\
	$(TOP)/src/nondebug/core/alloc.o\
	$(TOP)/src/nondebug/core/fs_posix.o\
	$(TOP)/src/nondebug/core/os_posix.o\
	$(TOP)/src/nondebug/core/os_thread_posix.o\
	$(TOP)/src/nondebug/core/out.o\
	$(TOP)/src/nondebug/core/util.o\
	$(TOP)/src/nondebug/core/util_posix.o

OBJS += $(LIBS_DIR)/debug/libpmemcore.a
INCS += -I$(TOP)/src/core
endif

ifeq ($(LIBPMEMCORE), internal-debug)
OBJS +=\
	$(TOP)/src/debug/core/alloc.o\
	$(TOP)/src/debug/core/fs_posix.o\
	$(TOP)/src/debug/core/os_posix.o\
	$(TOP)/src/debug/core/os_thread_posix.o\
	$(TOP)/src/debug/core/out.o\
	$(TOP)/src/debug/core/util.o\
	$(TOP)/src/debug/core/util_posix.o

OBJS += $(LIBS_DIR)/debug/libpmemcore.a
INCS += -I$(TOP)/src/core
endif

ifeq ($(LIBPMEMSET), y)
DYNAMIC_LIBS += -lpmemset
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmemset.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmemset.a
endif

ifeq ($(LIBPMEMSET), internal-debug)
OBJS +=\
	$(TOP)/src/debug/libpmemset/libpmemset.o

INCS += -I$(TOP)/src/libpmemset
endif

ifeq ($(LIBPMEMSET), internal-nondebug)
OBJS +=\
	$(TOP)/src/nondebug/libpmemset/libpmemset.o

INCS += -I$(TOP)/src/libpmemset
endif

ifeq ($(LIBPMEM),y)
DYNAMIC_LIBS += -lpmem
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/libpmem.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/libpmem.a
endif

ifeq ($(LIBRPMEM),y)
DYNAMIC_LIBS += -lrpmem
STATIC_DEBUG_LIBS += $(LIBS_DIR)/debug/librpmem.a
STATIC_NONDEBUG_LIBS += $(LIBS_DIR)/nondebug/librpmem.a
endif

ifneq ($(LIBPMEMCOMMON)$(LIBPMEM)$(LIBPMEM2)$(LIBPMEMPOOL)$(LIBPMEMBLK)$(LIBPMEMLOG)$(LIBPMEMOBJ)$(LIBRPMEM),)
LIBS += -pthread
endif

ifneq ($(LIBPMEMCOMMON)$(LIBPMEMPOOL)$(LIBPMEMOBJ),)
LIBS += $(LIBDL)
endif

ifneq ($(LIBPMEM)$(LIBPMEM2)$(LIBPMEMCOMMON)$(LIBPMEMPOOL)$(LIBPMEMBLK)$(LIBPMEMLOG)$(LIBPMEMOBJ),)
CFLAGS += $(LIBNDCTL_CFLAGS)
LIBS += $(LIBNDCTL_LIBS)
endif

#
# This is a helper function to be combined with usage of macros available
# in the unittest framework. It scans the code for functions that should be
# wrapped and adds required linker flags.
#
PAREN=(
extract_funcs = $(shell \
    awk -F '[$(PAREN),]' \
    '/(FUNC_MOCK_RET_ALWAYS|FUNC_MOCK_RET_ALWAYS_VOID|FUNC_MOCK)\$(PAREN)[^,]/ \
    { \
        print "-Wl,--wrap=" $$2 \
    }' $(1) )

INCS += -I../unittest -I$(TOP)/src/include -I$(TOP)/src/common -I$(TOP)/src/core

COMMON_FLAGS  = -ggdb
COMMON_FLAGS += -Wall
COMMON_FLAGS += -Werror
COMMON_FLAGS += -Wpointer-arith

ifeq ($(IS_ICC), n)
COMMON_FLAGS += -Wunused-macros
endif
COMMON_FLAGS += -fno-common

CXXFLAGS  = -std=c++11
CXXFLAGS += $(GLIBC_CXXFLAGS)
CXXFLAGS += -ggdb
CXXFLAGS += $(COMMON_FLAGS)
CXXFLAGS += $(EXTRA_CXXFLAGS)

CFLAGS  = -std=gnu99
CFLAGS += -Wmissing-prototypes
CFLAGS += $(COMMON_FLAGS)

CFLAGS += -Wsign-conversion

ifeq ($(WUNREACHABLE_CODE_RETURN_AVAILABLE), y)
CFLAGS += -Wunreachable-code-return
endif

ifeq ($(WMISSING_VARIABLE_DECLARATIONS_AVAILABLE), y)
CFLAGS += -Wmissing-variable-declarations
endif

ifeq ($(WFLOAT_EQUAL_AVAILABLE), y)
CFLAGS += -Wfloat-equal
endif

ifeq ($(WCAST_FUNCTION_TYPE_AVAILABLE), y)
CFLAGS += -Wcast-function-type
endif

ifeq ($(LINK_NDCTL),y)
CFLAGS += $(LIBNDCTL_CFLAGS)
LIBS += $(LIBNDCTL_LIBS)
endif

CFLAGS += $(EXTRA_CFLAGS)

LDFLAGS = -Wl,--warn-common -Wl,--fatal-warnings $(EXTRA_LDFLAGS)

ifeq ($(OS_DIMM),ndctl)
CFLAGS += $(OS_DIMM_CFLAG)
CFLAGS += -DSDS_ENABLED
endif

ifeq ($(COVERAGE),1)
CFLAGS += $(GCOV_CFLAGS)
CXXFLAGS += $(GCOV_CFLAGS)
LDFLAGS += $(GCOV_LDFLAGS)
LIBS += $(GCOV_LIBS)
endif

ifeq ($(VALGRIND),0)
CFLAGS += -DVALGRIND_ENABLED=0
CXXFLAGS += -DVALGRIND_ENABLED=0
endif

ifeq ($(FAULT_INJECTION),1)
CFLAGS += -DFAULT_INJECTION=1
CXXFLAGS += -DFAULT_INJECTION=1
endif

ifneq ($(SANITIZE),)
CFLAGS += -fsanitize=$(SANITIZE)
CXXFLAGS += -fsanitize=$(SANITIZE)
LDFLAGS += -fsanitize=$(SANITIZE)
endif

LINKER=$(CC)
ifeq ($(COMPILE_LANG), cpp)
LINKER=$(CXX)
endif

SUFFIX_STATIC_DEBUG=static-debug
SUFFIX_STATIC_NONDEBUG=static-nondebug

ifneq ($(TARGET),)
SCP_TARGET=$(TARGET)
SCP_SRC_DIR=.
#
# By default debug and non-debug static versions are built.
# It can be changed by setting BUILD_STATIC_DEBUG, BUILD_STATIC_NONDEBUG
# or BUILD_STATIC (for both of them) to 'n'.
#
ifneq ($(BUILD_STATIC),n)
ifneq ($(BUILD_STATIC_DEBUG),n)
TARGET_STATIC_DEBUG=$(TARGET).$(SUFFIX_STATIC_DEBUG)
SCP_TARGET_STATIC_DEBUG=$(SCP_SRC_DIR)/$(SCP_TARGET).$(SUFFIX_STATIC_DEBUG)
endif
ifneq ($(BUILD_STATIC_NONDEBUG),n)
ifneq ($(DEBUG),1)
TARGET_STATIC_NONDEBUG=$(TARGET).$(SUFFIX_STATIC_NONDEBUG)
SCP_TARGET_STATIC_NONDEBUG=$(SCP_SRC_DIR)/$(SCP_TARGET).$(SUFFIX_STATIC_NONDEBUG)
endif
endif
endif
endif

SCP=../sync-remotes/copy-to-remote-nodes.sh
TESTCONFIG=../testconfig.sh
SYNC_FILE=.synced

MAKEFILE_DEPS=Makefile ../Makefile.inc $(TOP)/src/common.inc

ifneq ($(HEADERS),)
ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addsuffix tmp, $(HEADERS))
endif
endif

all: $(TARGET) $(TARGET_STATIC_DEBUG) $(TARGET_STATIC_NONDEBUG)

$(UT):
	$(MAKE) -C ../unittest

$(TARGET_STATIC_DEBUG): $(TMP_HEADERS) $(OBJS) $(UT) $(STATIC_DEBUG_LIBS) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(STATIC_DEBUG_LIBS) $(LIBS)

$(TARGET_STATIC_NONDEBUG): $(TMP_HEADERS) $(OBJS) $(UT) $(STATIC_NONDEBUG_LIBS) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(STATIC_NONDEBUG_LIBS) $(LIBS)

$(TARGET): $(TMP_HEADERS) $(OBJS) $(UT) $(EXTRA_DEPS) $(MAKEFILE_DEPS)
	$(LINKER) -o $@ $(LDFLAGS) $(OBJS) $(DYNAMIC_LIBS) $(LIBS)

objdir=.

%.o: %.c $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	@mkdir -p .deps
	$(CC) -MD -c $(CFLAGS) $(INCS) $(call coverage-path, $<) -o $@
	$(call check-os, $@, $<)
	$(create-deps)

%.o: %.cpp $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	@mkdir -p .deps
	$(CXX) -MD -c $(CXXFLAGS) $(INCS) $(call coverage-path, $<) -o $@
	$(call check-os, $@, $<)
	$(create-deps)

%.htmp: %.h
	$(call check-cstyle, $<, $@)

clean:
	$(RM) *.o */*.o core *.core a.out *.log testfile* $(SYNC_FILE) $(TMP_HEADERS)

clobber: clean
	$(RM) $(TARGET) $(TARGET).$(SUFFIX_STATIC_DEBUG) $(TARGET).$(SUFFIX_STATIC_NONDEBUG)
	$(RM) -r .deps
	$(RM) -r __pycache__

$(TESTCONFIG):

$(SYNC_FILE): $(TARGET) $(TESTCONFIG)
ifeq ($(SCP_TO_REMOTE_NODES), y)
ifeq ($(SCP_TARGET),)
	$(SCP) test
else
ifeq ($(SCP_SRC_DIR),)
$(error SCP_SRC_DIR is not set)
endif
	$(SCP) common
	$(SCP) test $(SCP_SRC_DIR)/$(SCP_TARGET) $(SCP_TARGET_STATIC_DEBUG) $(SCP_TARGET_STATIC_NONDEBUG)
endif
	@touch $(SYNC_FILE)
endif

sync-test: all $(SYNC_FILE) $(TESTCONFIG)

TST=$(shell basename `pwd`)
TSTCHECKS=$(shell ls -1 TEST* 2> /dev/null | grep '^TEST[0-9]\+$$' | sort -V)
TSTCHECKSPY=$(shell test -f TESTS.py && cd .. && ./RUNTESTS.py --list-testcases $(TST) | while read name; do echo py/$$name; done)

$(TSTCHECKSPY):
	@cd .. && ./RUNTESTS.py $(subst nondebug,release,$(RUNTEST_OPTIONS)) -u $(shell echo $@ | sed 's/^py\/[^0-9]*\([0-9]*\)$$/\1/') -- ${TST}

$(TSTCHECKS): sync-test
	@cd .. && ./RUNTESTS ${TST} $(RUNTEST_OPTIONS) -s $@

check: sync-test
	@cd .. && ./RUNTESTS ${TST} $(RUNTEST_OPTIONS)

pcheck: export NOTTY=1

pcheck: $(TSTCHECKS)

pycheck: $(TSTCHECKSPY)

test: all

TOOLS=../tools

$(TOOLS)/anonymous_mmap/anonymous_mmap:
	$(MAKE) -C $(TOOLS)/anonymous_mmap all

ifeq ($(USE_ANONYMOUS_MMAP), y)
all: $(TOOLS)/anonymous_mmap/anonymous_mmap
endif

$(TOOLS)/pmemspoil/pmemspoil:
	$(MAKE) -C $(TOOLS)/pmemspoil all

ifeq ($(USE_PMEMSPOIL), y)
all: $(TOOLS)/pmemspoil/pmemspoil
endif

$(TOOLS)/bttcreate/bttcreate:
	$(MAKE) -C $(TOOLS)/bttcreate all

ifeq ($(USE_BTTCREATE), y)
all: $(TOOLS)/bttcreate/bttcreate
endif

$(TOOLS)/pmemwrite/pmemwrite:
	$(MAKE) -C $(TOOLS)/pmemwrite all

ifeq ($(USE_PMEMWRITE), y)
all: $(TOOLS)/pmemwrite/pmemwrite
endif

$(TOOLS)/pmemalloc/pmemalloc:
	$(MAKE) -C $(TOOLS)/pmemalloc all

ifeq ($(USE_PMEMALLOC), y)
all: $(TOOLS)/pmemalloc/pmemalloc
endif

$(TOOLS)/pmemobjcli/pmemobjcli:
	$(MAKE) -C $(TOOLS)/pmemobjcli all

ifeq ($(USE_PMEMOBJCLI), y)
all: $(TOOLS)/pmemobjcli/pmemobjcli
endif

$(TOOLS)/ddmap/ddmap:
	$(MAKE) -C $(TOOLS)/ddmap all

ifeq ($(USE_DDMAP), y)
all: $(TOOLS)/ddmap/ddmap
endif

$(TOOLS)/obj_verify/obj_verify:
	$(MAKE) -C $(TOOLS)/obj_verify all

ifeq ($(USE_OBJ_VERIFY), y)
all: $(TOOLS)/obj_verify/obj_verify
endif

$(TOOLS)/pmemdetect/pmemdetect.$(SUFFIX_STATIC_NONDEBUG):
	$(MAKE) -C $(TOOLS)/pmemdetect all

all: $(TOOLS)/pmemdetect/pmemdetect.$(SUFFIX_STATIC_NONDEBUG)

sparse:
	$(if $(TARGET), $(sparse-c))

.PHONY: all check clean clobber pcheck test sync-test $(TSTCHECKS)

-include .deps/*.P
