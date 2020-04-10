# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2020, Intel Corporation

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
	LD_LIBRARY_PATH=$(LIBFABRIC_LD_LIBRARY_PATHS):$(LIBNDCTL_LD_LIBRARY_PATHS):$(LD_LIBRARY_PATH) $(LINKER) -o $@ $^ $(LDFLAGS) $(LIBS)

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
