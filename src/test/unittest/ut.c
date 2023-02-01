// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * ut.c -- unit test support routines
 *
 * some of these functions look at errno, but none of them
 * change errno -- it is preserved across these calls.
 *
 * ut_done() and ut_fatal() never return.
 */

#include "unittest.h"

#ifdef __FreeBSD__
#include <uuid/uuid.h>
int
ut_get_uuid_str(char *uu)
{
	uuid_t uuid;

	uuid_generate(uuid);
	uuid_unparse(uuid, uu);
	return 0;
}
#else
int
ut_get_uuid_str(char *uu)
{
	int fd = OPEN(UT_POOL_HDR_UUID_GEN_FILE, O_RDONLY);
	size_t num = READ(fd, uu, UT_POOL_HDR_UUID_STR_LEN);
	UT_ASSERTeq(num, UT_POOL_HDR_UUID_STR_LEN);

	uu[UT_POOL_HDR_UUID_STR_LEN - 1] = '\0';
	CLOSE(fd);
	return 0;
}
#endif

/* RHEL5 seems to be missing decls, even though libc supports them */
extern DIR *fdopendir(int fd);
extern ssize_t readlinkat(int, const char *restrict, char *__restrict, size_t);
void
ut_strerror(int errnum, char *buff, size_t bufflen)
{
	strerror_r(errnum, buff, bufflen);
}
void ut_suppress_errmsg(void) {}
void ut_unsuppress_errmsg(void) {}
void ut_suppress_crt_assert(void) {}
void ut_unsuppress_crt_assert(void) {}

#define MAXLOGFILENAME 100	/* maximum expected .log file name length */
#define MAXPRINT 8192		/* maximum expected single print length */

/*
 * output gets replicated to these files
 */
static FILE *Outfp;
static FILE *Errfp;
static FILE *Tracefp;

static int LogLevel;		/* set by UNITTEST_LOG_LEVEL env variable */
static int Force_quiet;		/* set by UNITTEST_FORCE_QUIET env variable */
static char *Testname;		/* set by UNITTEST_NAME env variable */

/* set by UNITTEST_CHECK_OPEN_FILES_IGNORE_BADBLOCKS env variable */
static int Ignore_bb;

unsigned long Ut_pagesize;
unsigned long long Ut_mmap_align;
os_mutex_t Sigactions_lock;

static char Buff_out[MAXPRINT];
static char Buff_err[MAXPRINT];
static char Buff_trace[MAXPRINT];
static char Buff_stdout[MAXPRINT];

/*
 * flags that control output
 */
#define OF_NONL		1	/* do not append newline */
#define OF_ERR		2	/* output is error output */
#define OF_TRACE	4	/* output to trace file only */
#define OF_NAME		16	/* include Testname in the output */

/*
 * vout -- common output code, all output happens here
 */
static void
vout(int flags, const char *prepend, const char *fmt, va_list ap)
{
	char buf[MAXPRINT];
	unsigned cc = 0;
	int sn;
	const char *sep = "";
	char errstr[UT_MAX_ERR_MSG] = "";
	const char *nl = "\n";

	if (Force_quiet)
		return;

	if (flags & OF_NONL)
		nl = "";

	if (flags & OF_NAME && Testname) {
		sn = util_snprintf(&buf[cc], MAXPRINT - cc, "%s: ", Testname);
		if (sn < 0)
			abort();
		cc += (unsigned)sn;
	}

	if (prepend) {
		const char *colon = "";

		if (fmt)
			colon = ": ";

		sn = util_snprintf(&buf[cc], MAXPRINT - cc, "%s%s", prepend,
				colon);
		if (sn < 0)
			abort();
		cc += (unsigned)sn;
	}

	if (fmt) {
		if (*fmt == '!') {
			fmt++;
			sep = ": ";
			ut_strerror(errno, errstr, UT_MAX_ERR_MSG);
		}
		sn = vsnprintf(&buf[cc], MAXPRINT - cc, fmt, ap);
		if (sn < 0)
			abort();
		cc += (unsigned)sn;
	}

	int ret = util_snprintf(&buf[cc], MAXPRINT - cc,
		"%s%s%s", sep, errstr, nl);
	if (ret < 0)
		UT_FATAL("snprintf: %d", errno);

	/* buf has the fully-baked output, send it everywhere it goes... */
	fputs(buf, Tracefp);
	if (flags & OF_ERR) {
		fputs(buf, Errfp);
		if (LogLevel >= 2)
			fputs(buf, stderr);
	} else if ((flags & OF_TRACE) == 0) {
		fputs(buf, Outfp);
		if (LogLevel >= 2)
			fputs(buf, stdout);
	}
}

/*
 * out -- printf-like output controlled by flags
 */
static void
out(int flags, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	vout(flags, NULL, fmt, ap);

	va_end(ap);
}

/*
 * prefix -- emit the trace line prefix
 */
static void
prefix(const char *file, int line, const char *func, int flags)
{
	out(OF_NONL|OF_TRACE|flags, "{%s:%d %s} ", file, line, func);
}

/*
 * lookup table for open files
 */
static struct fd_lut {
	struct fd_lut *left;
	struct fd_lut *right;
	int fdnum;
	char *fdfile;
} *Fd_lut;

static int Fd_errcount;

/*
 * open_file_add -- add an open file to the lut
 */
static struct fd_lut *
open_file_add(struct fd_lut *root, int fdnum, const char *fdfile)
{
	if (root == NULL) {
		root = ZALLOC(sizeof(*root));
		root->fdnum = fdnum;
		root->fdfile = STRDUP(fdfile);
	} else if (root->fdnum == fdnum)
		UT_FATAL("duplicate fdnum: %d", fdnum);
	else if (root->fdnum < fdnum)
		root->left = open_file_add(root->left, fdnum, fdfile);
	else
		root->right = open_file_add(root->right, fdnum, fdfile);
	return root;
}

/*
 * open_file_remove -- find exact match & remove it from lut
 *
 * prints error if exact match not found, increments Fd_errcount
 */
static void
open_file_remove(struct fd_lut *root, int fdnum, const char *fdfile)
{
	if (root == NULL) {
		if (!Ignore_bb || strstr(fdfile, "badblocks") == NULL) {
			UT_ERR("unexpected open file: fd %d => \"%s\"",
				fdnum, fdfile);
			Fd_errcount++;
		}
	} else if (root->fdnum == fdnum) {
		if (root->fdfile == NULL) {
			UT_ERR("open file dup: fd %d => \"%s\"", fdnum, fdfile);
			Fd_errcount++;
		} else if (strcmp(root->fdfile, fdfile) == 0) {
			/* found exact match */
			FREE(root->fdfile);
			root->fdfile = NULL;
		} else {
			UT_ERR("open file changed: fd %d was \"%s\" now \"%s\"",
			    fdnum, root->fdfile, fdfile);
#ifdef __FreeBSD__
			/*
			 * XXX Pathname list not definitive on FreeBSD,
			 *     so treat as warning
			 */
			FREE(root->fdfile);
			root->fdfile = NULL;
#else
			Fd_errcount++;
#endif
		}
	} else if (root->fdnum < fdnum)
		open_file_remove(root->left, fdnum, fdfile);
	else
		open_file_remove(root->right, fdnum, fdfile);
}

/*
 * open_file_walk -- walk lut for any left-overs
 *
 * prints error if any found, increments Fd_errcount
 */
static void
open_file_walk(struct fd_lut *root)
{
	if (root) {
		open_file_walk(root->left);
		if (root->fdfile) {
			UT_ERR("open file missing: fd %d => \"%s\"",
			    root->fdnum, root->fdfile);
			Fd_errcount++;
		}
		open_file_walk(root->right);
	}
}

/*
 * open_file_free -- free the lut
 */
static void
open_file_free(struct fd_lut *root)
{
	if (root) {
		open_file_free(root->left);
		open_file_free(root->right);
		if (root->fdfile)
			FREE(root->fdfile);
		FREE(root);
	}
}

/*
 * close_output_files -- close opened output files
 */
static void
close_output_files(void)
{
	if (Outfp != NULL)
		fclose(Outfp);

	if (Errfp != NULL)
		fclose(Errfp);

	if (Tracefp != NULL)
		fclose(Tracefp);
}

#ifdef __FreeBSD__
/* XXX Note: Pathname retrieval is not really supported in FreeBSD */
#include <libutil.h>
#include <sys/user.h>
/*
 * record_open_files -- make a list of open files (used at START() time)
 */
static void
record_open_files(void)
{
	int numfds, i;
	struct kinfo_file *fip, *f;

	if ((fip = kinfo_getfile(getpid(), &numfds)) == NULL) {
		UT_FATAL("!kinfo_getfile");
	}
	for (i = 0, f = fip; i < numfds; i++, f++) {
		if (f->kf_fd >= 0) {
			Fd_lut = open_file_add(Fd_lut, f->kf_fd, f->kf_path);
		}
	}
	free(fip);
}

/*
 * check_open_files -- verify open files match recorded open files
 */
static void
check_open_files(void)
{
	int numfds, i;
	struct kinfo_file *fip, *f;

	if ((fip = kinfo_getfile(getpid(), &numfds)) == NULL) {
		UT_FATAL("!kinfo_getfile");
	}
	for (i = 0, f = fip; i < numfds; i++, f++) {
		if (f->kf_fd >= 0) {
			open_file_remove(Fd_lut, f->kf_fd, f->kf_path);
		}
	}
	open_file_walk(Fd_lut);
	if (Fd_errcount) {
		if (os_getenv("UNITTEST_DO_NOT_FAIL_OPEN_FILES"))
			UT_OUT(
				"open file list changed between START() and DONE()");
		else
			UT_FATAL(
				"open file list changed between START() and DONE()");
	}
	open_file_free(Fd_lut);
	free(fip);
}

#else /* !__FreeBSD__ */
/*
 * record_open_files -- make a list of open files (used at START() time)
 */
static void
record_open_files(void)
{
	int dirfd;
	DIR *dirp = NULL;
	struct dirent *dp;

	if ((dirfd = os_open("/proc/self/fd", O_RDONLY)) < 0 ||
	    (dirp = fdopendir(dirfd)) == NULL)
		UT_FATAL("!/proc/self/fd");
	while ((dp = readdir(dirp)) != NULL) {
		int fdnum;
		char fdfile[PATH_MAX];
		ssize_t cc;

		if (*dp->d_name == '.')
			continue;
		if ((cc = readlinkat(dirfd, dp->d_name, fdfile, PATH_MAX)) < 0)
		    UT_FATAL("!readlinkat: /proc/self/fd/%s", dp->d_name);
		fdfile[cc] = '\0';
		fdnum = atoi(dp->d_name);
		if (dirfd == fdnum)
			continue;
		Fd_lut = open_file_add(Fd_lut, fdnum, fdfile);
	}
	closedir(dirp);
}

/*
 * check_open_files -- verify open files match recorded open files
 */
static void
check_open_files(void)
{
	int dirfd;
	DIR *dirp = NULL;
	struct dirent *dp;

	if ((dirfd = os_open("/proc/self/fd", O_RDONLY)) < 0 ||
	    (dirp = fdopendir(dirfd)) == NULL)
		UT_FATAL("!/proc/self/fd");
	while ((dp = readdir(dirp)) != NULL) {
		int fdnum;
		char fdfile[PATH_MAX];
		ssize_t cc;

		if (*dp->d_name == '.')
			continue;
		if ((cc = readlinkat(dirfd, dp->d_name, fdfile, PATH_MAX)) < 0)
		    UT_FATAL("!readlinkat: /proc/self/fd/%s", dp->d_name);
		fdfile[cc] = '\0';
		fdnum = atoi(dp->d_name);
		if (dirfd == fdnum)
			continue;
		open_file_remove(Fd_lut, fdnum, fdfile);
	}
	closedir(dirp);
	open_file_walk(Fd_lut);
	if (Fd_errcount) {
		if (os_getenv("UNITTEST_DO_NOT_FAIL_OPEN_FILES"))
			UT_OUT(
				"open file list changed between START() and DONE()");
		else
			UT_FATAL(
				"open file list changed between START() and DONE()");
	}
	open_file_free(Fd_lut);
}
#endif /* __FreeBSD__ */

/*
 * ut_start_common -- (internal) initialize unit test framework,
 *		indicate test started
 */
static void
ut_start_common(const char *file, int line, const char *func,
    const char *fmt, va_list ap)
{

	int saveerrno = errno;
	char logname[MAXLOGFILENAME];
	char *logsuffix;

	long long sc = sysconf(_SC_PAGESIZE);
	if (sc < 0)
		abort();
	Ut_pagesize = (unsigned long)sc;

	Ut_mmap_align = Ut_pagesize;
	char *ignore_bb =
		os_getenv("UNITTEST_CHECK_OPEN_FILES_IGNORE_BADBLOCKS");

	if (ignore_bb && *ignore_bb)
		Ignore_bb = 1;
	if (os_getenv("UNITTEST_NO_SIGHANDLERS") == NULL)
		ut_register_sighandlers();

	if (os_getenv("UNITTEST_LOG_LEVEL") != NULL)
		LogLevel = atoi(os_getenv("UNITTEST_LOG_LEVEL"));
	else
		LogLevel = 2;

	if (os_getenv("UNITTEST_FORCE_QUIET") != NULL)
		Force_quiet++;

	Testname = os_getenv("UNITTEST_NAME");

	if ((logsuffix = os_getenv("UNITTEST_NUM")) == NULL)
		logsuffix = "";

	const char *fmode = "w";
	if (os_getenv("UNITTEST_LOG_APPEND") != NULL)
		fmode = "a";

	int ret = util_snprintf(logname, MAXLOGFILENAME, "out%s.log",
			logsuffix);
	if (ret < 0)
		UT_FATAL("snprintf: %d", errno);
	if ((Outfp = os_fopen(logname, fmode)) == NULL) {
		perror(logname);
		exit(1);
	}

	ret = util_snprintf(logname, MAXLOGFILENAME, "err%s.log", logsuffix);
	if (ret < 0)
		UT_FATAL("snprintf: %d", errno);
	if ((Errfp = os_fopen(logname, fmode)) == NULL) {
		perror(logname);
		exit(1);
	}

	ret = util_snprintf(logname, MAXLOGFILENAME, "trace%s.log", logsuffix);
	if (ret < 0)
		UT_FATAL("snprintf: %d", errno);
	if ((Tracefp = os_fopen(logname, fmode)) == NULL) {
		perror(logname);
		exit(1);
	}

	setvbuf(Outfp, Buff_out, _IOLBF, MAXPRINT);
	setvbuf(Errfp, Buff_err, _IOLBF, MAXPRINT);
	setvbuf(Tracefp, Buff_trace, _IOLBF, MAXPRINT);
	setvbuf(stdout, Buff_stdout, _IOLBF, MAXPRINT);

	prefix(file, line, func, 0);
	vout(OF_NAME, "START", fmt, ap);

#ifdef __FreeBSD__
	/* XXX Record the fd that will be leaked by uuid_generate */
	uuid_t u;
	uuid_generate(u);
#endif
	record_open_files();

	errno = saveerrno;
}

/*
 * ut_start -- initialize unit test framework, indicate test started
 */
void
ut_start(const char *file, int line, const char *func,
	int argc, char * const argv[], const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ut_start_common(file, line, func, fmt, ap);
	out(OF_NONL, 0, "     args:");
	for (int i = 0; i < argc; i++)
		out(OF_NONL, " %s", argv[i]);
	out(0, NULL);

	va_end(ap);
}

/*
 * ut_end -- indicate test is done, exit program with specified value
 */
void
ut_end(const char *file, int line, const char *func, int ret)
{
	if (!os_getenv("UNITTEST_DO_NOT_CHECK_OPEN_FILES"))
		check_open_files();
	prefix(file, line, func, 0);
	out(OF_NAME, "END %d", ret);

	close_output_files();
	exit(ret);
}

/*
 * ut_done -- indicate test is done, exit program
 */
void
ut_done(const char *file, int line, const char *func,
    const char *fmt, ...)
{
	if (!os_getenv("UNITTEST_DO_NOT_CHECK_OPEN_FILES"))
		check_open_files();

	va_list ap;

	va_start(ap, fmt);

	prefix(file, line, func, 0);
	vout(OF_NAME, "DONE", fmt, ap);

	va_end(ap);

	close_output_files();
	exit(0);
}

/*
 * ut_fatal -- indicate fatal error, exit program
 */
void
ut_fatal(const char *file, int line, const char *func,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	prefix(file, line, func, OF_ERR);
	vout(OF_ERR|OF_NAME, "Error", fmt, ap);

	va_end(ap);

	abort();
}

/*
 * ut_out -- output to stdout
 */
void
ut_out(const char *file, int line, const char *func,
    const char *fmt, ...)
{
	va_list ap;
	int saveerrno = errno;

	va_start(ap, fmt);

	prefix(file, line, func, 0);
	vout(0, NULL, fmt, ap);

	va_end(ap);

	errno = saveerrno;
}

/*
 * ut_err -- output to stderr
 */
void
ut_err(const char *file, int line, const char *func,
    const char *fmt, ...)
{
	va_list ap;
	int saveerrno = errno;

	va_start(ap, fmt);

	prefix(file, line, func, OF_ERR);
	vout(OF_ERR|OF_NAME, NULL, fmt, ap);

	va_end(ap);

	errno = saveerrno;
}

/*
 * ut_checksum -- compute checksum using Fletcher16 algorithm
 */
uint16_t
ut_checksum(uint8_t *addr, size_t len)
{
	uint16_t sum1 = 0;
	uint16_t sum2 = 0;

	for (size_t i = 0; i < len; ++i) {
		sum1 = (uint16_t)(sum1 + addr[i]) % 255;
		sum2 = (uint16_t)(sum2 + sum1) % 255;
	}

	return (uint16_t)((sum2 << 8) | sum1);
}

/*
 * ut_strtoi -- a strtoi call that cannot return error
 */
int
ut_strtoi(const char *file, int line, const char *func,
	const char *nptr, char **endptr, int base)
{
	long ret = ut_strtol(file, line, func, nptr, endptr, base);

	if (ret > INT_MAX || ret < INT_MIN)
		ut_fatal(file, line, func,
			"!strtoi: nptr=%s, endptr=%s, base=%d",
			nptr, endptr ? *endptr : "NULL", base);

	return (int)ret;
}

/*
 * ut_strtou -- a strtou call that cannot return error
 */
unsigned
ut_strtou(const char *file, int line, const char *func,
	const char *nptr, char **endptr, int base)
{
	unsigned long ret = ut_strtoul(file, line, func, nptr, endptr, base);

	if (ret > UINT_MAX)
		ut_fatal(file, line, func,
			"!strtou: nptr=%s, endptr=%s, base=%d",
			nptr, endptr ? *endptr : "NULL", base);

	return (unsigned)ret;
}

/*
 * ut_strtol -- a strtol call that cannot return error
 */
long
ut_strtol(const char *file, int line, const char *func,
	const char *nptr, char **endptr, int base)
{
	long long ret = ut_strtoll(file, line, func, nptr, endptr, base);

	if (ret > LONG_MAX || ret < LONG_MIN)
		ut_fatal(file, line, func,
			"!strtol: nptr=%s, endptr=%s, base=%d",
			nptr, endptr ? *endptr : "NULL", base);

	return (long)ret;
}

/*
 * ut_strtoul -- a strtou call that cannot return error
 */
unsigned long
ut_strtoul(const char *file, int line, const char *func,
	const char *nptr, char **endptr, int base)
{
	unsigned long long ret =
		ut_strtoull(file, line, func, nptr, endptr, base);

	if (ret > ULONG_MAX)
		ut_fatal(file, line, func,
			"!strtoul: nptr=%s, endptr=%s, base=%d",
			nptr, endptr ? *endptr : "NULL", base);

	return (unsigned long)ret;
}

/*
 * ut_strtoull -- a strtoul call that cannot return error
 */
unsigned long long
ut_strtoull(const char *file, int line, const char *func,
	const char *nptr, char **endptr, int base)
{
	unsigned long long retval;
	errno = 0;
	if (*nptr == '\0') {
		errno = EINVAL;
		goto fatal;
	}

	if (endptr != NULL) {
		retval = strtoull(nptr, endptr, base);
	} else {
		char *end;
		retval = strtoull(nptr, &end, base);
		if (*end != '\0')
			goto fatal;
	}
	if (errno != 0)
		goto fatal;

	return retval;
fatal:
	ut_fatal(file, line, func,
		"!strtoull: nptr=%s, endptr=%s, base=%d",
		nptr, endptr ? *endptr : "NULL", base);
}

/*
 * ut_strtoll -- a strtol call that cannot return error
 */
long long
ut_strtoll(const char *file, int line, const char *func,
	const char *nptr, char **endptr, int base)
{
	long long retval;
	errno = 0;
	if (*nptr == '\0') {
		errno = EINVAL;
		goto fatal;
	}

	if (endptr != NULL) {
		retval = strtoll(nptr, endptr, base);
	} else {
		char *end;
		retval = strtoll(nptr, &end, base);
		if (*end != '\0')
			goto fatal;
	}
	if (errno != 0)
		goto fatal;

	return retval;
fatal:
	ut_fatal(file, line, func,
		"!strtoll: nptr=%s, endptr=%s, base=%d",
		nptr, endptr ? *endptr : "NULL", base);
}

int
ut_snprintf(const char *file, int line, const char *func,
		char *str, size_t size, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int ret = vsnprintf(str, size, format, ap);
	va_end(ap);

	if (ret < 0) {
		if (!errno)
			errno = EIO;
		ut_fatal(file, line, func, "!snprintf");
	} else if ((size_t)ret >= size) {
		errno = ENOBUFS;
		ut_fatal(file, line, func, "!snprintf");
	}

	return ret;
}
