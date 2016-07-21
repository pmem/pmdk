#
# Copyright 2016, Intel Corporation
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

#
# src/test/obj_include/Makefile -- build include tests
#
TARGET = obj_inc
OBJS =	obj_inc.o\
	obj_inc_atomic_base.tmpo obj_inc_atomic.tmpo\
	obj_inc_base.tmpo\
	obj_inc_iter_base.tmpo obj_inc_iter.tmpo\
	obj_inc_lists_atomic_base.tmpo obj_inc_lists_atomic.tmpo\
	obj_inc_pool_base.tmpo obj_inc_pool.tmpo\
	obj_inc_thread.tmpo\
	obj_inc_tx_base.tmpo obj_inc_tx.tmpo\
	obj_inc_types.tmpo

LIBPMEM=y
LIBPMEMOBJ=y

include ../Makefile.inc

%.tmpo: obj_inc.c Makefile
	@mkdir -p .deps
	$(CC) -MD -c $(CFLAGS) $(INCS) $< -o $@
	$(create-deps)

clean-tmpo:
	rm -f *.tmpo

clean: clean-tmpo

obj_inc.o:                      CFLAGS+=-DBUILD_MAIN

obj_inc_atomic_base.tmpo:       CFLAGS+=-include libpmemobj/atomic_base.h
obj_inc_atomic.tmpo:            CFLAGS+=-include libpmemobj/atomic.h

obj_inc_base.tmpo:              CFLAGS+=-include libpmemobj/base.h

obj_inc_iter_base.tmpo:         CFLAGS+=-include libpmemobj/iterator_base.h
obj_inc_iter.tmpo:              CFLAGS+=-include libpmemobj/iterator.h

obj_inc_lists_atomic_base.tmpo: CFLAGS+=-include libpmemobj/lists_atomic_base.h
obj_inc_lists_atomic.tmpo:      CFLAGS+=-include libpmemobj/lists_atomic.h

obj_inc_pool_base.tmpo:         CFLAGS+=-include libpmemobj/pool_base.h
obj_inc_pool.tmpo:              CFLAGS+=-include libpmemobj/pool.h

obj_inc_thread.tmpo:            CFLAGS+=-include libpmemobj/thread.h

obj_inc_tx_base.tmpo:           CFLAGS+=-include libpmemobj/tx_base.h
obj_inc_tx.tmpo:                CFLAGS+=-include libpmemobj/tx.h

obj_inc_types.tmpo:             CFLAGS+=-include libpmemobj/types.h
