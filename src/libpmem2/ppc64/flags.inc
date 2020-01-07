# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, IBM Corporation

# Add ppc64 directory to the gnu-make search path
vpath %.c $(TOP)/src/libpmem2/ppc64
vpath %.h $(TOP)/src/libpmem2/ppc64

# Include the ppc64 directory in gcc include search path
CFLAGS += -Ippc64
