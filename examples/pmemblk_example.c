#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmem.h>

/* size of each element in the PMEM pool (bytes) */
#define	ELEMENT_SIZE ((size_t)1024)

int
main(int argc, char *argv[])
{
	int fd;
	PMEMblk *pbp;
	size_t nelements;
	char buf[ELEMENT_SIZE];

	/* create file on PMEM-aware file system */
	if ((fd = open("/my/pmem-aware/fs/myfile",
					O_CREAT|O_RDWR, 0666)) < 0) {
		perror("open");
		exit(1);
	}

	/* pre-allocate 2GB of persistent memory */
	if ((errno = posix_fallocate(fd, (off_t)0,
					(size_t)1024 * 1024 * 1024 * 2)) != 0) {
		perror("posix_fallocate");
		exit(1);
	}

	/* create an array of atomically writable elements */
	if ((pbp = pmemblk_map(fd, ELEMENT_SIZE)) == NULL) {
		perror("pmemblk_map");
		exit(1);
	}

	/* how many elements fit into the PMEM pool? */
	nelements = pmemblk_nblock(pbp);
	printf("file holds %zu elements\n", nelements);

	/* store a block at index 5 */
	strcpy(buf, "hello, world");
	if (pmemblk_write(pbp, buf, 5) < 0) {
		perror("pmemblk_write");
		exit(1);
	}

	/* read the block at index 10 (reads as zeros initially) */
	if (pmemblk_read(pbp, buf, 10) < 0) {
		perror("pmemblk_write");
		exit(1);
	}

	/* zero out the block at index 5 */
	if (pmemblk_set_zero(pbp, 5) < 0) {
		perror("pmemblk_set_zero");
		exit(1);
	}

	/* ... */

	pmemblk_unmap(pbp);
	close(fd);
}
