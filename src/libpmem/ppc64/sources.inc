ifeq ($(ARCH), ppc64)

LIBPMEM_ARCH_SOURCE += init.c cpuinfo.c 
LIBPMEM_ARCH_SOURCE += platform_p9.c

endif