/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ctrld.c -- simple application which helps running tests on remote node.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>

#define	APP_NAME "ctrld"
#define	BUFF_SIZE 4096

/* table of signal names */
#define	SIGNAL_2_STR(sig) [sig] = #sig
static const char *signal2str[] = {
	SIGNAL_2_STR(SIGHUP),
	SIGNAL_2_STR(SIGINT),
	SIGNAL_2_STR(SIGQUIT),
	SIGNAL_2_STR(SIGILL),
	SIGNAL_2_STR(SIGTRAP),
	SIGNAL_2_STR(SIGABRT),
	SIGNAL_2_STR(SIGBUS),
	SIGNAL_2_STR(SIGFPE),
	SIGNAL_2_STR(SIGKILL),
	SIGNAL_2_STR(SIGUSR1),
	SIGNAL_2_STR(SIGSEGV),
	SIGNAL_2_STR(SIGUSR2),
	SIGNAL_2_STR(SIGPIPE),
	SIGNAL_2_STR(SIGALRM),
	SIGNAL_2_STR(SIGTERM),
	SIGNAL_2_STR(SIGSTKFLT),
	SIGNAL_2_STR(SIGCHLD),
	SIGNAL_2_STR(SIGCONT),
	SIGNAL_2_STR(SIGSTOP),
	SIGNAL_2_STR(SIGTSTP),
	SIGNAL_2_STR(SIGTTIN),
	SIGNAL_2_STR(SIGTTOU),
	SIGNAL_2_STR(SIGURG),
	SIGNAL_2_STR(SIGXCPU),
	SIGNAL_2_STR(SIGXFSZ),
	SIGNAL_2_STR(SIGVTALRM),
	SIGNAL_2_STR(SIGPROF),
	SIGNAL_2_STR(SIGWINCH),
	SIGNAL_2_STR(SIGPOLL),
	SIGNAL_2_STR(SIGPWR),
	SIGNAL_2_STR(SIGSYS)
};

struct inode_item {
	LIST_ENTRY(inode_item) next;
	unsigned long inode;
};

struct inodes {
	LIST_HEAD(inode_items, inode_item) head;
};

/*
 * usage -- print usage and exit with failure code
 */
static void
usage(void)
{
	printf("usage: %s <pid file> <cmd> [<arg>]\n", APP_NAME);
	printf("commands:\n");
	printf("  run  <command> [<args...>] -- run specified command\n");
	printf("  wait [<timeout>]           -- wait for command\n");
	printf("  wait_port <port>           -- wait until a port is opened\n");
	printf("  kill <signal>              -- send a signal to command\n");
	exit(EXIT_FAILURE);
}

/*
 * alloc_argv -- allocate NULL terminated list of arguments at specified offset
 */
static char **
alloc_argv(unsigned argc, char *argv[], unsigned off)
{
	if (argc < off)
		return NULL;

	unsigned nargc = argc - off;
	char **nargv = malloc((nargc + 1) * sizeof (char *));
	if (!nargv)
		return NULL;

	for (unsigned i = 0; i < nargc; i++)
		nargv[i] = argv[i + off];

	nargv[nargc] = NULL;

	return nargv;
}

/*
 * do_run -- execute the 'run' command
 */
static int
do_run(const char *pid_file, char *cmd, char *argv[])
{
	FILE *fh = fopen(pid_file, "w+");
	if (!fh) {
		perror(pid_file);
		return 1;
	}

	int fd = fileno(fh);
	if (fd == -1) {
		perror("fileno");
		goto err;
	}

	if (flock(fd, LOCK_EX | LOCK_NB)) {
		perror("flock");
		goto err;
	}

	if (daemon(1, 0)) {
		perror("daemon");
		goto err;
	}

	int child = fork();
	switch (child) {
	case -1:
		perror("fork");
		fprintf(fh, "-1r%d", errno);
		fclose(fh);
		goto err;
	case 0:
		execvp(cmd, argv);
		perror("execve");
		goto err;
	default:
		break;
	}

	if (fprintf(fh, "%d", child) < 0) {
		perror("fprintf");
		goto err;
	}

	if (fflush(fh)) {
		perror("fflush");
		goto err;
	}

	int ret = 0;
	if (waitpid(child, &ret, 0) == -1) {
		perror("waitpid");
		goto err;
	}

	if (WIFSIGNALED(ret)) {
		ret = 128 + WTERMSIG(ret);
	} else {
		ret = WEXITSTATUS(ret);
	}

	if (fseek(fh, 0, SEEK_SET)) {
		perror("fseek");
		goto err;
	}

	if (ftruncate(fileno(fh), 0)) {
		perror("ftruncate");
		goto err;
	}

	fprintf(fh, "%dr%d", child, ret);

	return 0;
err:
	fclose(fh);
	return 1;
}

/*
 * do_wait -- execute the 'wait' command
 */
static int
do_wait(char *pid_file, int timeout)
{
	int fd = open(pid_file, O_RDONLY);
	if (fd < 0) {
		perror(pid_file);
		return 1;
	}

	int ret;

	int t = 0;
	while ((timeout == -1 || t < timeout) &&
		flock(fd, LOCK_EX | LOCK_NB)) {
		sleep(1);
		t++;
	}

	FILE *fh = fdopen(fd, "r");
	if (!fh) {
		perror("fdopen");
		ret = 1;
		goto err;
	}

	pid_t pid;
	char r;
	int n = fscanf(fh, "%d%c%d", &pid, &r, &ret);
	if (n < 0) {
		perror("fscanf");
		ret = 1;
		goto err;
	}

	if (n == 2 || (n == 3 && r != 'r')) {
		fprintf(stderr, "invalid format of PID file\n");
		ret = 1;
		goto err;
	}

	if (n == 1) {
		if (timeout >= 0) {
			ret = -1;
			goto err;
		} else {
			fprintf(stderr, "missing return value\n");
			ret = 1;
			goto err;
		}
	}

err:
	close(fd);
	fclose(fh);
	return ret;
}

/*
 * do_kill -- execute the 'kill' command
 */
static int
do_kill(char *pid_file, int signo)
{
	FILE *fh = fopen(pid_file, "r");
	if (!fh) {
		perror(pid_file);
		return 1;
	}

	int ret;
	pid_t pid;
	int n = fscanf(fh, "%d", &pid);
	if (n == 0) {
		ret = 0;
		goto out;
	}

	/* do not fail if such process already does not exist */
	if (kill(pid, signo) && errno != ESRCH) {
		perror("kill");
		ret = 1;
		goto out;
	}

	ret = 0;
out:
	fclose(fh);
	return ret;
}

/*
 * contains_inode -- check if list contains specified inode
 */
static int
contains_inode(struct inodes *inodes, unsigned long inode)
{
	struct inode_item *inode_item;
	LIST_FOREACH(inode_item, &inodes->head, next) {
		if (inode_item->inode == inode)
			return 1;
	}

	return 0;
}

/*
 * has_port_inode -- check if /proc/net/tcp has an entry with specified
 * port and inode
 */
static int
has_port_inode(unsigned short port, struct inodes *inodes)
{
	/* format of /proc/net/tcp entries */
	const char *tcp_fmt =
		"%*d: "
		"%*64[0-9A-Fa-f]:%X "
		"%*64[0-9A-Fa-f]:%*X "
		"%*X %*lX:%*lX %*X:%*lX "
		"%*lX %*d %*d %lu %*s\n";

	char buff[BUFF_SIZE];

	FILE *fh = fopen("/proc/net/tcp", "r");
	if (!fh)
		return -1;

	int ret;
	/* read heading */
	char *s = fgets(buff, 4096, fh);
	if (!s) {
		ret = -1;
		goto out;
	}

	while (1) {
		char *s = fgets(buff, 4096, fh);
		if (!s)
			break;

		/* read port number and inode number */
		unsigned p;
		unsigned long inode;
		if (sscanf(s, tcp_fmt, &p, &inode) != 2) {
			ret = -1;
			goto out;
		}

		/*
		 * if port matches and inode is on a list
		 * the process has this port opened
		 */
		if (p == port && contains_inode(inodes, inode)) {
			ret = 1;
			goto out;
		}
	}

	ret = 0;

out:
	fclose(fh);
	return ret;
}

/*
 * get_inodes -- get list of inodes
 */
static int
get_inodes(pid_t pid, struct inodes *inodes)
{
	char path[PATH_MAX];
	char link[PATH_MAX];

	/* set a path to opened files of specified process */
	if (snprintf(path, PATH_MAX, "/proc/%d/fd", pid) < 0)
		return -1;

	int ret;

	/* open dir with all opened files */
	DIR *d = opendir(path);
	if (!d) {
		ret = -1;
		goto out_dir;
	}

	/* read all directory entries */
	struct dirent *dent;
	while ((dent = readdir(d)) != NULL) {
		/* create a full path to file */
		if (snprintf(path, PATH_MAX,
			"/proc/%d/fd/%s", pid, dent->d_name) < 0) {
			ret = -1;
			goto out_dir;
		}

		/* read symbolic link */
		ssize_t sret = readlink(path, link, PATH_MAX - 1);
		if (sret <= 0)
			continue;
		link[sret] = '\0';

		/* check if this is a socket, read inode number if so */
		unsigned long inode;
		if (sscanf(link, "socket:[%lu]", &inode) != 1)
			continue;

		/* add inode to a list */
		struct inode_item *inode_item = malloc(sizeof (*inode_item));
		if (!inode_item) {
			perror("malloc inode item");
			exit(1);
		}

		inode_item->inode = inode;
		LIST_INSERT_HEAD(&inodes->head, inode_item, next);

	}

	ret = 0;
out_dir:
	closedir(d);
	return ret;
}

/*
 * clear_inodes -- clear list of inodes
 */
static void
clear_inodes(struct inodes *inodes)
{
	while (!LIST_EMPTY(&inodes->head)) {
		struct inode_item *inode_item = LIST_FIRST(&inodes->head);
		LIST_REMOVE(inode_item, next);
		free(inode_item);
	}
}

/*
 * has_port -- check if process has the specified tcp port opened
 */
static int
has_port(pid_t pid, unsigned short port)
{
	struct inodes inodes;
	memset(&inodes, 0, sizeof (inodes));

	int ret = get_inodes(pid, &inodes);
	if (ret < 0)
		return -1;

	if (!LIST_EMPTY(&inodes.head)) {
		ret = has_port_inode(port, &inodes);
		clear_inodes(&inodes);
	}

	return ret;
}

/*
 * do_wait_port -- wait until process opens a specified tcp port
 */
static int
do_wait_port(char *pid_file, unsigned short port)
{
	FILE *fh = fopen(pid_file, "r");
	if (!fh) {
		perror(pid_file);
		return 1;
	}

	int ret;

	pid_t pid;
	char r;
	int n = fscanf(fh, "%d%c%d", &pid, &r, &ret);
	if (n < 0) {
		perror("fscanf");
		ret = 1;
		goto err;
	}

	if (n == 2 || (n == 3 && r != 'r')) {
		fprintf(stderr, "invalid format of PID file\n");
		ret = 1;
		goto err;
	}

	if (n == 3) {
		fprintf(stderr, "process already terminated\n");
		ret = 1;
		goto err;
	}

	int hp;
	do {
		hp = has_port(pid, port);
		if (hp < 0) {
			ret = 1;
			goto err;
		}
	} while (!hp);

	return 0;
err:
	fclose(fh);
	return -1;
}

/*
 * convert_signal_name -- convert a signal name to a signal number
 */
static int
convert_signal_name(const char *signal_name)
{
	for (int sig = SIGHUP; sig <= SIGSYS; sig++)
		if (strcmp(signal_name, signal2str[sig]) == 0)
			return sig;
	return -1;
}

int
main(int argc, char *argv[])
{
	if (argc < 3)
		usage();

	int ret = 0;
	char *pid_file = argv[1];
	char *cmd = argv[2];
	if (strcmp(cmd, "run") == 0) {
		if (argc < 4)
			usage();

		char *command = argv[3];
		char **nargv = alloc_argv((unsigned)argc, argv, 3);
		if (!nargv) {
			perror("get_argv");
			return 1;
		}

		ret = do_run(pid_file, command, nargv);

		free(nargv);
	} else if (strcmp(cmd, "wait") == 0) {
		if (argc != 3 && argc != 4)
			usage();

		int timeout = -1;
		if (argc == 4)
			timeout = atoi(argv[3]);

		ret = do_wait(pid_file, timeout);
	} else if (strcmp(cmd, "kill") == 0) {
		if (argc != 4)
			usage();

		int signo = atoi(argv[3]);
		if (signo == 0) {
			signo = convert_signal_name(argv[3]);
			if (signo == -1) {
				fprintf(stderr, "Invalid signal name or number"
						" (%s)\n", argv[3]);
				return 1;
			}
		}

		ret = do_kill(pid_file, signo);
	} else if (strcmp(cmd, "wait_port") == 0) {
		if (argc != 4)
			usage();

		unsigned short port = (unsigned short)atoi(argv[3]);

		ret = do_wait_port(pid_file, port);
	} else {
		usage();
	}

	return ret;
}
