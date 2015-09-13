#
# Copyright (c) 2014-2015, Intel Corporation
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
#     * Neither the name of Intel Corporation nor the names of its
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

#
# Makefile -- build all benchmarks
#

BENCHMARK = pmembench

GLIB:= $(shell pkg-config glib-2.0 && echo "y" || echo "n")
ifeq ($(GLIB),y)
TARGET = $(BENCHMARK)
else
TARGET = no-glib
endif

SRC=pmembench.c\
    benchmark_time.c\
    benchmark_worker.c\
    clo.c\
    clo_vec.c\
    config_reader.c\
    scenario.c\
    log.c\
    pmem_memset.c

# Configuration file without the .cfg extension
CONFIGS=pmembench_log\
	pmembench_memset

LIBS_PATH=../nondebug
CSTYLE = ../../utils/cstyle
OBJS=$(SRC:.c=.o)
LDFLAGS = -L$(LIBS_PATH)
LDFLAGS += $(EXTRA_LDFLAGS)
LIBS += -lpmemlog -lpmem -pthread -lm
CFLAGS = -std=gnu99 -Wall -Werror -Wmissing-prototypes -pthread $(EXTRA_CFLAGS)
CFLAGS += -I../include
CFLAGS += $(shell pkg-config --cflags glib-2.0)
LIBS += $(shell pkg-config --libs glib-2.0)

ifeq ($(DEBUG),)
CFLAGS += -O3
else
CFLAGS += -ggdb
endif

default: $(TARGET)

no-glib:
	@echo "glib-2.0 development package required"

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

$(BENCHMARK): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	$(RM) $(OBJS)
	$(RM) *.csv

clobber: clean
	$(RM) $(BENCHMARK)

cstyle:
	$(CSTYLE) -pP *.[ch]

$(CONFIGS):
	LD_LIBRARY_PATH=$(LIBS_PATH) ./$(BENCHMARK) $@.cfg > $@.csv

run: $(BENCHMARK) $(CONFIGS)

.PHONY: all clean clobber run cstyle no-glib $(CONFIGS)
