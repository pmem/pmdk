#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmem.h>

int
main(int argc, char *argv[])
{
	int fd;
	char *pmaddr;

	/* memory map some persistent memory */
	if ((fd = open("/my/pmem-aware/fs/myfile", O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	/* just map 4k for this example */
	if ((pmaddr = mmap(NULL, 4096, PROT_READ|PROT_WRITE,
				MAP_SHARED, fd, 0)) == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	close(fd);

	/* store a string to the persistent memory */
	strcpy(pmaddr, "hello, persistent memory");

	/*
	 * The above stores may or may not be sitting in cache at
	 * this point, depending on other system activity causing
	 * cache pressure.  Now force the change to be durable
	 * (flushed all the say to the persistent memory).  If
	 * unsure whether the file is really persistent memory,
	 * use pmem_is_pmem() to decide whether pmem_persist() can
	 * be used, or whether msync() must be used.
	 */
	if (pmem_is_pmem(pmaddr, 4096))
		pmem_persist(pmaddr, 4096, 0);
	else
		msync(pmaddr, 4096, MS_SYNC);
}
