#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libvmem.h>

int
main(int argc, char *argv[])
{
	VMEM *vmp;
	char *ptr;

	/* create minimum size pool of memory */
	if ((vmp = vmem_pool_create("/my/pmem-aware/fs",
					VMEM_MIN_POOL)) == NULL) {
		perror("vmem_pool_create");
		exit(1);
	}

	if ((ptr = vmem_malloc(vmp, 100)) == NULL) {
		perror("vmem_malloc");
		exit(1);
	}

	strcpy(ptr, "hello, world");

	/* give the memory back */
	vmem_free(vmp, ptr);

	/* ... */
}
