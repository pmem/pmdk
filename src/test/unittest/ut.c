/*
 * Copyright 2014-2017, Intel Corporation
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
 * ut.c -- unit test support routines
 *
 * some of these functions look at errno, but none of them
 * change errno -- it is preserved across these calls.
 *
 * ut_done() and ut_fatal() never return.
 */

#include "unittest.h"

#ifndef _WIN32
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
#else
#pragma comment(lib, "rpcrt4.lib")

void
ut_suppress_errmsg(void)
{
	ErrMode = GetErrorMode();
	SetErrorMode(ErrMode | SEM_NOGPFAULTERRORBOX |
		SEM_FAILCRITICALERRORS);
	AbortBehave = _set_abort_behavior(0, _WRITE_ABORT_MSG |
		_CALL_REPORTFAULT);
	Suppressed = TRUE;
}

void
ut_unsuppress_errmsg(void)
{
	if (Suppressed) {
		SetErrorMode(ErrMode);
		_set_abort_behavior(AbortBehave, _WRITE_ABORT_MSG |
			_CALL_REPORTFAULT);
		Suppressed = FALSE;
	}
}

int
ut_get_uuid_str(char *uuid_str)
{
	UUID uuid;
	char *buff;

	if (UuidCreate(&uuid) == 0)
		if (UuidToStringA(&uuid, &buff) == RPC_S_OK) {
			strcpy_s(uuid_str, UT_POOL_HDR_UUID_STR_LEN, buff);
			return 0;
		}
	return -1;
}
/* XXX - fix this temp hack dup'ing util_strerror when we get mock for win */
#define ENOTSUP_STR "Operation not supported"
#define UNMAPPED_STR "Unmapped error"
void
ut_strerror(int errnum, char *buff, size_t bufflen)
{
	switch (errnum) {
		case ENOTSUP:
			strcpy_s(buff, bufflen, ENOTSUP_STR);
			break;
		default:
			if (strerror_s(buff, bufflen, errnum))
				strcpy_s(buff, bufflen, UNMAPPED_STR);
	}
}

/*
 * ut_spawnv -- creates and executes new synchronous process,
 * ... are additional parameters to new process,
 * the last argument must be a NULL
 */
intptr_t
ut_spawnv(int argc, const char **argv, ...)
{
	int va_count = 0;

	va_list ap;
	va_start(ap, argv);
	while (va_arg(ap, char *)) {
		va_count++;
	}
	va_end(ap);

	/* 1 for terminating NULL */
	char **argv2 = calloc(argc + va_count + 1, sizeof(char *));
	if (argv2 == NULL) {
		UT_ERR("Cannot calloc memory for new array");
		return -1;
	}
	memcpy(argv2, argv, argc * sizeof(char *));

	va_start(ap, argv);
	for (int i = 0; i < va_count; i++) {
		argv2[argc + i] = va_arg(ap, char *);
	}
	va_end(ap);

	intptr_t ret = _spawnv(_P_WAIT, argv2[0], argv2);
	free(argv2);

	return ret;
}
#endif

#define MAXLOGNAME 100		/* maximum expected .log file name length */
#define MAXPRINT 8192		/* maximum expected single print length */

/*
 * output gets replicated to these files
 */
static FILE *Outfp;
static FILE *Errfp;
static FILE *Tracefp;

static int Quiet;		/* set by UNITTEST_QUIET env variable */
static int Force_quiet;		/* set by UNITTEST_FORCE_QUIET env variable */
static char *Testname;		/* set by UNITTEST_NAME env variable */
unsigned long Ut_pagesize;
unsigned long long Ut_mmap_align;

/*
 * flags that control output
 */
#define OF_NONL		1	/* do not append newline */
#define OF_ERR		2	/* output is error output */
#define OF_TRACE	4	/* output to trace file only */
#define OF_LOUD		8	/* output even in Quiet mode */
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
	int quiet = Quiet;
	const char *sep = "";
	char errstr[UT_MAX_ERR_MSG] = "";
	const char *nl = "\n";

	if (Force_quiet)
		return;

	if (flags & OF_LOUD)
		quiet = 0;

	if (flags & OF_NONL)
		nl = "";

	if (flags & OF_NAME && Testname) {
		sn = snprintf(&buf[cc], MAXPRINT - cc, "%s: ", Testname);
		if (sn < 0)
			abort();
		cc += (unsigned)sn;
	}

	if (prepend) {
		const char *colon = "";

		if (fmt)
			colon = ": ";

		sn = snprintf(&buf[cc], MAXPRINT - cc, "%s%s", prepend, colon);
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

	snprintf(&buf[cc], MAXPRINT - cc, "%s%s%s", sep, errstr, nl);

	/* buf has the fully-baked output, send it everywhere it goes... */
	fputs(buf, Tracefp);
	if (flags & OF_ERR) {
		fputs(buf, Errfp);
		if (!quiet)
			fputs(buf, stderr);
	} else if ((flags & OF_TRACE) == 0) {
		fputs(buf, Outfp);
		if (!quiet)
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
		UT_ERR("unexpected open file: fd %d => \"%s\"", fdnum, fdfile);
		Fd_errcount++;
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
			Fd_errcount++;
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

#ifndef _WIN32

/*
 * record_open_files -- make a list of open files (used at START() time)
 */
static void
record_open_files(void)
{
	int dirfd;
	DIR *dirp = NULL;
	struct dirent *dp;

	if ((dirfd = open("/proc/self/fd", O_RDONLY)) < 0 ||
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

	if ((dirfd = open("/proc/self/fd", O_RDONLY)) < 0 ||
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
	if (Fd_errcount)
		UT_FATAL("open file list changed between START() and DONE()");
	open_file_free(Fd_lut);
}

#else /* _WIN32 */

#include <winternl.h>

#define STATUS_INFO_LENGTH_MISMATCH 0xc0000004

#define ObjectBasicInformation 0
#define ObjectNameInformation 1
#define ObjectTypeInformation 2
#define SystemHandleInformation 16

typedef struct _SYSTEM_HANDLE {
	ULONG ProcessId;
	BYTE ObjectTypeNumber;
	BYTE Flags;
	USHORT Handle;
	PVOID Object;
	ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE, *PSYSTEM_HANDLE;

typedef struct _SYSTEM_HANDLE_INFORMATION {
	ULONG HandleCount;
	SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

typedef enum _POOL_TYPE {
	NonPagedPool,
	PagedPool,
	NonPagedPoolMustSucceed,
	DontUseThisType,
	NonPagedPoolCacheAligned,
	PagedPoolCacheAligned,
	NonPagedPoolCacheAlignedMustS
} POOL_TYPE, *PPOOL_TYPE;

typedef struct _OBJECT_TYPE_INFORMATION {
	UNICODE_STRING Name;
	ULONG TotalNumberOfObjects;
	ULONG TotalNumberOfHandles;
	ULONG TotalPagedPoolUsage;
	ULONG TotalNonPagedPoolUsage;
	ULONG TotalNamePoolUsage;
	ULONG TotalHandleTableUsage;
	ULONG HighWaterNumberOfObjects;
	ULONG HighWaterNumberOfHandles;
	ULONG HighWaterPagedPoolUsage;
	ULONG HighWaterNonPagedPoolUsage;
	ULONG HighWaterNamePoolUsage;
	ULONG HighWaterHandleTableUsage;
	ULONG InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccess;
	BOOLEAN SecurityRequired;
	BOOLEAN MaintainHandleCount;
	USHORT MaintainTypeList;
	POOL_TYPE PoolType;
	ULONG PagedPoolUsage;
	ULONG NonPagedPoolUsage;
} OBJECT_TYPE_INFORMATION, *POBJECT_TYPE_INFORMATION;

/*
 * enum_handles -- (internal) record or check a list of open handles
 */
static void
enum_handles(int op)
{
	ULONG hi_size = 0x200000; /* default size */
	ULONG req_size = 0;

	PSYSTEM_HANDLE_INFORMATION hndl_info =
		(PSYSTEM_HANDLE_INFORMATION)MALLOC(hi_size);

	/* if it fails with the default info size, realloc and try again */
	NTSTATUS status;
	while ((status = NtQuerySystemInformation(SystemHandleInformation,
			hndl_info, hi_size, &req_size)
				== STATUS_INFO_LENGTH_MISMATCH)) {
		hi_size = req_size + 4096;
		hndl_info = (PSYSTEM_HANDLE_INFORMATION)REALLOC(hndl_info,
				hi_size);
	}
	UT_ASSERT(status >= 0);

	DWORD pid = GetProcessId(GetCurrentProcess());

	DWORD ti_size = 4096; /* initial size */
	POBJECT_TYPE_INFORMATION type_info =
		(POBJECT_TYPE_INFORMATION)MALLOC(ti_size);

	DWORD ni_size = 4096; /* initial size */
	PVOID name_info = MALLOC(ni_size);

	for (ULONG i = 0; i < hndl_info->HandleCount; i++) {
		SYSTEM_HANDLE handle = hndl_info->Handles[i];
		UNICODE_STRING wname;
		char name[MAX_PATH];

		/* ignore handles not owned by current process */
		if (handle.ProcessId != pid)
			continue;

		/* query the object type */
		status = NtQueryObject((HANDLE)handle.Handle,
			ObjectTypeInformation, type_info, ti_size, NULL);
		UT_ASSERT(status >= 0);

		/* register/verify only handles of selected types */
		switch (type_info->MaintainTypeList) {
			case 0x03: /* Directory */
			case 0x0d: /* Mutant */
			case 0x0f: /* Semaphore */
			case 0x1e: /* File */
			case 0x23: /* Section (memory mapping) */
				;
			default:
				continue;
		}

		/*
		 * Skip handles with access 0x0012019f.  NtQueryObject() may
		 * hang on querying the handles pointing to named pipes.
		 */
		if (handle.GrantedAccess == 0x0012019f)
			continue;

		wname.Length = 0;
		wname.Buffer = NULL;
		if (NtQueryObject((HANDLE)handle.Handle, ObjectNameInformation,
				name_info, ni_size, &req_size) < 0) {
			/* reallocate buffer to required size and try again */
			if (req_size > ni_size)
				ni_size = req_size;
			name_info = REALLOC(name_info, ni_size);
			if (NtQueryObject((HANDLE)handle.Handle,
					ObjectNameInformation,
					name_info, ni_size, NULL) >= 0) {
				wname = *(PUNICODE_STRING)name_info;
			}
		} else {
			wname = *(PUNICODE_STRING)name_info;
		}

		snprintf(name, MAX_PATH, "%.*S: %.*S",
			type_info->Name.Length / 2, type_info->Name.Buffer,
			wname.Length / 2, wname.Buffer);

		if (op == 0)
			Fd_lut = open_file_add(Fd_lut, handle.Handle, name);
		else
			open_file_remove(Fd_lut, handle.Handle, name);
	}

	FREE(type_info);
	FREE(name_info);
	FREE(hndl_info);
}

/*
 * record_open_files -- record a number of open handles (used at START() time)
 *
 * On Windows, it records not only file handles, but some other handle types
 * as well.
 * XXX: We can't register all the handles, as spawning new process in the test
 * may result in opening new handles of some types (i.e. registry keys).
 */
static void
record_open_files()
{
	/*
	 * XXX: Dummy call to CoCreateGuid() to ignore files/handles open
	 * by this function.  They won't be closed until process termination.
	 */
	GUID uuid;
	HRESULT res = CoCreateGuid(&uuid);

	enum_handles(0);
}

/*
 * check_open_files -- verify open handles match recorded open handles
 */
static void
check_open_files()
{
	enum_handles(1);

	open_file_walk(Fd_lut);
	if (Fd_errcount)
		UT_FATAL("open file list changed between START() and DONE()");
	open_file_free(Fd_lut);
}

#endif /* _WIN32 */

/*
 * ut_start -- initialize unit test framework, indicate test started
 */
void
ut_start(const char *file, int line, const char *func,
    int argc, char * const argv[], const char *fmt, ...)
{
	va_list ap;
	int saveerrno = errno;
	char logname[MAXLOGNAME];
	char *logsuffix;

	va_start(ap, fmt);

	long long sc = sysconf(_SC_PAGESIZE);
	if (sc < 0)
		abort();
	Ut_pagesize = (unsigned long)sc;

#ifdef _WIN32
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	Ut_mmap_align = si.dwAllocationGranularity;

	if (getenv("UNITTEST_NO_ABORT_MSG") != NULL) {
		/* disable windows error message boxes */
		ut_suppress_errmsg();
	}
#else
	Ut_mmap_align = Ut_pagesize;
#endif
	if (getenv("UNITTEST_NO_SIGHANDLERS") == NULL)
		ut_register_sighandlers();

	if (getenv("UNITTEST_QUIET") != NULL)
		Quiet++;

	if (getenv("UNITTEST_FORCE_QUIET") != NULL)
		Force_quiet++;

	Testname = getenv("UNITTEST_NAME");

	if ((logsuffix = getenv("UNITTEST_NUM")) == NULL)
		logsuffix = "";

	const char *fmode = "w";
	if (getenv("UNITTEST_LOG_APPEND") != NULL)
		fmode = "a";

	snprintf(logname, MAXLOGNAME, "out%s.log", logsuffix);
	if ((Outfp = fopen(logname, fmode)) == NULL) {
		perror(logname);
		exit(1);
	}

	snprintf(logname, MAXLOGNAME, "err%s.log", logsuffix);
	if ((Errfp = fopen(logname, fmode)) == NULL) {
		perror(logname);
		exit(1);
	}

	snprintf(logname, MAXLOGNAME, "trace%s.log", logsuffix);
	if ((Tracefp = fopen(logname, fmode)) == NULL) {
		perror(logname);
		exit(1);
	}

	setlinebuf(Outfp);
	setlinebuf(Errfp);
	setlinebuf(Tracefp);
	setlinebuf(stdout);

	prefix(file, line, func, 0);
	vout(OF_LOUD|OF_NAME, "START", fmt, ap);

	for (int i = 0; i < argc; i++)
		out(OF_NONL, " %s", argv[i]);
	out(0, NULL);

	va_end(ap);

	record_open_files();

	errno = saveerrno;
}

/*
 * ut_done -- indicate test is done, exit program
 */
void
ut_done(const char *file, int line, const char *func,
    const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (!getenv("UNITTEST_DO_NOT_CHECK_OPEN_FILES"))
		check_open_files();

	prefix(file, line, func, 0);
	vout(OF_NAME, "DONE", fmt, ap);

	va_end(ap);

	if (Outfp != NULL)
		fclose(Outfp);

	if (Errfp != NULL)
		fclose(Errfp);

	if (Tracefp != NULL)
		fclose(Tracefp);

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

	return (uint16_t)(sum2 << 8) | sum1;
}
