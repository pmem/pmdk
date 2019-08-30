/*
 * Copyright 2014-2019, Intel Corporation
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
 *
 * XXX: argc/argv are ignored actually, as we need to use the unmodified
 * UTF16-encoded command line args.
 */
intptr_t
ut_spawnv(int argc, const char **argv, ...)
{
	int va_count = 0;

	int wargc;
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);

	va_list ap;
	va_start(ap, argv);
	while (va_arg(ap, char *)) {
		va_count++;
	}
	va_end(ap);

	/* 1 for terminating NULL */
	wchar_t **wargv2 = calloc(wargc + va_count + 1, sizeof(wchar_t *));
	if (wargv2 == NULL) {
		UT_ERR("Cannot calloc memory for new array");
		return -1;
	}
	memcpy(wargv2, wargv, wargc * sizeof(wchar_t *));

	va_start(ap, argv);
	for (int i = 0; i < va_count; i++) {
		char *a = va_arg(ap, char *);
		wargv2[wargc + i] = ut_toUTF16(a);
	}
	va_end(ap);

	intptr_t ret = _wspawnv(_P_WAIT, wargv2[0], wargv2);

	for (int i = 0; i < va_count; i++) {
		free(wargv2[wargc + i]);
	}

	free(wargv2);

	return ret;
}
#endif

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

	int ret = snprintf(&buf[cc], MAXPRINT - cc,
		"%s%s%s", sep, errstr, nl);
	if (ret < 0 || ret >= MAXPRINT - (int)cc)
		UT_FATAL("!snprintf");

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

#ifndef _WIN32
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
	if (Fd_errcount)
		UT_FATAL("open file list changed between START() and DONE()");
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
	if (Fd_errcount)
		UT_FATAL("open file list changed between START() and DONE()");
	open_file_free(Fd_lut);
}
#endif /* __FreeBSD__ */

#else /* _WIN32 */

#include <winternl.h>

#define STATUS_INFO_LENGTH_MISMATCH 0xc0000004

#define ObjectTypeInformation 2
#define SystemExtendedHandleInformation 64

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
	PVOID Object;
	HANDLE UniqueProcessId;
	HANDLE HandleValue;
	ULONG GrantedAccess;
	USHORT CreatorBackTraceIndex;
	USHORT ObjectTypeIndex;
	ULONG HandleAttributes;
	ULONG Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
	ULONG_PTR NumberOfHandles;
	ULONG_PTR Reserved;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX, *PSYSTEM_HANDLE_INFORMATION_EX;

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

	PSYSTEM_HANDLE_INFORMATION_EX hndl_info =
		(PSYSTEM_HANDLE_INFORMATION_EX)MALLOC(hi_size);

	/* if it fails with the default info size, realloc and try again */
	NTSTATUS status;
	while ((status = NtQuerySystemInformation(
			SystemExtendedHandleInformation,
			hndl_info, hi_size, &req_size)
				== STATUS_INFO_LENGTH_MISMATCH)) {
		hi_size = req_size + 4096;
		hndl_info = (PSYSTEM_HANDLE_INFORMATION_EX)REALLOC(hndl_info,
				hi_size);
	}
	UT_ASSERT(status >= 0);

	DWORD pid = GetProcessId(GetCurrentProcess());

	DWORD ti_size = 4096; /* initial size */
	POBJECT_TYPE_INFORMATION type_info =
		(POBJECT_TYPE_INFORMATION)MALLOC(ti_size);

	DWORD ni_size = 4096; /* initial size */
	PVOID name_info = MALLOC(ni_size);

	for (ULONG i = 0; i < hndl_info->NumberOfHandles; i++) {
		SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle
			= hndl_info->Handles[i];
		char name[MAX_PATH];

		/* ignore handles not owned by current process */
		if ((ULONGLONG)handle.UniqueProcessId != pid)
			continue;

		/* query the object type */
		status = NtQueryObject(handle.HandleValue,
			ObjectTypeInformation, type_info, ti_size, NULL);
		if (status < 0)
			continue; /* if handle can't be queried, ignore it */

		/*
		 * Register/verify only handles of selected types.
		 * Do not rely on type numbers - check type name instead.
		 */
		if (wcscmp(type_info->Name.Buffer, L"Directory") &&
		    wcscmp(type_info->Name.Buffer, L"Mutant") &&
		    wcscmp(type_info->Name.Buffer, L"Semaphore") &&
		    wcscmp(type_info->Name.Buffer, L"File")) {
			/* does not match any of the above types */
			continue;
		}

		/*
		 * Skip handles with access 0x0012019f.  NtQueryObject() may
		 * hang on querying the handles pointing to named pipes.
		 */
		if (handle.GrantedAccess == 0x0012019f)
			continue;

		int ret = snprintf(name, MAX_PATH, "%.*S",
			type_info->Name.Length / 2, type_info->Name.Buffer);

		if (ret < 0 || ret >= MAX_PATH)
			UT_FATAL("!snprintf");

		int fd = (int)(ULONGLONG)handle.HandleValue;
		if (op == 0)
			Fd_lut = open_file_add(Fd_lut, fd, name);
		else
			open_file_remove(Fd_lut, fd, name);
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

#ifdef _WIN32
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	Ut_mmap_align = si.dwAllocationGranularity;

	if (os_getenv("UNITTEST_NO_ABORT_MSG") != NULL) {
		/* disable windows error message boxes */
		ut_suppress_errmsg();
	}
	os_mutex_init(&Sigactions_lock);
#else
	Ut_mmap_align = Ut_pagesize;
#endif
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

	int ret = snprintf(logname, MAXLOGFILENAME, "out%s.log", logsuffix);
	if (ret < 0 || ret >= MAXLOGFILENAME)
		UT_FATAL("!snprintf");
	if ((Outfp = os_fopen(logname, fmode)) == NULL) {
		perror(logname);
		exit(1);
	}

	ret = snprintf(logname, MAXLOGFILENAME, "err%s.log", logsuffix);
	if (ret < 0 || ret >= MAXLOGFILENAME)
		UT_FATAL("!snprintf");
	if ((Errfp = os_fopen(logname, fmode)) == NULL) {
		perror(logname);
		exit(1);
	}

	ret = snprintf(logname, MAXLOGFILENAME, "trace%s.log", logsuffix);
	if (ret < 0 || ret >= MAXLOGFILENAME)
		UT_FATAL("!snprintf");
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

#ifdef _WIN32
/*
 * ut_startW -- initialize unit test framework, indicate test started
 */
void
ut_startW(const char *file, int line, const char *func,
	int argc, wchar_t * const argv[], const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ut_start_common(file, line, func, fmt, ap);
	out(OF_NONL, 0, "     args:");
	for (int i = 0; i < argc; i++) {
		char *str = ut_toUTF8(argv[i]);
		UT_ASSERTne(str, NULL);
		out(OF_NONL, " %s", str);
		free(str);
	}
	out(0, NULL);

	va_end(ap);
}
#endif

/*
 * ut_done -- indicate test is done, exit program
 */
void
ut_done(const char *file, int line, const char *func,
    const char *fmt, ...)
{
#ifdef _WIN32
	os_mutex_destroy(&Sigactions_lock);
#endif
	va_list ap;

	va_start(ap, fmt);

	if (!os_getenv("UNITTEST_DO_NOT_CHECK_OPEN_FILES"))
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

	return (uint16_t)((sum2 << 8) | sum1);
}

#ifdef _WIN32

/*
 * ut_toUTF8 -- convert WCS to UTF-8 string
 */
char *
ut_toUTF8(const wchar_t *wstr)
{
	int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, -1,
		NULL, 0, NULL, NULL);
	if (size == 0) {
		UT_FATAL("!ut_toUTF8");
	}

	char *str = malloc(size * sizeof(char));
	if (str == NULL) {
		UT_FATAL("!ut_toUTF8");
	}

	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, -1, str,
		size, NULL, NULL) == 0) {
		UT_FATAL("!ut_toUTF8");
	}

	return str;
}

/*
 * ut_toUTF16 -- convert UTF-8 to WCS string
 */
wchar_t *
ut_toUTF16(const char *wstr)
{
	int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, wstr, -1,
					NULL, 0);
	if (size == 0) {
		UT_FATAL("!ut_toUTF16");
	}

	wchar_t *str = malloc(size * sizeof(wchar_t));
	if (str == NULL) {
		UT_FATAL("!ut_toUTF16");
	}

	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, wstr, -1, str,
				size) == 0) {
		UT_FATAL("!ut_toUTF16");
	}

	return str;
}
#endif

/*
 * ut_strtoull -- a strtoull call that cannot return error
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
 * ut_strtoll -- a strtoll call that cannot return error
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
