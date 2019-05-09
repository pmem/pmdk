#
# Copyright 2015-2019, Intel Corporation
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
# examples/Makefile.inc -- build the Persistent Memory Development Kit examples
#
TOP_SRC := $(dir $(lastword $(MAKEFILE_LIST)))..
TOP := $(TOP_SRC)/..
HEADERS = $(wildcard *.h) $(wildcard *.hpp)

INCDIR = $(TOP_SRC)/include
LIBDIR = $(TOP_SRC)/debug

include $(TOP)/src/common.inc

CXXFLAGS = -std=c++11 -ggdb -Wall -Werror
CXXFLAGS += $(GLIBC_CXXFLAGS)
CXXFLAGS +=  $(EXTRA_CXXFLAGS)
CFLAGS = -std=gnu99 -ggdb -Wall -Werror -Wmissing-prototypes $(EXTRA_CFLAGS)
LDFLAGS = -Wl,-rpath=$(LIBDIR) -L$(LIBDIR) $(EXTRA_LDFLAGS)
ifneq ($(SANITIZE),)
CFLAGS += -fsanitize=$(SANITIZE)
CXXFLAGS += -fsanitize=$(SANITIZE)
LDFLAGS += -fsanitize=$(SANITIZE)
endif
INCS = -I$(INCDIR) -I. -I$(TOP_SRC)/examples $(OS_INCS)
LIBS += $(OS_LIBS) $(LIBUUID)

LINKER=$(CC)
ifeq ($(COMPILE_LANG), cpp)
LINKER=$(CXX)
endif

all-dirs:     TARGET = all
clean-dirs:   TARGET = clean
clobber-dirs: TARGET = clobber
cstyle-dirs:  TARGET = cstyle
format-dirs:  TARGET = format
sparse-dirs:  TARGET = sparse

all: $(if $(DIRS), all-dirs) $(if $(LIBRARIES), all-libraries) $(if $(PROGS), all-progs)
clean: $(if $(DIRS), clean-dirs) $(if $(PROGS), clean-progs) $(if $(LIBRARIES), clean-libraries)
clobber: $(if $(DIRS), clobber-dirs) $(if $(PROGS), clobber-progs) $(if $(LIBRARIES), clobber-libraries)
cstyle: $(if $(DIRS), cstyle-dirs)
format: $(if $(DIRS), format-dirs)
sparse: $(if $(DIRS), sparse-dirs)
	$(if $(DIRS), , $(sparse-c))

DYNAMIC_LIBRARIES = $(addprefix lib, $(addsuffix .so, $(LIBRARIES)))
STATIC_LIBRARIES = $(addprefix lib, $(addsuffix .a, $(LIBRARIES)))

all-dirs clean-dirs clobber-dirs cstyle-dirs format-dirs sparse-dirs: $(DIRS)
all-progs: $(PROGS)
all-libraries: $(DYNAMIC_LIBRARIES) $(STATIC_LIBRARIES)

$(foreach l, $(LIBRARIES), $(eval lib$(l).so: lib$(l).o))
$(foreach l, $(LIBRARIES), $(eval lib$(l).a: lib$(l).o))
$(foreach l, $(LIBRARIES), $(eval lib$(l).o: CFLAGS+=-fPIC))
$(foreach l, $(LIBRARIES), $(eval lib$(l).o: CXXFLAGS+=-fPIC))
$(foreach l, $(LIBRARIES), $(eval $(l): lib$(l).so lib$(l).a))
$(foreach l, $(LIBRARIES), $(eval .PHONY: $(l)))

$(DIRS):
	$(MAKE) -C $@ $(TARGET)

clobber-progs: clean-progs
clobber-libraries: clean-libraries

clobber-progs clobber-libraries:
ifneq ($(PROGS),)
	$(RM) $(PROGS)
endif
ifneq ($(LIBRARIES),)
	$(RM) $(DYNAMIC_LIBRARIES) $(STATIC_LIBRARIES)
endif

clean-progs clean-libraries:
	$(RM) *.o $(TMP_HEADERS)

MAKEFILE_DEPS=Makefile $(TOP)/src/examples/Makefile.inc $(TOP)/src/common.inc

ifneq ($(HEADERS),)
ifneq ($(filter 1 2, $(CSTYLEON)),)
TMP_HEADERS := $(addsuffix tmp, $(HEADERS))
endif
endif

ifeq ($(COVERAGE),1)
CFLAGS += $(GCOV_CFLAGS)
LDFLAGS += $(GCOV_LDFLAGS)
LIBS += $(GCOV_LIBS)
endif

all: $(TMP_HEADERS)

%.o: %.c $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	$(CC) -c -o $@ $(CFLAGS) $(INCS) $<

%.o: %.cpp $(MAKEFILE_DEPS)
	$(call check-cstyle, $<)
	$(CXX) -c -o $@ $(CXXFLAGS) $(INCS) $<

%.htmp: %.h
	$(call check-cstyle, $<, $@)

%.hpptmp: %.hpp
	$(call check-cstyle, $<, $@)

$(PROGS): | $(TMP_HEADERS)
	LD_LIBRARY_PATH=$(LIBNDCTL_LD_LIBRARY_PATHS):$(LD_LIBRARY_PATH) $(LINKER) -o $@ $^ $(LDFLAGS) $(LIBS)

lib%.o:
	$(LD) -o $@ -r $^

$(STATIC_LIBRARIES):
	$(AR) rv $@ $<

$(DYNAMIC_LIBRARIES):
	$(LINKER) -shared -o $@ $(LDFLAGS) -Wl,-shared,-soname=$@ $(LIBS) $<

.PHONY: all clean clobber cstyle\
	all-dirs clean-dirs clobber-dirs cstyle-dirs\
	all-progs clean-progs clobber-progs cstyle-progs\
	$(DIRS)
