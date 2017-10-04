---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMOBJ!7
collection: libpmemobj
header: NVM Library
date: pmemobj API version 2.2
...

[comment]: <> (Copyright 2016-2017, Intel Corporation)

[comment]: <> (Redistribution and use in source and binary forms, with or without)
[comment]: <> (modification, are permitted provided that the following conditions)
[comment]: <> (are met:)
[comment]: <> (    * Redistributions of source code must retain the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer.)
[comment]: <> (    * Redistributions in binary form must reproduce the above copyright)
[comment]: <> (      notice, this list of conditions and the following disclaimer in)
[comment]: <> (      the documentation and/or other materials provided with the)
[comment]: <> (      distribution.)
[comment]: <> (    * Neither the name of the copyright holder nor the names of its)
[comment]: <> (      contributors may be used to endorse or promote products derived)
[comment]: <> (      from this software without specific prior written permission.)

[comment]: <> (THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS)
[comment]: <> ("AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR)
[comment]: <> (A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT)
[comment]: <> (OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,)
[comment]: <> (SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT)
[comment]: <> (LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,)
[comment]: <> (DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY)
[comment]: <> (THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT)
[comment]: <> ((INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE)
[comment]: <> (OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.)

[comment]: <> (libpmemobj.7 -- man page for libpmemobj)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />


[TRANSACTIONAL OBJECT MANIPULATION](#transactional-object-manipulation-1)<br />
[CAVEATS](#caveats)<br />
[LIBRARY API VERSIONING](#library-api-versioning-1)<br />
[MANAGING LIBRARY BEHAVIOR](#managing-library-behavior)<br />
[DEBUGGING AND ERROR HANDLING](#debugging-and-error-handling)<br />
[CONTROL AND STATISTICS](#control-and-statistics)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**libpmemobj** -- persistent memory transactional object store


# SYNOPSIS #

```c
#include <libpmemobj.h>
cc -std=gnu99 ... -lpmemobj -lpmem
```

!ifdef{WIN32}
{
>NOTE: NVML API supports UNICODE. If **NVML_UTF8_API** macro is defined then
basic API functions are expanded to UTF-8 API with postfix *U*,
otherwise they are expanded to UNICODE API with postfix *W*.
}

##### Most commonly used functions: #####

```c
!ifdef{WIN32}
{
PMEMobjpool *pmemobj_openU(const char *path, const char *layout);
PMEMobjpool *pmemobj_openW(const wchar_t *path, const wchar_t *layout);
PMEMobjpool *pmemobj_createU(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
PMEMobjpool *pmemobj_createW(const wchar_t *path, const wchar_t *layout,
	size_t poolsize, mode_t mode);
}{
PMEMobjpool *pmemobj_open(const char *path, const char *layout);
PMEMobjpool *pmemobj_create(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
}
void pmemobj_close(PMEMobjpool *pop);
```

##### Low-level memory manipulation: #####

```c
void *(PMEMobjpool *pop, void *dest,
	const void *src, size_t len);
void *pmemobj_memset_persist(PMEMobjpool *pop, void *dest,
	int c, size_t len);
void pmemobj_persist(PMEMobjpool *pop, const void *addr, size_t len);
void pmemobj_flush(PMEMobjpool *pop, const void *addr, size_t len);
void pmemobj_drain(PMEMobjpool *pop);
```

##### Locking: #####

```c
void pmemobj_mutex_zero(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_lock(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_timedlock(PMEMobjpool *pop, PMEMmutex *restrict mutexp,
	const struct timespec *restrict abs_timeout);
int pmemobj_mutex_trylock(PMEMobjpool *pop, PMEMmutex *mutexp);
int pmemobj_mutex_unlock(PMEMobjpool *pop, PMEMmutex *mutexp);

void pmemobj_rwlock_zero(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_rdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_wrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_timedrdlock(PMEMobjpool *pop, PMEMrwlock *restrict rwlockp,
	const struct timespec *restrict abs_timeout);
int pmemobj_rwlock_timedwrlock(PMEMobjpool *pop, PMEMrwlock *restrict rwlockp,
	const struct timespec *restrict abs_timeout);
int pmemobj_rwlock_tryrdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_trywrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
int pmemobj_rwlock_unlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);

void pmemobj_cond_zero(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_broadcast(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_signal(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_timedwait(PMEMobjpool *pop, PMEMcond *restrict condp,
	PMEMmutex *restrict mutexp, const struct timespec *restrict abs_timeout);
int pmemobj_cond_wait(PMEMobjpool *pop, PMEMcond *restrict condp,
	PMEMmutex *restrict mutexp);
```

##### Persistent object identifier: #####

```c
OID_IS_NULL(PMEMoid oid)
OID_EQUALS(PMEMoid lhs, PMEMoid rhs)
```

##### Type-safety: #####

```c
TOID(TYPE)
TOID_DECLARE(TYPE, uint64_t type_num)
TOID_DECLARE_ROOT(ROOT_TYPE)

TOID_TYPE_NUM(TYPE)
TOID_TYPE_NUM_OF(TOID oid)
TOID_VALID(TOID oid)
OID_INSTANCEOF(PMEMoid oid, TYPE)

TOID_ASSIGN(TOID oid, VALUE)

TOID_IS_NULL(TOID oid)
TOID_EQUALS(TOID lhs, TOID rhs)
TOID_TYPEOF(TOID oid)
TOID_OFFSETOF(TOID oid, FIELD)
DIRECT_RW(TOID oid)
DIRECT_RO(TOID oid)
D_RW(TOID oid)
D_RO(TOID oid)
```

##### Layout declaration: #####

```c
POBJ_LAYOUT_BEGIN(layout)
POBJ_LAYOUT_TOID(layout, TYPE)
POBJ_LAYOUT_ROOT(layout, ROOT_TYPE)
POBJ_LAYOUT_NAME(layout)
POBJ_LAYOUT_END(layout)
POBJ_LAYOUT_TYPES_NUM(layout)
```

##### Non-transactional atomic allocations: #####

```c
typedef int (*pmemobj_constr)(PMEMobjpool *pop, void *ptr, void *arg);

int pmemobj_alloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num,
	pmemobj_constr constructor, void *arg);
int pmemobj_zalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num);
int pmemobj_xalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num,
	uint64_t flags, pmemobj_constr constructor, void *arg);
int pmemobj_realloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num);
int pmemobj_zrealloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num);
int pmemobj_strdup(PMEMobjpool *pop, PMEMoid *oidp, const char *s, uint64_t type_num);
int pmemobj_wcsdup(PMEMobjpool *pop, PMEMoid *oidp, const wchar_t *s, uint64_t type_num);
void pmemobj_free(PMEMoid *oidp);

size_t pmemobj_alloc_usable_size(PMEMoid oid);
PMEMobjpool *pmemobj_pool_by_oid(PMEMoid oid);
PMEMobjpool *pmemobj_pool_by_ptr(const void *addr);
void *pmemobj_direct(PMEMoid oid);
PMEMoid pmemobj_oid(const void *addr); (EXPERIMENTAL)
uint64_t pmemobj_type_num(PMEMoid oid);

POBJ_NEW(PMEMobjpool *pop, TOID *oidp, TYPE,
	pmemobj_constr constructor, void *arg)
POBJ_ALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size,
	pmemobj_constr constructor, void *arg)
POBJ_ZNEW(PMEMobjpool *pop, TOID *oidp, TYPE)
POBJ_ZALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
POBJ_REALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
POBJ_ZREALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
POBJ_FREE(TOID *oidp)
```

##### Root object management: #####

```c
PMEMoid pmemobj_root(PMEMobjpool *pop, size_t size);
PMEMoid pmemobj_root_construct(PMEMobjpool *pop, size_t size,
	pmemobj_constr constructor, void *arg);
size_t pmemobj_root_size(PMEMobjpool *pop);

POBJ_ROOT(PMEMobjpool *pop, TYPE)
```

##### Object containers: #####

```c
PMEMoid pmemobj_first(PMEMobjpool *pop);
PMEMoid pmemobj_next(PMEMoid oid);

POBJ_FIRST_TYPE_NUM(PMEMobjpool *pop, uint64_t type_num)
POBJ_FIRST(PMEMobjpool *pop, TYPE)
POBJ_NEXT_TYPE_NUM(PMEMoid oid)
POBJ_NEXT(TOID oid)

POBJ_FOREACH(PMEMobjpool *pop, PMEMoid varoid)
POBJ_FOREACH_SAFE(PMEMobjpool *pop, PMEMoid varoid, PMEMoid nvaroid)
POBJ_FOREACH_TYPE(PMEMobjpool *pop, TOID var)
POBJ_FOREACH_SAFE_TYPE(PMEMobjpool *pop, TOID var, TOID nvar)
```

##### Non-transactional persistent atomic circular doubly-linked list: #####

```c
int pmemobj_list_insert(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid dest, int before, PMEMoid oid);
PMEMoid pmemobj_list_insert_new(PMEMobjpool *pop, size_t pe_offset,
	void *head, PMEMoid dest, int before, size_t size,
	uint64_t type_num, pmemobj_constr constructor, void *arg);
int pmemobj_list_remove(PMEMobjpool *pop, size_t pe_offset,
	void *head, PMEMoid oid, int free);
int pmemobj_list_move(PMEMobjpool *pop,
	size_t pe_old_offset, void *head_old,
	size_t pe_new_offset, void *head_new,
	PMEMoid dest, int before, PMEMoid oid);

POBJ_LIST_ENTRY(TYPE)
POBJ_LIST_HEAD(HEADNAME, TYPE)
POBJ_LIST_FIRST(POBJ_LIST_HEAD *head)
POBJ_LIST_NEXT(TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_LAST(POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_PREV(TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_EMPTY(POBJ_LIST_HEAD *head)
POBJ_LIST_DEST_HEAD
POBJ_LIST_DEST_TAIL

POBJ_LIST_FOREACH(TOID var, POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_FOREACH_REVERSE(TOID var, POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)

POBJ_LIST_INSERT_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_INSERT_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_INSERT_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_INSERT_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_INSERT_NEW_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
POBJ_LIST_INSERT_NEW_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_ENTRY FIELD, size_t size,
	void (*constructor)(PMEMobjpool *pop, void *ptr, void *arg),
	void *arg)
POBJ_LIST_INSERT_NEW_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
POBJ_LIST_INSERT_NEW_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
POBJ_LIST_REMOVE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_REMOVE_FREE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
POBJ_LIST_MOVE_ELEMENT_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID elm, POBJ_LIST_ENTRY FIELD,
	POBJ_LIST_ENTRY field_new)
POBJ_LIST_MOVE_ELEMENT_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID elm, POBJ_LIST_ENTRY FIELD,
	POBJ_LIST_ENTRY field_new)
POBJ_LIST_MOVE_ELEMENT_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID listelm, TOID elm,
	POBJ_LIST_ENTRY FIELD, POBJ_LIST_ENTRY field_new)
POBJ_LIST_MOVE_ELEMENT_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID listelm, TOID elm,
	POBJ_LIST_ENTRY FIELD, POBJ_LIST_ENTRY field_new)
```

##### Transactional object manipulation: #####

```c
enum tx_stage pmemobj_tx_stage(void);

int pmemobj_tx_begin(PMEMobjpool *pop, jmp_buf *env, enum pobj_tx_param, ...);
int pmemobj_tx_lock(enum tx_lock lock_type, void *lockp);
void pmemobj_tx_abort(int errnum);
void pmemobj_tx_commit(void);
int pmemobj_tx_end(void);
int pmemobj_tx_errno(void);
void pmemobj_tx_process(void);

int pmemobj_tx_add_range(PMEMoid oid, uint64_t off, size_t size);
int pmemobj_tx_add_range_direct(const void *ptr, size_t size);
int pmemobj_tx_xadd_range(PMEMoid oid, uint64_t off, size_t size, uint64_t flags); (EXPERIMENTAL)
int pmemobj_tx_xadd_range_direct(const void *ptr, size_t size, uint64_t flags); (EXPERIMENTAL)

PMEMoid pmemobj_tx_alloc(size_t size, uint64_t type_num);
PMEMoid pmemobj_tx_zalloc(size_t size, uint64_t type_num);
PMEMoid pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags); (EXPERIMENTAL)
PMEMoid pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num);
PMEMoid pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num);
PMEMoid pmemobj_tx_strdup(const char *s, uint64_t type_num);
PMEMoid pmemobj_tx_wcsdup(const wchar_t *s, uint64_t type_num);
int pmemobj_tx_free(PMEMoid oid);

TX_BEGIN_PARAM(PMEMobjpool *pop, ...)
TX_BEGIN_CB(PMEMobjpool *pop, cb, arg, ...) (EXPERIMENTAL)
TX_BEGIN(PMEMobjpool *pop)
TX_ONABORT
TX_ONCOMMIT
TX_FINALLY
TX_END

TX_ADD(TOID o)
TX_ADD_FIELD(TOID o, FIELD)
TX_ADD_DIRECT(TYPE *p)
TX_ADD_FIELD_DIRECT(TYPE *p, FIELD)

TX_XADD(TOID o, uint64_t flags) (EXPERIMENTAL)
TX_XADD_FIELD(TOID o, FIELD, uint64_t flags) (EXPERIMENTAL)
TX_XADD_DIRECT(TYPE *p, uint64_t flags) (EXPERIMENTAL)
TX_XADD_FIELD_DIRECT(TYPE *p, FIELD, uint64_t flags) (EXPERIMENTAL)

TX_NEW(TYPE)
TX_ALLOC(TYPE, size_t size)
TX_ZNEW(TYPE)
TX_ZALLOC(TYPE, size_t size)
TX_XALLOC(TYPE, size_t size, uint64_t flags) (EXPERIMENTAL)
TX_REALLOC(TOID o, size_t size)
TX_ZREALLOC(TOID o, size_t size)
TX_STRDUP(const char *s, uint64_t type_num)
TX_WCSDUP(const wchar_t *s, uint64_t type_num)
TX_FREE(TOID o)

TX_SET(TOID o, FIELD, VALUE)
TX_SET_DIRECT(TYPE *p, FIELD, VALUE)
TX_MEMCPY(void *dest, const void *src, size_t num)
TX_MEMSET(void *dest, int c, size_t num)
```

##### Library API versioning: #####

```c
!ifdef{WIN32}
{
const char *pmemobj_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemobj_check_versionW(
	unsigned major_required,
	unsigned minor_required);
}{
const char *pmemobj_check_version(
	unsigned major_required,
	unsigned minor_required);
}
```

##### Managing library behavior: #####

```c
void pmemobj_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));

!ifdef{WIN32}
{
int pmemobj_checkU(const char *path, const char *layout);
int pmemobj_checkW(const wchar_t *path, const wchar_t *layout);
}{
int pmemobj_check(const char *path, const char *layout);
}
```

##### Error handling: #####

```c
!ifdef{WIN32}
{
const char *pmemobj_errormsgU(void);
const wchar_t *pmemobj_errormsgW(void);
}{
const char *pmemobj_errormsg(void);
}
```

##### Control and statistics: #####

```c
!ifdef{WIN32}
{
int pmemobj_ctl_getU(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_getW(PMEMobjpool *pop, const wchar_t *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_setU(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_setW(PMEMobjpool *pop, const wchar_t *name, void *arg); (EXPERIMENTAL)
}{
int pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
}
```

# DESCRIPTION #

**libpmemobj** provides a transactional object store in *persistent memory* (pmem).
This library is intended for applications using direct access storage
(DAX), which is storage that supports load/store access without paging blocks
from a block storage device. Some types of *non-volatile memory DIMMs* (NVDIMMs)
provide this type of byte addressable access to storage. A *persistent memory aware
file system* is typically used to expose the direct access to applications.
Memory mapping a file from this type of file system results in the load/store,
non-paged access to pmem. **libpmemobj** builds on this type of memory mapped file.

This library is for applications that need a transactions and persistent memory management.
!ifndef{WIN32}
{The **libpmemobj** requires a **-std=gnu99** compilation flag to build properly.}
This library builds on the low-level pmem support provided by **libpmem**, handling
the transactional updates, flushing changes to persistence, and recovery for the application.

**libpmemobj** is one of a collection of persistent memory libraries available, the others are:

+ **libpmemblk**(3), providing pmem-resident arrays of fixed-sized blocks with atomic updates.

+ **libpmemlog**(3), providing a pmem-resident log file.

+ **libpmem**(3), low-level persistent memory support.

Under normal usage, **libpmemobj** will never print messages or intentionally cause
the process to exit. The only exception to this is the debugging
information, when enabled, as described under **DEBUGGING AND ERROR HANDLING** below.


# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing applications
to work with an evolving API.

```c
!ifdef{WIN32}
{
const char *pmemobj_check_versionU(
	unsigned major_required,
	unsigned minor_required);
const wchar_t *pmemonj_check_versionW(
	unsigned major_required,
	unsigned minor_required);
}{
const char *pmemobj_check_version(
	unsigned major_required,
	unsigned minor_required);
}
```

The !pmemobj_check_version function is used to see if the installed **libpmemobj**
supports the version of the library API required by an application. The easiest way
to do this is for the application to supply the compile-time version information,
supplied by defines in **\<libpmemobj.h\>**, like this:

```c
reason = pmemobj_check_version!U{}(PMEMOBJ_MAJOR_VERSION,
                               PMEMOBJ_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a library
with a newer minor version number will pass this check since increasing minor
versions imply backwards compatibility.

An application can also check specifically for the existence of an interface
by checking for the version where that interface was introduced. These versions
are documented in this man page as follows: unless otherwise specified, all
interfaces described here are available in version 1.0 of the library. Interfaces
added after version 1.0 will contain the text *introduced in version x.y* in
the section of this manual describing the feature.

When the version check performed by !pmemobj_check_version is successful, the retur
value is NULL. Otherwise the return value is a static string describing the reason
for failing the version check. The string returned by !pmemobj_check_version
must not be modified or freed.


# MANAGING LIBRARY BEHAVIOR #

The library entry points described in this section are less commonly used than
the previous sections.

```c
void pmemobj_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
```

The **pmemobj_set_funcs**() function allows an application to override memory
allocation calls used internally by **libpmemobj**. Passing in NULL for any of
the handlers will cause the **libpmemobj** default function to be used. The library
does not make heavy use of the system malloc functions, but it does allocate
approximately 4-8 kilobytes for each memory pool in use.

```c
!ifdef{WIN32}
{
int pmemobj_checkU(const char *path, const char *layout);
int pmemobj_checkW(const wchar_t *path, const wchar_t *layout);
}{
int pmemobj_check(const char *path, const char *layout);
}
```

The !pmemobj_check function performs a consistency check of the file indicated by
*path* and returns 1 if the memory pool is found to be consistent. Any
inconsistencies found will cause !pmemobj_check to return 0, in which case the use of
the file with **libpmemobj** will result in undefined behavior. The debug version of
**libpmemobj** will provide additional details on inconsistencies when **PMEMOBJ_LOG_LEVEL**
is at least 1, as described in the **DEBUGGING ANDERROR HANDLING** section below.
!pmemobj_check will return -1 and set *errno* if it cannot perform the consistency
check due to other errors. !pmemobj_check opens the given *path* read-only so
it never makes any changes to the file. This function is not supported on Device DAX.


# DEBUGGING AND ERROR HANDLING #

!ifndef{WIN32}
{Two versions of **libpmemobj** are typically available on a development system.
The normal version, accessed when a program is linked using the **-lpmemobj**
option, is optimized for performance. That version skips checks that impact
performance and never logs any trace information or performs any run-time assertions.}
If an error is detected during the call to **libpmemobj** function, an application may
retrieve an error message describing the reason of failure using the following function:

```c
!ifdef{WIN32}
{
const char *pmemobj_errormsgU(void);
const wchar_t *pmemobj_errormsgW(void);
}{
const char *pmemobj_errormsg(void);
}
```

The !pmemobj_errormsg function returns a pointer to a static buffer containing
the last error message logged for current thread. The error message may
include description of the corresponding error code (if *errno* was set),
as returned by **strerror**(3). The error message buffer is thread-local;
errors encountered in one thread do not affect its value in other threads.
The buffer is never cleared by any library function; its content is significant
only when the return value of the immediately preceding call to **libpmemobj**
function indicated an error, or if *errno* was set. The application must not modify
or free the error message string, but it may be modified by subsequent calls to other
library functions.

A second version of **libpmemobj**, accessed when a program uses
the libraries under !ifdef{WIN32}{**/nvml/src/x64/Debug**}{**/usr/lib/nvml_debug**},
contains run-time assertions and trace points. The typical way to
access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to !ifdef{WIN32}{**/nvml/src/x64/Debug** or other location}
{**/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug**} depending on where the debug
libraries are installed on the system.
The trace points in the debug version of the library are enabled using the environment
variable **PMEMOBJ_LOG_LEVEL** which can be set to the following values:

+ **0** - This is the default level when **PMEMOBJ_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged
(in addition to returning the *errno*-based errors as usual).
The same information may be retrieved using !pmemobj_errormsg.

+ **2** - A trace of basic operations is logged.

+ **3** - This level enables a very verbose amount of function call
tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing information
that is likely only useful to the **libpmemobj** developers.

The environment variable **PMEMOBJ_LOG_FILE** specifies a file name where all
logging information should be written. If the last character in the name is
"-", the PID of the current process will be appended to the file name when the
log file is created. If **PMEMOBJ_LOG_FILE** is not set, the logging output
goes to stderr.

Setting the environment variable **PMEMOBJ_LOG_LEVEL** has no effect on the
non-debug version of **libpmemobj**. See also **libpmem**(3) to get information
about other environment variables affecting **libpmemobj** behavior.

**libpmemobj** by default supports up to 1024 parallel transactions / allocations.
For debugging purposes it is possible to decrease this value by writing
a desired limit to the **PMEMOBJ_NLANES** environment variable.


# CONTROL AND STATISTICS #

The library provides a uniform interface that allows to impact its behavior as
well as reason about its internals.

There are two main functions to that interface:

```c
!ifdef{WIN32}
{
int pmemobj_ctl_getU(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_getW(PMEMobjpool *pop, const wchar_t *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_setU(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_setW(PMEMobjpool *pop, const wchar_t *name, void *arg); (EXPERIMENTAL)
}{
int pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
}
```

For more details look at **pmemobj_ctl_get**(3) and **pmemobj_ctl_set**(3) manpages.


# EXAMPLE #

See <http://pmem.io/nvml/libpmemobj> for examples using the **libpmemobj** API.


# ACKNOWLEDGEMENTS #

**libpmemobj** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**pmemobj_ctl_get**(3), **pmemobj_ctl_set**(3), **strerror**(3),
**libpmemblk**(7), **libpmemlog**(7), **libpmem**(7),
**libvmem**(7) and **<http://pmem.io>**
