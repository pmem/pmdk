/*
 * Copyright 2016-2019, Intel Corporation
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
 *
 * XXX - wait_port is not supported on FreeBSD because there are currently
 *       no test cases that require it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <limits.h>
#include <queue.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdarg.h>

#include "os.h"

#ifdef __FreeBSD__
#include "signals_freebsd.h"
#else
#include "signals_linux.h"
#endif

#define APP_NAME "ctrld"
#define BUFF_SIZE 4096

#define S_MINUTE	(60)		/* seconds in one minute */
#define S_HOUR		(60 * 60)	/* seconds in one hour */
#define S_DAY		(60 * 60 * 24)	/* seconds in one day */

static FILE *log_fh;

static void
log_err(const char *file, int lineno, const char *fmt, ...)
{
	FILE *fh = log_fh ? log_fh : stderr;
	va_list ap;
	fprintf(fh, "[%s:%d] ", file, lineno);

	char *prefix = "";
	char *errstr = "";
	if (*fmt == '!') {
		fmt++;
		prefix = ": ";
		errstr = strerror(errno);
	}

	va_start(ap, fmt);
	vfprintf(fh, fmt, ap);
	va_end(ap);

	fprintf(fh, "%s%s\n", prefix, errstr);
	fflush(fh);
}

#define CTRLD_LOG(...) log_err(__FILE__, __LINE__, __VA_ARGS__)

struct inode_item {
	PMDK_LIST_ENTRY(inode_item) next;
	unsigned long inode;
};

struct inodes {
	PMDK_LIST_HEAD(inode_items, inode_item) head;
};

/*
 * usage -- print usage and exit with failure code
 */
static void
usage(void)
{
	CTRLD_LOG("usage: %s <pid file> <cmd> [<arg>]", APP_NAME);
	CTRLD_LOG("commands:");
	CTRLD_LOG("  exe <command> [<args...>] -- "
			"run specified command");
	CTRLD_LOG("  run  <timeout> <command> [<args...>] -- "
			"run specified command with given timeout");
	CTRLD_LOG("  wait [<timeout>]                     -- "
			"wait for command");
#ifndef __FreeBSD_
	CTRLD_LOG("  wait_port <port>                     -- "
			"wait until a port is opened");
#endif
	CTRLD_LOG("  kill <signal>                        -- "
			"send a signal to command");
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
	char **nargv = malloc((nargc + 1) * sizeof(char *));
	if (!nargv)
		return NULL;

	for (unsigned i = 0; i < nargc; i++)
		nargv[i] = argv[i + off];

	nargv[nargc] = NULL;

	return nargv;
}

/*
 * do_run_or_exe -- execute the 'run' or the 'exe' command
 *
 * if timeout is equal to 0 cmd will be just executed (the 'exe' command)
 * otherwise it will be run and wait with timeout (the 'run' command)
 */
static int
do_run_or_exe(const char *pid_file, char *cmd, char *argv[], unsigned timeout)
{
	int rv = -1;

	FILE *fh = os_fopen(pid_file, "w+");
	if (!fh) {
		CTRLD_LOG("!%s", pid_file);
		return -1;
	}

	int fd = fileno(fh);
	if (fd == -1) {
		CTRLD_LOG("!fileno");
		goto err;
	}

	if (os_flock(fd, LOCK_EX | LOCK_NB)) {
		CTRLD_LOG("!flock");
		goto err;
	}

	if (timeout != 0) {
		if (daemon(1, 0)) {
			CTRLD_LOG("!daemon");
			goto err;
		}
	}

	int child = fork();
	switch (child) {
	case -1:
		CTRLD_LOG("!fork");
		fprintf(fh, "-1r%d", errno);
		goto err;
	case 0:
		execvp(cmd, argv);
		CTRLD_LOG("!execvp(%s)", cmd);
		goto err;
	default:
		break;
	}

	if (fprintf(fh, "%d", child) < 0) {
		CTRLD_LOG("!fprintf");
		goto err;
	}

	if (fflush(fh)) {
		CTRLD_LOG("!fflush");
		goto err;
	}

	int child_timeout = -1;
	if (timeout != 0) {
		child_timeout = fork();
		switch (child_timeout) {
		case -1:
			CTRLD_LOG("!fork");
			fprintf(fh, "-1r%d", errno);
			goto err;
		case 0:
			fclose(fh);
			sleep(timeout);
			return 0;
		default:
			break;
		}
	}

	int ret = 0;
	int pid = wait(&ret);
	if (pid == child) {
		if (timeout != 0) {
			/* kill the timeout child */
			kill(child_timeout, SIGTERM);
		}

		if (WIFSIGNALED(ret)) {
			ret = 128 + WTERMSIG(ret);
		} else {
			ret = WEXITSTATUS(ret);
		}

		if (fseek(fh, 0, SEEK_SET)) {
			CTRLD_LOG("!fseek");
			goto err;
		}

		if (os_ftruncate(fileno(fh), 0)) {
			CTRLD_LOG("!ftruncate");
			goto err;
		}

		fprintf(fh, "%dr%d", child, ret);

	} else if (timeout != 0 && pid == child_timeout) {
		CTRLD_LOG("run: timeout");
		if (kill(child, SIGTERM) && errno != ESRCH) {
			CTRLD_LOG("!kill");
			goto err;
		}
		CTRLD_LOG("run: process '%s' killed (PID %i)", cmd, child);
	} else {
		CTRLD_LOG("!wait");
		goto err;
	}

	rv = 0;
err:
	fclose(fh);
	return rv;
}

/*
 * do_wait -- execute the 'wait' command
 */
static int
do_wait(char *pid_file, int timeout)
{
	int fd = os_open(pid_file, O_RDONLY);
	if (fd < 0) {
		perror(pid_file);
		return 1;
	}

	int ret;

	int t = 0;
	while ((timeout == -1 || t < timeout) &&
		os_flock(fd, LOCK_EX | LOCK_NB)) {
		sleep(1);
		t++;
	}

	FILE *fh = os_fdopen(fd, "r");
	if (!fh) {
		CTRLD_LOG("!fdopen");
		ret = 1;
		goto err;
	}

	pid_t pid;
	char r;
	int n = fscanf(fh, "%d%c%d", &pid, &r, &ret);
	if (n < 0) {
		CTRLD_LOG("!fscanf");
		ret = 1;
		goto err;
	}

	if (n == 2 || (n == 3 && r != 'r')) {
		CTRLD_LOG("invalid format of PID file");
		ret = 1;
		goto err;
	}

	if (n == 1) {
		if (timeout >= 0) {
			ret = -1;
			goto err;
		} else {
			CTRLD_LOG("missing return value");
			ret = 1;
			goto err;
		}
	}

err:
	os_close(fd);
	fclose(fh);
	return ret;
}

/*
 * do_kill -- execute the 'kill' command
 */
static int
do_kill(char *pid_file, int signo)
{
	FILE *fh = os_fopen(pid_file, "r");
	if (!fh) {
		CTRLD_LOG("!%s", pid_file);
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
		CTRLD_LOG("!kill");
		ret = 1;
		goto out;
	}

	ret = 0;
out:
	fclose(fh);
	return ret;
}

#ifndef __FreeBSD__	/* XXX wait_port support */
/*
 * contains_inode -- check if list contains specified inode
 */
static int
contains_inode(struct inodes *inodes, unsigned long inode)
{
	struct inode_item *inode_item;
	PMDK_LIST_FOREACH(inode_item, &inodes->head, next) {
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
	const char * const tcp_fmt =
		"%*d: "
		"%*64[0-9A-Fa-f]:%X "
		"%*64[0-9A-Fa-f]:%*X "
		"%*X %*X:%*X %*X:%*X "
		"%*X %*d %*d %lu %*s\n";

	char buff[BUFF_SIZE];

	FILE *fh = os_fopen("/proc/net/tcp", "r");
	if (!fh) {
		CTRLD_LOG("!%s", "/proc/net/tcp");
		return -1;
	}

	int ret;
	/* read heading */
	char *s = fgets(buff, 4096, fh);
	if (!s) {
		ret = -1;
		goto out;
	}

	while (1) {
		s = fgets(buff, 4096, fh);
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
	int ret;

	/* set a path to opened files of specified process */
	if ((ret = snprintf(path, PATH_MAX, "/proc/%d/fd", pid)) < 0) {
		CTRLD_LOG("snprintf: %d", ret);
		return -1;
	}

	/* open dir with all opened files */
	DIR *d = opendir(path);
	if (!d) {
		CTRLD_LOG("!%s", path);
		ret = -1;
		goto out_dir;
	}

	/* read all directory entries */
	struct dirent *dent;
	while ((dent = readdir(d)) != NULL) {
		/* create a full path to file */
		if ((ret = snprintf(path, PATH_MAX,
			"/proc/%d/fd/%s", pid, dent->d_name)) < 0) {
			CTRLD_LOG("snprintf: %d", ret);
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
		struct inode_item *inode_item = malloc(sizeof(*inode_item));
		if (!inode_item) {
			CTRLD_LOG("!malloc inode item");
			exit(1);
		}

		inode_item->inode = inode;
		PMDK_LIST_INSERT_HEAD(&inodes->head, inode_item, next);

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
	while (!PMDK_LIST_EMPTY(&inodes->head)) {
		struct inode_item *inode_item = PMDK_LIST_FIRST(&inodes->head);
		PMDK_LIST_REMOVE(inode_item, next);
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
	memset(&inodes, 0, sizeof(inodes));

	int ret = get_inodes(pid, &inodes);
	if (ret < 0)
		return -1;

	if (!PMDK_LIST_EMPTY(&inodes.head)) {
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
	FILE *fh = os_fopen(pid_file, "r");
	if (!fh) {
		CTRLD_LOG("!%s", pid_file);
		return 1;
	}

	int ret;

	pid_t pid;
	char r;
	int n = fscanf(fh, "%d%c%d", &pid, &r, &ret);
	if (n < 0) {
		CTRLD_LOG("!fscanf");
		ret = 1;
		goto err;
	}

	if (n == 2 || (n == 3 && r != 'r')) {
		CTRLD_LOG("invalid format of PID file");
		ret = 1;
		goto err;
	}

	if (n == 3) {
		CTRLD_LOG("process already terminated");
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
#endif	/* __FreeBSD__ wait_port support */

/*
 * convert_signal_name -- convert a signal name to a signal number
 */
static int
convert_signal_name(const char *signal_name)
{
	for (int sig = SIGHUP; sig <= SIGNALMAX; sig++)
		if (strcmp(signal_name, signal2str[sig]) == 0)
			return sig;
	return -1;
}

/*
 * log_run -- print run command with arguments
 */
static void
log_run(const char *pid_file, char *cmd, char *argv[])
{
	char buff[BUFF_SIZE];
	buff[0] = '\0';
	size_t cnt = 0;
	size_t i = 0;
	char *arg = argv[0];
	while (arg) {
		int ret = snprintf(&buff[cnt], BUFF_SIZE - cnt,
				" %s", arg);
		if (ret < 0) {
			CTRLD_LOG("snprintf: %d", ret);
			exit(EXIT_FAILURE);
		}

		cnt += (size_t)ret;

		i++;
		arg = argv[i];
	}

	CTRLD_LOG("run %s%s", pid_file, buff);
}

/*
 * convert_timeout -- convert a floating point number with an optional suffix
 *                    to unsigned integer: 's' for seconds (the default),
 *                    'm' for minutes, 'h' for hours or 'd' for days.
 */
static unsigned
convert_timeout(char *str)
{
	char *endptr;
	float ftimeout = strtof(str, &endptr);
	switch (*endptr) {
	case 'm':
		ftimeout *= S_MINUTE;
		break;
	case 'h':
		ftimeout *= S_HOUR;
		break;
	case 'd':
		ftimeout *= S_DAY;
		break;
	default:
		break;
	}
	return (unsigned)ftimeout;
}

int
main(int argc, char *argv[])
{
	if (argc < 3)
		usage();

	int ret = 0;
	char *pid_file = argv[1];
	char *cmd = argv[2];

	char buff[BUFF_SIZE];
	if (snprintf(buff, BUFF_SIZE, "%s.%s.%s.log",
			pid_file, cmd, APP_NAME) < 0) {
		perror("snprintf");
		return -1;
	}

	log_fh = os_fopen(buff, "a");
	if (!log_fh) {
		perror(buff);
		return -1;
	}

	if (strcmp(cmd, "exe") == 0) {
		if (argc < 4)
			usage();

		char *command = argv[3];
		char **nargv = alloc_argv((unsigned)argc, argv, 3);
		if (!nargv) {
			CTRLD_LOG("!get_argv");
			return 1;
		}

		log_run(pid_file, command, nargv);
		ret = do_run_or_exe(pid_file, command, nargv, 0 /* timeout */);

		free(nargv);
	} else if (strcmp(cmd, "run") == 0) {
		if (argc < 5)
			usage();

		unsigned timeout = convert_timeout(argv[3]);
		char *command = argv[4];
		char **nargv = alloc_argv((unsigned)argc, argv, 4);
		if (!nargv) {
			CTRLD_LOG("!get_argv");
			return 1;
		}

		log_run(pid_file, command, nargv);
		ret = do_run_or_exe(pid_file, command, nargv, timeout);

		free(nargv);
	} else if (strcmp(cmd, "wait") == 0) {
		if (argc != 3 && argc != 4)
			usage();

		int timeout = -1;
		if (argc == 4)
			timeout = atoi(argv[3]);

		CTRLD_LOG("wait %s %d", pid_file, timeout);
		ret = do_wait(pid_file, timeout);
	} else if (strcmp(cmd, "kill") == 0) {
		if (argc != 4)
			usage();

		int signo = atoi(argv[3]);
		if (signo == 0) {
			signo = convert_signal_name(argv[3]);
			if (signo == -1) {
				CTRLD_LOG("Invalid signal name or number"
						" (%s)", argv[3]);
				return 1;
			}
		}

		CTRLD_LOG("kill %s %s", pid_file, argv[3]);
		ret = do_kill(pid_file, signo);
#ifndef __FreeBSD__
	} else if (strcmp(cmd, "wait_port") == 0) {
		if (argc != 4)
			usage();

		unsigned short port = (unsigned short)atoi(argv[3]);

		CTRLD_LOG("wait_port %s %u", pid_file, port);
		ret = do_wait_port(pid_file, port);
#endif
	} else {
		usage();
	}

	return ret;
}
