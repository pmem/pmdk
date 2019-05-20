
ifeq ($(ARCH), ppc64)

# Add ppc64 directory to the gnu-make search path

vpath %.c $(TOP)/src/libpmem/ppc64
vpath %.h $(TOP)/src/libpmem/ppc64

# Include the ppc64 directory in gcc include search path
CFLAGS += -Ippc64

endif