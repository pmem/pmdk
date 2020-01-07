# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation

include ../../src/common.inc

CFLAGS += -std=gnu99
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wpointer-arith
CFLAGS += -Wunused-macros
CFLAGS += -Wmissing-field-initializers
CFLAGS += -Wsign-conversion
CFLAGS += -Wsign-compare

ifeq ($(WCONVERSION_AVAILABLE), y)
CFLAGS += -Wconversion
endif

CFLAGS += -fno-common

ifeq ($(WUNREACHABLE_CODE_RETURN_AVAILABLE), y)
CFLAGS += -Wunreachable-code-return
endif

ifeq ($(WMISSING_VARIABLE_DECLARATIONS_AVAILABLE), y)
CFLAGS += -Wmissing-variable-declarations
endif

ifeq ($(WFLOAT_EQUAL_AVAILABLE), y)
CFLAGS += -Wfloat-equal
endif

ifeq ($(WSWITCH_DEFAULT_AVAILABLE), y)
CFLAGS += -Wswitch-default
endif

ifeq ($(WCAST_FUNCTION_TYPE_AVAILABLE), y)
CFLAGS += -Wcast-function-type
endif

TARGET=check-license

all: $(TARGET)

$(TARGET): $(TARGET).o

clean:
	$(RM) -f *.o

clobber: clean
	$(RM) -f $(TARGET)

.PHONY: all clean clobber
