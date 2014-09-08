#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmem.h>

/* log processing callback for use with pmemlog_walk() */
int
printit(const void *buf, size_t len, void *arg)
{
	fwrite(buf, len, 1, stdout);
	return 0;
}

int
main(int argc, char *argv[])
{
	int fd;
	PMEMlog *plp;
	size_t nbyte;
	char *str;

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

	/* create a persistent memory resident log */
	if ((plp = pmemlog_map(fd)) == NULL) {
		perror("pmemlog_map");
		exit(1);
	}

	/* how many bytes does the log hold? */
	nbyte = pmemlog_nbyte(plp);
	printf("log holds %zu bytes\n", nbyte);

	/* append to the log... */
	str = "This is the first string appended\n";
	if (pmemlog_append(plp, str, strlen(str)) < 0) {
		perror("pmemlog_append");
		exit(1);
	}
	str = "This is the second string appended\n";
	if (pmemlog_append(plp, str, strlen(str)) < 0) {
		perror("pmemlog_append");
		exit(1);
	}

	/* print the log contents */
	printf("log contains:\n");
	pmemlog_walk(plp, 0, printit, NULL);

	pmemlog_unmap(plp);
	close(fd);
}
