---
layout: manual
Content-Style: 'text/css'
title: LIBPMEMOBJ(3)
header: NVM Library
date: pmemobj API version 2.1
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

[comment]: <> (libpmemobj.3 -- man page for libpmemobj)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[MOST COMMONLY USED FUNCTIONS](#most-commonly-used-functions-1)<br />
[LOW-LEVEL MEMORY MANIPULATION](#low-level-memory-manipulation-1)<br />
[POOL SETS AND REPLICAS](#pool-sets-and-replicas)<br />
[LOCKING](#locking-1)<br />
[PERSISTENT OBJECTS](#persistent-objects)<br />
[TYPE-SAFETY](#type-safety-1)<br />
[LAYOUT DECLARATION](#layout-declaration-1)<br />
[OBJECT CONTAINERS](#object-containers-1)<br />
[ROOT OBJECT MANAGEMENT](#root-object-management-1)<br />
[NON-TRANSACTIONAL ATOMIC ALLOCATIONS](#non-transactional-atomic-allocations-1)<br />
[NON-TRANSACTIONAL PERSISTENT ATOMIC LISTS](#non-transactional-persistent-atomic-lists)<br />
[TYPE-SAFE NON-TRANSACTIONAL PERSISTENT ATOMIC LISTS](#type-safe-non-transactional-persistent-atomic-lists)<br />
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

##### Most commonly used functions: #####

```c
PMEMobjpool *pmemobj_open(const char *path, const char *layout);
PMEMobjpool *pmemobj_create(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
void pmemobj_close(PMEMobjpool *pop);
```

##### Low-level memory manipulation: #####

```c
void *pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest,
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

int pmemobj_tx_ctx_set(struct pobj_tx_ctx *new_ctx, struct pobj_tx_ctx **old_ctx);
int pmemobj_tx_ctx_restore(struct pobj_tx_ctx *ctx);

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
TX_BEGIN_CB(PMEMobjpool *pop, cb, arg, ...)
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
const char *pmemobj_check_version(
	unsigned major_required,
	unsigned minor_required);
```

##### Managing library behavior: #####

```c
void pmemobj_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));

int pmemobj_check(const char *path, const char *layout);
```

##### Error handling: #####

```c
const char *pmemobj_errormsg(void);
```

##### Control and statistics: #####

```c
int pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg);
```

# DESCRIPTION #

**libpmemobj** provides a transactional object store in *persistent memory* (pmem). This library is intended for applications using direct access storage
(DAX), which is storage that supports load/store access without paging blocks from a block storage device. Some types of *non-volatile memory DIMMs* (NVDIMMs)
provide this type of byte addressable access to storage. A *persistent memory aware file system* is typically used to expose the direct access to applications.
Memory mapping a file from this type of file system results in the load/store, non-paged access to pmem. **libpmemobj** builds on this type of memory mapped
file.

This library is for applications that need a transactions and persistent memory management. The **libpmemobj** requires a **-std=gnu99** compilation flag to
build properly. This library builds on the low-level pmem support provided by **libpmem**, handling the transactional updates, flushing changes to persistence,
and recovery for the application.

**libpmemobj** is one of a collection of persistent memory libraries available, the others are:

+ **libpmemblk**(3), providing pmem-resident arrays of fixed-sized blocks with atomic updates.

+ **libpmemlog**(3), providing a pmem-resident log file.

+ **libpmem**(3), low-level persistent memory support.

Under normal usage, **libpmemobj** will never print messages or intentionally cause the process to exit. The only exception to this is the debugging
information, when enabled, as described under **DEBUGGING AND ERROR HANDLING** below.


# MOST COMMONLY USED FUNCTIONS #

To use the pmem-resident transactional object store provided by **libpmemobj**,
a *memory pool* is first created.
This is done with the **pmemobj_create**() function described in this section.
The other functions described in this section then operate on the resulting
memory pool.

Additionally, none of the three functions described below are thread-safe with
respect to any other **libpmemobj** functions. In other words, when creating,
opening or deleting a pool, nothing else in the library can happen in parallel.

Once created, the memory pool is represented by an opaque handle, of type *PMEMobjpool\**, which is passed to most of the other functions in this section.
Internally, **libpmemobj** will use either **pmem_persist**() or **msync**(2) when it needs to flush changes, depending on whether the memory pool appears to
be persistent memory or a regular file (see the **pmem_is_pmem**() function in **libpmem**(3) for more information). There is no need for applications to flush
changes directly when using the obj memory API provided by **libpmemobj**.

```c
PMEMobjpool *pmemobj_open(const char *path, const char *layout);
```

The **pmemobj_open**() function opens an existing object store memory pool, returning a memory pool handle used with most of the functions in this section.
*path* must be an existing file containing a pmemobj memory pool as created by **pmemobj_create**(). If *layout* is non-NULL, it is compared to the layout
name provided to **pmemobj_create**() when the pool was first created. This can be used to verify the layout of the pool matches what was expected. The
application must have permission to open the file and memory map it with read/write permissions. If an error prevents the pool from being opened, or if the
given *layout* does not match the pool's layout, **pmemobj_open**() returns NULL and sets *errno* appropriately.

```c
PMEMobjpool *pmemobj_create(const char *path, const char *layout,
	size_t poolsize, mode_t mode);
```

The **pmemobj_create**() function creates a transactional object store with the given total *poolsize*. *path* specifies the name of the memory pool file to be
created. *layout* specifies the application's layout type in the form of a string. The layout name is not interpreted by **libpmemobj**, but may be used as a
check when **pmemobj_open**() is called. The layout name, including the terminating null byte ('\0'), cannot be longer than **PMEMOBJ_MAX_LAYOUT** as defined in
**\<libpmemobj.h\>**. It is allowed to pass NULL as *layout*, which is equivalent for using an empty string as a layout name. *mode* specifies the permissions to
use when creating the file as described by **creat**(2). The memory pool file is fully allocated to the size *poolsize* using **posix_fallocate**(3). The
caller may choose to take responsibility for creating the memory pool file by creating it before calling **pmemobj_create**() and then specifying *poolsize* as
zero. In this case **pmemobj_create**() will take the pool size from the size of the existing file and will verify that the file appears to be empty by
searching for any non-zero data in the pool header at the beginning of the file. The minimum file size allowed by the library for a local transactional object store
is defined in **\<libpmemobj.h\>** as **PMEMOBJ_MIN_POOL**. For remote replicas the minimum file size is defined in
**\<librpmem.h\>** as **RPMEM_MIN_PART**.

```c
void pmemobj_close(PMEMobjpool *pop);
```

The **pmemobj_close**() function closes the memory pool indicated by *pop* and deletes the memory pool handle. The object store itself lives on in the file
that contains it and may be re-opened at a later time using **pmemobj_open**() as described above.


# LOW-LEVEL MEMORY MANIPULATION #

The **libpmemobj** specific low-level memory manipulation functions leverage the knowledge of the additional configuration options available for **libpmemobj**
pools, such as replication. They also take advantage of the type of storage behind the pool and use appropriate flush/drain functions. It is advised to use
these functions in conjunction with **libpmemobj** objects, instead of using low-level memory manipulations functions from **libpmem**.

```c
void pmemobj_persist(PMEMobjpool *pop, const void *addr, size_t len);
```

Forces any changes in the range \[*addr*, *addr*+*len*) to be stored durably in persistent memory. Internally this may call either **pmem_msync**() or
**pmem_persist**(). There are no alignment restrictions on the range described by *addr* and *len*, but **pmemobj_persist**() may expand the range as necessary
to meet platform alignment requirements.

>WARNING:
Like **msync**(2), there is nothing atomic or transactional about this call. Any unwritten stores in the given range will be written, but some stores
may have already been written by virtue of normal cache eviction/replacement policies. Correctly written code must not depend on stores waiting until
**pmemobj_persist**() is called to become persistent - they can become persistent at any time before **pmemobj_persist**() is called.

```c
void pmemobj_flush(PMEMobjpool *pop, const void *addr, size_t len);
void pmemobj_drain(PMEMobjpool *pop);
```

These functions provide partial versions of the **pmemobj_persist**() function described above. **pmemobj_persist**() can be thought of as this:

```c
void
pmemobj_persist(PMEMobjpool *pop, const void *addr, size_t len)
{
	/* flush the processor caches */
	pmemobj_flush(pop, addr, len);

	/* wait for any pmem stores to drain from HW buffers */
	pmemobj_drain(pop);
}
```

These functions allow advanced programs to create their own variations of **pmemobj_persist**(). For example, a program that needs to flush several
discontiguous ranges can call **pmemobj_flush**() for each range and then follow up by calling **pmemobj_drain**() once. For more information on partial
flushing operations see the **libpmem** manpage.

```c
void *pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest,
	const void *src, size_t len);
void *pmemobj_memset_persist(PMEMobjpool *pop, void *dest,
	int c, size_t len);
```

The **pmemobj_memcpy_persist**(), and **pmemobj_memset_persist**(), functions provide the same memory copying as their namesakes **memcpy**(3), and
**memset**(3), and ensure that the result has been flushed to persistence before returning. For example, the following code is functionally equivalent to
**pmemobj_memcpy_persist**():

```c
void *
pmemobj_memcpy_persist(PMEMobjpool *pop, void *dest,
	const void *src, size_t len)
{
	void *retval = memcpy(dest, src, len);

	pmemobj_persist(pop, dest, len);

	return retval;
}
```


# POOL SETS AND REPLICAS #

Depending on the configuration of the system, the available space of non-volatile memory space may be divided into multiple memory devices. In such case, the
maximum size of the transactional object store could be limited by the capacity of a single memory device. The **libpmemobj** allows building transactional
object stores spanning multiple memory devices by creation of persistent memory pools consisting of multiple files, where each part of such a *pool set* may be
stored on different pmem-aware filesystem.

To improve reliability and eliminate the single point of failure, all the changes of the data stored in the persistent memory pool could be also automatically
written to local or remote pool replicas, thereby providing a backup for a persistent memory pool by producing a *mirrored pool set*. In practice, the pool
replicas may be considered as binary copies of the "master" pool set.

Creation of all the parts of the pool set and the associated replica sets can be done with the **pmemobj_create**() function or by using the **pmempool**(1)
utility.

Restoring data from a local or remote replica can be done by using the
**pmempool-sync**(1) command or **pmempool_sync**() API from the
**libpmempool**(3) library.

Modifications of a pool set file configuration can be done by using the
**pmempool-transform**(1) command or **pmempool_transform**() API from the
**libpmempool**(3) library.

When creating the pool set consisting of multiple files, or when creating the replicated pool set, the *path* argument passed to **pmemobj_create**() must
point to the special *set* file that defines the pool layout and the location of all the parts of the pool set. The *poolsize* argument must be 0. The meaning
of *layout* and *mode* arguments doesn't change, except that the same *mode* is used for creation of all the parts of the pool set and replicas. If the error
prevents any of the pool set files from being created, **pmemobj_create**() returns NULL and sets *errno* appropriately.

When opening the pool set consisting of multiple files, or when opening the replicated pool set, the *path* argument passed to **pmemobj_open**() must not
point to the pmemobj memory pool file, but to the same *set* file that was used for the pool set creation. If an error prevents any of the pool set files from
being opened, or if the actual size of any file does not match the corresponding part size defined in *set* file **pmemobj_open**() returns NULL and sets
*errno* appropriately.

The set file is a plain text file, which must start with the line containing a *PMEMPOOLSET* string, followed by the specification of all the pool parts in the
next lines. For each part, the file size and the absolute path must be provided. The size has to be compliant with the format specified in IEC 80000-13, IEEE
1541 or the Metric Interchange Format. Standards accept SI units with obligatory B - kB, MB, GB, ... (multiplier by 1000) and IEC units with optional "iB"
- KiB, MiB, GiB, ..., K, M, G, ... - (multiplier by 1024). The minimum file size of each part of the pool set is the same as the minimum size allowed
for a transactional object store consisting of one file. It is defined in **\<libpmemobj.h\>** as **PMEMOBJ_MIN_POOL**.

Sections defining the replica sets are optional. There could be multiple replica sections and each must start with the line containing a *REPLICA* string.
Lines starting with "#" character are ignored. A replica can be local or remote. In case of a local replica, the REPLICA line has to consist of the *REPLICA*
string only and it has to be followed by at least one line defining a part of the local replica. The format of such line is the same as the format of the line
defining a part of the PMEMOBJ pool as described above.

The path of a part can point to a Device DAX and in such case the size
argument can be set to an "AUTO" string, which means that the size of the device
will be automatically resolved at pool creation time.
When using Device DAX there's also one additional restriction - it is not allowed
to concatenate more than one Device DAX device in a single replica
if the configured internal alignment is other than 4KiB.  In such case given
replica can consist only of a single part (single Device DAX).
Please see **ndctl-create-namespace**(1) for information on how to configure
desired alignment on Device DAX.

Device DAX is the device-centric analogue of Filesystem DAX. It allows memory
ranges to be allocated and mapped without need of an intervening file system.
For more information please see **ndctl-create-namespace**(1).

In case of a remote replica, the *REPLICA* keyword has to be followed by
an address of a remote host (in the format recognized by the **ssh**(1)
remote login client) and a relative path to a remote pool set file (located
in the root config directory on the target node - see **librpmem**(3)):

```
REPLICA [<user>@]<hostname> [<relative-path>/]<remote-pool-set-file>
```

There are no other lines in the remote replica section - the REPLICA line
defines a remote replica entirely. Here is the example of "myobjpool.set"
file:

```
PMEMPOOLSET
100G /mountpoint0/myfile.part0
200G /mountpoint1/myfile.part1
400G /mountpoint2/myfile.part2

# local replica
REPLICA
500G /mountpoint3/mymirror.part0
200G /mountpoint4/mymirror.part1

# remote replica
REPLICA user@example.com remote-objpool.set
```

The files in the set may be created by running the following command:

```
$ pmempool create --layout="mylayout" obj myobjpool.set
```

# LOCKING #

**libpmemobj** provides several types of synchronization primitives, designed so as to use them with persistent memory. The locks are not dynamically
allocated, but embedded in pmem-resident objects. For performance reasons, they are also padded up to 64 bytes (cache line size).

Pmem-aware locks implementation is based on the standard POSIX Thread Library, as described in **pthread_mutex**(3), **pthread_rwlock**(3) and
**pthread_cond**(3). They provide semantics similar to standard **pthread** locks, except that they are considered initialized by zeroing them. So allocating
the locks with **pmemobj_zalloc**() or **pmemobj_tx_zalloc**() does not require another initialization step.

The fundamental property of pmem-aware locks is their automatic reinitialization every time the persistent object store pool is open. This way, all the
pmem-aware locks may be considered initialized (unlocked) right after opening the pool, regardless of their state at the time the pool was closed for the last
time.

Pmem-aware mutexes, read/write locks and condition variables must be declared with one of the *PMEMmutex*, *PMEMrwlock*, or *PMEMcond* type respectively.

```c
void pmemobj_mutex_zero(PMEMobjpool *pop, PMEMmutex *mutexp);
```

The **pmemobj_mutex_zero**() function explicitly initializes pmem-aware mutex pointed by *mutexp* by zeroing it. Initialization is not necessary if the object
containing the mutex has been allocated using one of **pmemobj_zalloc**() or **pmemobj_tx_zalloc**() functions.

```c
int pmemobj_mutex_lock(PMEMobjpool *pop, PMEMmutex *mutexp);
```

The **pmemobj_mutex_lock**() function locks pmem-aware mutex pointed by *mutexp*. If the mutex is already locked, the calling thread will block until the mutex
becomes available. If this is the first use of the mutex since opening of the pool *pop*, the mutex is automatically reinitialized and then locked.

```c
int pmemobj_mutex_timedlock(PMEMobjpool *pop, PMEMmutex *restrict mutexp,
	const struct timespec *restrict abs_timeout);
```

The **pmemobj_mutex_timedlock**() performs the same action as **pmemobj_mutex_lock**(), but will not wait beyond *abs_timeout* to obtain the lock before
returning.

```c
int pmemobj_mutex_trylock(PMEMobjpool *pop, PMEMmutex *mutexp);
```

The **pmemobj_mutex_trylock**() function locks pmem-aware mutex pointed by *mutexp*. If the mutex is already locked, **pthread_mutex_trylock**() will not block
waiting for the mutex, but will return an error condition. If this is the first use of the mutex since opening of the pool *pop* the mutex is automatically
reinitialized and then locked.

```c
int pmemobj_mutex_unlock(PMEMobjpool *pop, PMEMmutex *mutexp);
```

The **pmemobj_mutex_unlock**() function unlocks an acquired pmem-aware mutex pointed by *mutexp*. Undefined behavior follows if a thread tries to unlock a
mutex that has not been locked by it, or if a thread tries to release a mutex that is already unlocked or not initialized.

```c
void pmemobj_rwlock_zero(PMEMobjpool *pop, PMEMrwlock *rwlockp);
```

The **pmemobj_rwlock_zero**() function is used to explicitly initialize pmem-aware read/write lock pointed by *rwlockp* by zeroing it. Initialization is not
necessary if the object containing the lock has been allocated using one of **pmemobj_zalloc**() or **pmemobj_tx_zalloc**() functions.

```c
int pmemobj_rwlock_rdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
```

The **pmemobj_rwlock_rdlock**() function acquires a read lock on *rwlockp* provided that lock is not presently held for writing and no writer threads are
presently blocked on the lock. If the read lock cannot be immediately acquired, the calling thread blocks until it can acquire the lock. If this is the first
use of the lock since opening of the pool *pop*, the lock is automatically reinitialized and then acquired.

```c
int pmemobj_rwlock_timedrdlock(PMEMobjpool *pop, PMEMrwlock *restrict rwlockp,
	const struct timespec *restrict abs_timeout);
```

The **pmemobj_rwlock_timedrdlock**() performs the same action, but will not wait beyond *abs_timeout* to obtain the lock before returning.
A thread may hold multiple concurrent read locks. If so, **pmemobj_rwlock_unlock**() must be called once for each lock obtained.
The results of acquiring a read lock while the calling thread holds a write lock are undefined.

```c
int pmemobj_rwlock_wrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
```

The **pmemobj_rwlock_wrlock**() function blocks until a write lock can be acquired against lock pointed by *rwlockp*. If this is the first use of the lock
since opening of the pool *pop*, the lock is automatically reinitialized and then acquired.

```c
int pmemobj_rwlock_timedwrlock(PMEMobjpool *pop, PMEMrwlock *restrict rwlockp,
	const struct timespec *restrict abs_timeout);
```

The **pmemobj_rwlock_timedwrlock**() performs the same action, but will not wait beyond *abs_timeout* to obtain the lock before returning.

```c
int pmemobj_rwlock_tryrdlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
```

The **pmemobj_rwlock_tryrdlock**() function performs the same action as **pmemobj_rwlock_rdlock**(), but does not block if the lock cannot be immediately
obtained.
The results are undefined if the calling thread already holds the lock at the time the call is made.

```c
int pmemobj_rwlock_trywrlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
```

The **pmemobj_rwlock_trywrlock**() function performs the same action as **pmemobj_rwlock_wrlock**(), but does not block if the lock cannot be immediately
obtained.
The results are undefined if the calling thread already holds the lock at the time the call is made.

```c
int pmemobj_rwlock_unlock(PMEMobjpool *pop, PMEMrwlock *rwlockp);
```

The **pmemobj_rwlock_unlock**() function is used to release the read/write lock previously obtained by **pmemobj_rwlock_rdlock**(),
**pmemobj_rwlock_wrlock**(), **pthread_rwlock_tryrdlock**(), or **pmemobj_rwlock_trywrlock**().

```c
void pmemobj_cond_zero(PMEMobjpool *pop, PMEMcond *condp);
```

The **pmemobj_cond_zero**() function explicitly initializes pmem-aware condition variable by zeroing it. Initialization is not necessary if the object
containing the condition variable has been allocated using one of **pmemobj_zalloc**() or **pmemobj_tx_zalloc**() functions.

```c
int pmemobj_cond_broadcast(PMEMobjpool *pop, PMEMcond *condp);
int pmemobj_cond_signal(PMEMobjpool *pop, PMEMcond *condp);
```

The difference between **pmemobj_cond_broadcast**() and **pmemobj_cond_signal**() is that the former unblocks all threads waiting for the condition variable,
whereas the latter blocks only one waiting thread. If no threads are waiting on *condp*, neither function has any effect. If more than one thread is blocked on
a condition variable, the used scheduling policy determines the order in which threads are unblocked. The same mutex used for waiting must be held while
calling either function. Although neither function strictly enforces this requirement, undefined behavior may follow if the mutex is not held.

```c
int pmemobj_cond_timedwait(PMEMobjpool *pop, PMEMcond *restrict condp,
	PMEMmutex *restrict mutexp, const struct timespec *restrict abs_timeout);
int pmemobj_cond_wait(PMEMobjpool *pop, PMEMcond *restrict condp,
	PMEMmutex *restrict mutexp);
```

The **pmemobj_cond_timedwait**() and **pmemobj_cond_wait**() functions shall block on a condition variable. They shall be called with mutex locked by the
calling thread or undefined behavior results. These functions atomically release mutex pointed by *mutexp* and cause the calling thread to block on the
condition variable *condp*; atomically here means "atomically with respect to access by another thread to the mutex and then the condition variable". That
is, if another thread is able to acquire the mutex after the about-to-block thread has released it, then a subsequent call to **pmemobj_cond_broadcast**() or
**pmemobj_cond_signal**() in that thread shall behave as if it were issued after the about-to-block thread has blocked. Upon successful return, the mutex shall
have been locked and shall be owned by the calling thread.


# PERSISTENT OBJECTS #

Each object stored in persistent memory pool is represented by an object handle of type *PMEMoid*. In practice, such a handle is a unique Object IDentifier
(*OID*) of a global scope, which means that two objects from different pools may not have the same *OID*. The special **OID_NULL** macro defines a NULL-like
handle that does not represent any object. The size of a single object is limited by a **PMEMOBJ_MAX_ALLOC_SIZE**. Thus an allocation with requested size greater
than this value will fail.

An *OID* cannot be considered as a direct pointer to an object. Each time the program attempts to read or write object data, it must obtain the current memory
address of the object by converting its *OID* into the pointer.

In contrast to the memory address, the *OID* value for given object does not change during the life of an object (except for realloc operation), and remains
valid after closing and reopening the pool. For this reason, if an object contains a reference to another persistent object - necessary to build some kind of a
linked data structure - it shall never use memory address of an object, but its *OID*.

```c
void *pmemobj_direct(PMEMoid oid);
```

The **pmemobj_direct**() function returns a pointer to an object represented by *oid*.
If **OID_NULL** is passed as an argument, function returns NULL.

>NOTE:
For performance reasons, on Linux this function is inlined by default.
You may decide to compile your programs using the non-inlined variant
of **pmemobj_direct**() by defining **PMEMOBJ_DIRECT_NON_INLINE** macro.
You should define this macro by using *\#define* preprocessor directive,
which must come before *\#include* of **\<libpmemobj.h\>**.
You could also use *\-D* option to gcc.
On Windows **PMEMOBJ_DIRECT_NON_INLINE** macro has no effect.

```c
PMEMoid pmemobj_oid(const void *addr); (EXPERIMENTAL)
```
The **pmemobj_oid**() function returns a *PMEMoid* to an object pointed to by *addr*. If *addr* is not from within a pmemobj pool, **OID_NULL** is returned. If *addr* is not the start of an object (does not point to the beginning of a valid allocation), the resulting *PMEMoid* can be safely used only with:

+ **pmemobj_pool_by_oid**
+ **pmemobj_direct**
+ **pmemobj_tx_add_range**


```c
uint64_t pmemobj_type_num(PMEMoid oid);
```

The **pmemobj_type_num**() function returns a type number of the object represented by *oid*.

```c
PMEMobjpool *pmemobj_pool_by_oid(PMEMoid oid);
```

The **pmemobj_pool_by_oid**() function returns a handle to the pool which contains the object represented by *oid*. If the pool is not open or **OID_NULL** is
passed as an argument, function returns NULL.

```c
PMEMobjpool *pmemobj_pool_by_ptr(const void *addr);
```

The **pmemobj_pool_by_ptr**() function returns a handle to the pool which contains the address. If the address does not belong to any open pool, function
returns NULL.

At the time of allocation (or reallocation), each object may be assigned a number representing its type. Such a *type number* may be used to arrange the
persistent objects based on their actual user-defined structure type, thus facilitating implementation of a simple run-time type safety mechanism. It also
allows to iterate through all the objects of given type stored in the persistent memory pool. See **OBJECT CONTAINERS** section for more details.

```c
OID_IS_NULL(PMEMoid oid)
OID_EQUALS(PMEMoid lhs, PMEMoid rhs)
```

The **OID_IS_NULL**() macro checks if given *PMEMoid* represents a NULL object.

The **OID_EQUALS**() macro compares two *PMEMoid* objects.


# TYPE-SAFETY #

Operating on untyped object handles, as well as on direct untyped object pointers (*void\**) may be confusing and error prone. To facilitate implementation of
type safety mechanism, **libpmemobj** defines a set of macros that provide a static type enforcement, catching potential errors at compile time. For example, a
compile-time error is generated when an attempt is made to assign a handle to an object of one type to the object handle variable of another type of object.

```c
TOID_DECLARE(TYPE, uint64_t type_num)
```

The **TOID_DECLARE**() macro declares a typed *OID* of user-defined type specified by argument *TYPE*, and with type number specified by argument *type_num*.

```c
TOID_DECLARE_ROOT(ROOT_TYPE)
```

The **TOID_DECLARE_ROOT**() macro declares a typed *OID* of user-defined type specified by argument *ROOT_TYPE*, and with type number for root object
**POBJ_ROOT_TYPE_NUM**.

```c
TOID(TYPE)
```

The **TOID**() macro declares a handle to an object of type specified by argument *TYPE*, where *TYPE* is the name of a user-defined structure. The typed *OID*
must be declared first using the **TOID_DECLARE**(), **TOID_DECLARE_ROOT**(), **POBJ_LAYOUT_TOID**() or **POBJ_LAYOUT_ROOT**() macros.

```c
TOID_TYPE_NUM(TYPE)
```

The **TOID_TYPE_NUM**() macro returns a type number of the type specified by argument *TYPE*.

```c
TOID_TYPE_NUM_OF(TOID oid)
```

The **TOID_TYPE_NUM_OF**() macro returns a type number of the object specified by argument *oid*. The type number is read from the typed *OID*.

```c
TOID_VALID(TOID oid)
```

The **TOID_VALID**() macro validates whether the type number stored in object's metadata is equal to the type number read from typed *OID*.

```c
OID_INSTANCEOF(PMEMoid oid, TYPE)
```

The **OID_INSTANCEOF**() macro checks whether the *oid* is of the type specified by argument *TYPE*.

```c
TOID_ASSIGN(TOID o, VALUE)
```

The **TOID_ASSIGN**() macro assigns an object handle specified by *VALUE* to the variable *o*.

```c
TOID_IS_NULL(TOID o)
```

The **TOID_IS_NULL**() macro evaluates to true if the object handle represented by argument *o* has **OID_NULL** value.

```c
TOID_EQUALS(TOID lhs, TOID rhs)
```

The **TOID_EQUALS**() macro evaluates to true if both *lhs* and *rhs* object handles are referencing the same persistent object.

```c
TOID_TYPEOF(TOID o)
```

The **TOID_TYPEOF**() macro returns a type of the object handle represented by argument *o*.

```c
TOID_OFFSETOF(TOID o, FILED)
```

The **TOID_OFFSETOF**() macro returns the offset of the *FIELD* member from the start of the object represented by argument *o*.

```c
DIRECT_RW(TOID oid)
D_RW(TOID oid)
```

The **DIRECT_RW**() macro and its shortened form **D_RW**() return a typed write pointer (*TYPE\**) to an object represented by *oid*. If *oid* holds
**OID_NULL** value, the macro evaluates to NULL.

```c
DIRECT_RO(TOID oid)
D_RO(TOID oid)
```

The **DIRECT_RO**() macro and its shortened form **D_RO**() return a typed read-only (const) pointer (*TYPE\**) to an object represented by *oid*. If *oid*
holds **OID_NULL** value, the macro evaluates to NULL.


# LAYOUT DECLARATION #

The **libpmemobj** defines a set of macros for convenient declaration of pool's layout. The declaration of layout consist of declaration of number of used
types. The declared types will be assigned consecutive type numbers. Declared types may be used in conjunction with type safety macros. Once created the layout
declaration shall not be changed unless the new types are added at the end of the existing layout declaration. Modifying any of existing declaration may lead
to changes in type numbers of declared types which in consequence may cause data corruption.

```c
POBJ_LAYOUT_BEGIN(LAYOUT)
```

The **POBJ_LAYOUT_BEGIN**() macro indicates a begin of declaration of layout. The *LAYOUT* argument is a name of layout. This argument must be passed to all macros
related to the declaration of layout.

```c
POBJ_LAYOUT_TOID(LAYOUT, TYPE)
```

The **POBJ_LAYOUT_TOID**() macro declares a typed *OID* for type passed as *TYPE* argument inside the declaration of layout. All types declared using this macro
are assigned with consecutive type numbers. This macro must be used between the **POBJ_LAYOUT_BEGIN**() and **POBJ_LAYOUT_END**() macros, with the same name passed as
*LAYOUT* argument.

```c
POBJ_LAYOUT_ROOT(LAYOUT, ROOT_TYPE)
```

The **POBJ_LAYOUT_ROOT**() macro declares a typed *OID* for type passed as *ROOT_TYPE* argument inside the declaration of layout. The typed *OID* will be assigned
with type number for root object **POBJ_ROOT_TYPE_NUM**.

```c
POBJ_LAYOUT_END(LAYOUT)
```

The **POBJ_LAYOUT_END**() macro ends the declaration of layout.

```c
POBJ_LAYOUT_NAME(LAYOUT)
```

The **POBJ_LAYOUT_NAME**() macro returns the name of layout as a null-terminated string.

```c
POBJ_LAYOUT_TYPES_NUM(LAYOUT)
```

The **POBJ_LAYOUT_TYPES_NUM**() macro returns number of types declared using the **POBJ_LAYOUT_TOID**() macro within the layout declaration.
This is an example of layout declaration:

```c
POBJ_LAYOUT_BEGIN(mylayout);
POBJ_LAYOUT_ROOT(mylayout, struct root);
POBJ_LAYOUT_TOID(mylayout, struct node);
POBJ_LAYOUT_TOID(mylayout, struct foo);
POBJ_LAYOUT_END(mylayout);

struct root
{
	TOID(struct node) node;
};

struct node
{
	TOID(struct node) next;
	TOID(struct foo) foo;
};
```

The name of layout and the number of declared types can be retrieved using the following code:

```c
const char *layout_name = POBJ_LAYOUT_NAME(mylayout);
int num_of_types = POBJ_LAYOUT_TYPES_NUM(mylayout);
```


# OBJECT CONTAINERS #

All the objects in the persistent memory pool are assigned a *type number* and are accessible by it.

The **libpmemobj** provides a mechanism allowing to iterate through the internal object collection, either looking for a specific object, or performing a
specific operation on each object of given type. Software should not make any assumptions about the order of the objects in the internal object containers.

```c
PMEMoid pmemobj_first(PMEMobjpool *pop);
```

The **pmemobj_first**() function returns the first object from the pool. If the pool is empty, **OID_NULL** is returned.

```c
POBJ_FIRST(PMEMobjpool *pop, TYPE)
```

The **POBJ_FIRST**() macro returns the first object from the pool of the type specified by *TYPE*.

```c
POBJ_FIRST_TYPE_NUM(PMEMobjpool *pop, uint64_t type_num)
```

The **POBJ_FIRST_TYPE_NUM**() macro returns the first object from the pool of the type specified by *type_num*.

```c
PMEMoid pmemobj_next(PMEMoid oid);
```

The **pmemobj_next**() function returns the next object from the pool. If an object referenced by *oid* is the last object in the collection, or if the
*OID_NULL* is passed as an argument, function returns **OID_NULL**.

```c
POBJ_NEXT(TOID oid)
```

The **POBJ_NEXT**() macro returns the next object of the same type as the object referenced by *oid*.

```c
POBJ_NEXT_TYPE_NUM(PMEMoid oid)
```

The **POBJ_NEXT_TYPE_NUM**() macro returns the next object of the same type as the object referenced by *oid*.

The following four macros provide more convenient way to iterate through the internal collections, performing a specific operation on each object.

```c
POBJ_FOREACH(PMEMobjpool *pop, PMEMoid varoid)
```

The **POBJ_FOREACH**() macro allows to perform a specific operation on each allocated object stored in the persistent memory pool pointed by *pop*. It
traverses the internal collection of all the objects, assigning a handle to each element in turn to *varoid* variable.

```c
POBJ_FOREACH_TYPE(PMEMobjpool *pop, TOID var)
```

The **POBJ_FOREACH_TYPE**() macro allows to perform a specific operation on each allocated object of the same type as object passed as *var* argument, stored
in the persistent memory pool pointed by *pop*. It traverses the internal collection of all the objects of the specified type, assigning a handle to each
element in turn to *var* variable.

```c
POBJ_FOREACH_SAFE(PMEMobjpool *pop, PMEMoid varoid, PMEMoid nvaroid)
POBJ_FOREACH_SAFE_TYPE(PMEMobjpool *pop, TOID var, TOID nvar)
```

The macros **POBJ_FOREACH_SAFE**() and **POBJ_FOREACH_SAFE_TYPE**() work in a similar fashion as **POBJ_FOREACH**() and **POBJ_FOREACH_TYPE**() except that
prior to performing the operation on the object, they preserve a handle to the next object in the collection by assigning it to *nvaroid* or *nvar* variable.
This allows safe deletion of selected objects while iterating through the collection.


# ROOT OBJECT MANAGEMENT #

The root object of persistent memory pool is an entry point for all other persistent objects allocated using the **libpmemobj** API. In other words, every
single object stored in persistent memory pool should have the root object at the end of its reference path. It may be assumed that for each persistent memory
pool the root object always exists, and there is exactly one root object in each pool.

```c
PMEMoid pmemobj_root(PMEMobjpool *pop, size_t size);
```

The **pmemobj_root**() function returns a handle to the root object associated with the persistent memory pool pointed by *pop*. If this is the first call to
**pmemobj_root**() and the root object does not exists yet, it is implicitly allocated in a thread-safe manner, so if the function is called by more than one
thread simultaneously (with identical *size* value), the same root object handle is returned in all the threads.

The size of the root object is guaranteed to be not less than the requested *size*. If the requested size is larger than the current size, the root object is
automatically resized. In such case, the old data is preserved and the extra space is zeroed. The **pmemobj_root**() function shall not fail, except for the
case if the requested object size is larger than the maximum allocation size supported for given pool, or if there is not enough free space in the pool to
satisfy the reallocation of the root object. In such case, **OID_NULL** is returned.

```c
PMEMoid pmemobj_root_construct(PMEMobjpool *pop, size_t size,
	pmemobj_constr constructor, void *arg)
```

The **pmemobj_root_construct**() performs the same actions as the **pmemobj_root**() function, but instead of zeroing the newly allocated object a
*constructor* function is called. The constructor is also called on reallocations. If the constructor returns non-zero value the allocation is canceled, the
*OID_NULL* value is returned from the caller and *errno* is set to **ECANCELED**. The **pmemobj_root_size**() can be used in the constructor to check whether
it's the first call to the constructor.

```c
POBJ_ROOT(PMEMobjpool *pop, TYPE)
```

The **POBJ_ROOT**() macro works the same way as the **pmemobj_root**() function except it returns a typed *OID* of type *TYPE* instead of *PMEMoid*.

```c
size_t pmemobj_root_size(PMEMobjpool *pop);
```

The **pmemobj_root_size**() function returns the current size of the root object associated with the persistent memory pool pointed by *pop*. The returned size
is the largest value requested by any of the earlier **pmemobj_root**() calls. 0 is returned if the root object has not been allocated yet.


# NON-TRANSACTIONAL ATOMIC ALLOCATIONS #

Functions described in this section provide the mechanism to allocate, resize and free objects from the persistent memory pool in a thread-safe and fail-safe
manner. All the routines are atomic with respect to other threads and any power-fail interruptions. If any of those operations is torn by program failure or
system crash; on recovery they are guaranteed to be entirely completed or discarded, leaving the persistent memory heap and internal object containers in a
consistent state.

All these functions can be used outside transactions. Note that operations performed using non-transactional API are considered durable after completion, even
if executed within the open transaction. Such non-transactional changes will not be rolled-back if the transaction is aborted or interrupted.

The allocations are always aligned to the cache-line boundary.

```c
typedef int (*pmemobj_constr)(**PMEMobjpool *pop, void *ptr, void *arg);
```

The *pmemobj_constr* type represents a constructor for atomic allocation from persistent memory heap associated with memory pool *pop*. The *ptr* is a pointer
to allocating memory area and the *arg* is an user-defined argument passed to an appropriate function.

```c
int pmemobj_alloc(PMEMobjpool *pop, PMEMoid *oidp,
	size_t size, uint64_t type_num,
	pmemobj_constr constructor , void *arg);
```

The **pmemobj_alloc**() function allocates a new object from the persistent memory heap associated with memory pool *pop*. The *PMEMoid* of allocated object is
stored in *oidp*. If NULL is passed as *oidp*, then the newly allocated object may be accessed only by iterating objects in the object container associated
with given *type_num*, as described in **OBJECT CONTAINERS** section. If the *oidp* points to memory location from the **pmemobj** heap the *oidp* is modified
atomically. Before returning, it calls the *constructor* function passing the pool handle *pop*, the pointer to the newly allocated object in *ptr* along with
the *arg* argument. It is guaranteed that allocated object is either properly initialized, or if the allocation is interrupted before the constructor
completes, the memory space reserved for the object is reclaimed. If the constructor returns non-zero value the allocation is canceled, the -1 value is
returned from the caller and *errno* is set to **ECANCELED**. The *size* can be any non-zero value, however due to internal padding and object metadata, the
actual size of the allocation will differ from the requested one by at least 64 bytes. For this reason, making the allocations of a size less than 64 bytes is
extremely inefficient and discouraged. If *size* equals 0, then **pmemobj_alloc**() returns non-zero value, sets the *errno* and leaves the *oidp* untouched.
The allocated object is added to the internal container associated with given *type_num*.

```c
int pmemobj_zalloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num);
```

The **pmemobj_zalloc**() function allocates a new zeroed object from the the persistent memory heap associated with memory pool *pop*. The *PMEMoid* of
allocated object is stored in *oidp*. If NULL is passed as *oidp*, then the newly allocated object may be accessed only by iterating objects in the object
container associated with given *type_num*, as described in **OBJECT CONTAINERS** section. If the *oidp* points to memory location from the **pmemobj** heap
the *oidp* is modified atomically. The *size* can be any non-zero value, however due to internal padding and object metadata, the actual size of the allocation
will differ from the requested one by at least 64 bytes. For this reason, making the allocations of a size less than 64 bytes is extremely inefficient and
discouraged. If *size* equals 0, then **pmemobj_zalloc**() returns non-zero value, sets the *errno* and leaves the *oidp* untouched. The allocated object is
added to the internal container associated with given *type_num*.

```c
void pmemobj_free(PMEMoid *oidp);
```

The **pmemobj_free**() function provides the same semantics as **free**(3), but instead of the process heap supplied by the system, it operates on the
persistent memory heap. It frees the memory space represented by *oidp*, which must have been returned by a previous call to **pmemobj_alloc**(),
**pmemobj_zalloc**(), **pmemobj_realloc**(), or **pmemobj_zrealloc**(). If *oidp* is NULL or if it points to the root object's *OID*, behavior of the
function is undefined. If it points to **OID_NULL**, no operation is performed. It sets the *oidp* to **OID_NULL** value after freeing the memory. If the *oidp*
points to memory location from the **pmemobj** heap the *oidp* is changed atomically.

```c
int pmemobj_realloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num);
```

The **pmemobj_realloc**() function provide similar semantics to **realloc**(3), but operates on the persistent memory heap associated with memory pool *pop*.
It changes the size of the object represented by *oidp*, to *size* bytes. The resized object is also added or moved to the internal container associated with
given *type_num*. The contents will be unchanged in the range from the start of the region up to the minimum of the old and new sizes. If the new size is
larger than the old size, the added memory will *not* be initialized. If *oidp* is NULL or if it points to the root object's *OID*, behavior of the function
is undefined. If it points to *OID_NULL*, then the call is equivalent to *pmemobj_alloc(pop, size, type_num)*. If *size* is equal to zero, and *oidp* is not
**OID_NULL**, then the call is equivalent to *pmemobj_free(oid)*. Unless *oidp* is **OID_NULL**, it must have been returned by an earlier call to
**pmemobj_alloc**(), **pmemobj_zalloc**(), **pmemobj_realloc**(), or **pmemobj_zrealloc**(). Note that the object handle value may change in result of
reallocation. If the object was moved, a memory space represented by *oid* is reclaimed. If *oidp* points to memory location from the **pmemobj** heap the
*oidp* is changed atomically. If **pmemobj_realloc**() is unable to satisfy the allocation request, a non-zero value is returned and *errno* is set
appropriately.

```c
int pmemobj_zrealloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num);
```

The **pmemobj_zrealloc**() function provide similar semantics to **realloc**(3), but operates on the persistent memory heap associated with memory pool *pop*.
It changes the size of the object represented by *oid*, to *size* bytes. The resized object is also added or moved to the internal container associated with
given *type_num*. The contents will be unchanged in the range from the start of the region up to the minimum of the old and new sizes. If the new size is
larger than the old size, the added memory will be zeroed. If *oidp* is NULL or if it points to the root object's *OID*, behavior of the function is
undefined. If it points to **OID_NULL**, then the call is equivalent to *pmemobj_zalloc(pop, size, type_num)*. If *size* is equal to zero, and *oidp* doesn't
point to **OID_NULL**, then the call is equivalent to *pmemobj_free(pop, oid)*. Unless *oidp* points to **OID_NULL**, it must have been returned by an earlier call
to **pmemobj_alloc**(), **pmemobj_zalloc**(), **pmemobj_realloc**(), or **pmemobj_zrealloc**(). Note that the object handle value may change in result of
reallocation. If the object was moved, a memory space represented by *oidp* is reclaimed. If *oidp* points to memory location from the **pmemobj** heap the
*oidp* is changed atomically. If **pmemobj_zrealloc**() is unable to satisfy the allocation request, a non-zero value is returned and *errno* is set appropriately.

```c
int pmemobj_strdup(PMEMobjpool *pop, PMEMoid *oidp, const char *s, uint64_t type_num);
```

The **pmemobj_strdup**() function provides the same semantics as **strdup**(3), but operates on the persistent memory heap associated with memory pool *pop*.
It stores a handle to a new object in *oidp* which is a duplicate of the string *s*. If NULL is passed as *oidp*, then the newly allocated object may be
accessed only by iterating objects in the object container associated with given *type_num*, as described in **OBJECT CONTAINERS** section. If the *oidp*
points to memory location from the **pmemobj** heap the *oidp* is changed atomically. The allocated string object is also added to the internal container
associated with given *type_num*. Memory for the new string is obtained with **pmemobj_alloc**(), on the given memory pool, and can be freed with
**pmemobj_free**() on the same memory pool. If **pmemobj_strdup**() is unable to satisfy the allocation request, a non-zero value is returned and *errno* is set
appropriately.

```c
int pmemobj_wcsdup(PMEMobjpool *pop, PMEMoid *oidp, const wchar_t *s, uint64_t type_num);
```

The **pmemobj_wcsdup**() function provides the same semantics as **wcsdup**(3), but operates on the persistent memory heap associated with memory pool *pop*.
It stores a handle to a new object in *oidp* which is a duplicate of the wide character string *s*. If NULL is passed as *oidp*, then the newly allocated object may be
accessed only by iterating objects in the object container associated with given *type_num*, as described in **OBJECT CONTAINERS** section. If the *oidp*
points to memory location from the **pmemobj** heap the *oidp* is changed atomically. The allocated wide character string object is also added to the internal container
associated with given *type_num*. Memory for the new wide character string is obtained with **pmemobj_alloc**(), on the given memory pool, and can be freed with
**pmemobj_free**() on the same memory pool. If **pmemobj_wcsdup**() is unable to satisfy the allocation request, a non-zero value is returned and *errno* is set
appropriately.

```c
size_t pmemobj_alloc_usable_size(PMEMoid oid);
```

The **pmemobj_alloc_usable_size**() function provides the same semantics as **malloc_usable_size**(3), but instead of the process heap supplied by the system, it
operates on the persistent memory heap. It returns the number of usable bytes in the object represented by *oid*, a handle to an object allocated by
**pmemobj_alloc**() or a related function. If *oid* is **OID_NULL**, 0 is returned.

```c
POBJ_NEW(PMEMobjpool *pop, TOID *oidp, TYPE, pmemobj_constr constructor, void *arg)
```

The **POBJ_NEW**() macro is a wrapper around the **pmemobj_alloc**() function which takes the type name *TYPE* and passes the size and type number to the
**pmemobj_alloc**() function from the typed *OID*. Instead of taking a pointer to *PMEMoid* it takes a pointer to typed *OID* of *TYPE*.

```c
POBJ_ALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size,
	pmemobj_constr constructor , void *arg)
```

The **POBJ_ALLOC**() macro is a wrapper around the **pmemobj_alloc**() function which takes the type name *TYPE* the size of allocation *size* and passes the type
number to the **pmemobj_alloc**() function from the typed *OID*. Instead of taking a pointer to *PMEMoid* it takes a pointer to typed *OID* of *TYPE*.

```c
POBJ_ZNEW(PMEMobjpool *pop, TOID *oidp, TYPE)
```

The **POBJ_ZNEW**() macro is a wrapper around the **pmemobj_zalloc**() function which takes the type name *TYPE* and passes the size and type number to the
**pmemobj_zalloc**() function from the typed *OID*. Instead of taking a pointer to *PMEMoid* it takes a pointer to typed *OID* of *TYPE*.

```c
POBJ_ZALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
```

The **POBJ_ZALLOC**() macro is a wrapper around the **pmemobj_zalloc**() function which takes the type name *TYPE*, the size of allocation *size* and passes the
type number to the **pmemobj_zalloc**() function from the typed *OID*. Instead of taking a pointer to *PMEMoid* it takes a pointer to typed *OID* of *TYPE*.

```c
POBJ_REALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
```

The **POBJ_REALLOC**() macro is a wrapper around the **pmemobj_realloc**() function which takes the type name *TYPE* and passes the type number to the
**pmemobj_realloc**() function from the typed *OID*. Instead of taking a pointer to *PMEMoid* it takes a pointer to typed *OID* of *TYPE*.

```c
POBJ_ZREALLOC(PMEMobjpool *pop, TOID *oidp, TYPE, size_t size)
```

The **POBJ_ZREALLOC**() macro is a wrapper around the **pmemobj_zrealloc**() function which takes the type name *TYPE* and passes the type number to the
**pmemobj_zrealloc**() function from the typed *OID*. Instead of taking a pointer to *PMEMoid* it takes a pointer to typed *OID* of *TYPE*.

```c
POBJ_FREE(TOID *oidp)
```

The **POBJ_FREE**() macro is a wrapper around the **pmemobj_free**() function which takes pointer to typed *OID* as *oidp* argument instead of *PMEMoid*.


# NON-TRANSACTIONAL PERSISTENT ATOMIC LISTS #

Besides the internal objects collections described in section **OBJECT CONTAINERS** the **libpmemobj** provides a mechanism to organize persistent objects in
the user-defined persistent atomic circular doubly linked lists. All the routines and macros operating on the persistent lists provide atomicity with respect
to any power-fail interruptions. If any of those operations is torn by program failure or system crash; on recovery they are guaranteed to be entirely
completed or discarded, leaving the lists, persistent memory heap and internal object containers in a consistent state.

The persistent atomic circular doubly linked lists support the following functionality:

+ Insertion of an object at the head of the list, or at the end of the list.
+ Insertion of an object before or after any element in the list.
+ Atomic allocation and insertion of a new object at the head of the list, or at the end of the list.
+ Atomic allocation and insertion of a new object before or after any element in the list.
+ Atomic moving of an element from one list to the specific location on another list.
+ Removal of any object in the list.
+ Atomic removal and freeing of any object in the list.
+ Forward or backward traversal through the list.

A list is headed by a *list_head* structure containing a single object handle of the first element on the list. The elements are doubly linked so that an
arbitrary element can be removed without a need to traverse the list. New elements can be added to the list before or after an existing element, at the head of
the list, or at the end of the list. A list may be traversed in either direction.

The user-defined structure of each element must contain a field of type *list_entry* holding the object handles to the previous and next element on the list.
Both the *list_head* and the *list_entry* structures are declared in **\<libpmemobj.h\>**.

The functions below are intended to be used outside transactions - transactional variants are described in section **TRANSACTIONAL OBJECT MANIPULATION**. Note
that operations performed using this non-transactional API are independent from their transactional counterparts. If any non-transactional allocations or list
manipulations are performed within an open transaction, the changes will not be rolled-back if such a transaction is aborted or interrupted.

```c
int pmemobj_list_insert(PMEMobjpool *pop, size_t pe_offset, void *head,
	PMEMoid dest, int before, PMEMoid oid);
```

The **pmemobj_list_insert**() function inserts an element represented by object handle *oid* into the list referenced by *head*. Depending on the value of flag
*before*, the object is added before or after the element *dest*. If *dest* value is **OID_NULL**, the object is inserted at the head or at the end of the list,
depending on the *before* flag value. If value is non-zero the object is inserted at the head, if value is zero the object is inserted at the end of the list.
The relevant values are available through **POBJ_LIST_DEST_HEAD** and **POBJ_LIST_DEST_TAIL** defines respectively. The argument *pe_offset* declares an offset of
the structure that connects the elements in the list. All the handles *head*, *dest* and *oid* must point to the objects allocated from the same memory pool
*pop*. The *head* and *oid* cannot be **OID_NULL**. On success, zero is returned. On error, -1 is returned and *errno* is set.

```c
PMEMoid pmemobj_list_insert_new(PMEMobjpool *pop, size_t pe_offset,
	void *head, PMEMoid dest, int before, size_t size,
	uint64_t type_num, pmemobj_constr constructor, void arg);
```

The **pmemobj_list_insert_new**() function atomically allocates a new object of given *size* and type *type_num* and inserts it into the list referenced by
*head*. Depending on the value of *before* flag, the newly allocated object is added before or after the element *dest*. If *dest* value is **OID_NULL**, the
object is inserted at the head or at the end of the list, depending on the *before* flag value. If value is non-zero the object is inserted at the head, if
value is zero the object is inserted at the end of the list. The relevant values are available through **POBJ_LIST_DEST_HEAD** and **POBJ_LIST_DEST_TAIL** defines
respectively. The argument *pe_offset* declares an offset of the structure that connects the elements in the list. All the handles *head*, *dest* must point to
the objects allocated from the same memory pool *pop*. Before returning, it calls the *constructor* function passing the pool handle *pop*, the pointer to the
newly allocated object in *ptr* along with the *arg* argument. It is guaranteed that allocated object is either properly initialized or, if the allocation is
interrupted before the constructor completes, the memory space reserved for the object is reclaimed. If the constructor returns non-zero value the allocation
is canceled, the -1 value is returned from the caller and *errno* is set to **ECANCELED**. The *head* cannot be **OID_NULL**. The allocated object is also added to
the internal container associated with given *type_num*. as described in section **OBJECT CONTAINERS**. On success, it returns a handle to the newly allocated
object. On error, **OID_NULL** is returned and *errno* is set.

```c
int pmemobj_list_remove(PMEMobjpool *pop, size_t pe_offset,
	void *head, PMEMoid oid, int free);
```

The **pmemobj_list_remove**() function removes the object referenced by *oid* from the list pointed by *head*. If *free* flag is set, it also removes the
object from the internal object container and frees the associated memory space. The argument *pe_offset* declares an offset of the structure that connects the
elements in the list. Both *head* and *oid* must point to the objects allocated from the same memory pool *pop* and cannot be **OID_NULL**. On success, zero is
returned. On error, -1 is returned and *errno* is set.

```c
int pmemobj_list_move(PMEMobjpool *pop,
	size_t pe_old_offset, void *head_old,
	size_t pe_new_offset, void *head_new,
	PMEMoid dest, int before, PMEMoid oid);
```

The **pmemobj_list_move**() function moves the object represented by *oid* from the list pointed by *head_old* to the list pointed by *head_new*. Depending on
the value of flag *before*, the newly allocated object is added before or after the element *dest*. If *dest* value is **OID_NULL**, the object is inserted at
the head or at the end of the second list, depending on the *before* flag value. If value is non-zero the object is inserted at the head, if value is zero the
object is inserted at the end of the list. The relevant values are available through **POBJ_LIST_DEST_HEAD** and **POBJ_LIST_DEST_TAIL** defines respectively. The
arguments *pe_old_offset* and *pe_new_offset* declare the offsets of the structures that connects the elements in the old and new lists respectively. All the
handles *head_old*, *head_new*, *dest* and *oid* must point to the objects allocated from the same memory pool *pop*. *head_old*, *head_new* and *oid* cannot
be **OID_NULL**. On success, zero is returned. On error, -1 is returned and *errno* is set.


# TYPE-SAFE NON-TRANSACTIONAL PERSISTENT ATOMIC LISTS #

The following macros define and operate on a type-safe persistent atomic circular doubly linked list data structure that consist of a set of persistent objects
of a well-known type. Unlike the functions described in the previous section, these macros provide type enforcement by requiring declaration of type of the
objects stored in given list, and not allowing mixing objects of different types in a single list.

The functionality and semantics of those macros is similar to circular queues defined in **queue**(3).

The majority of the macros must specify the handle to the memory pool *pop* and the name of the *field* in the user-defined structure, which must be of type
*POBJ_LIST_ENTRY* and is used to connect the elements in the list.

A list is headed by a structure defined by the **POBJ_LIST_HEAD**() macro. This structure contains an object handle of the first element on the list. The elements
are doubly linked so that an arbitrary element can be removed without a need to traverse the list. New elements can be added to the list before or after an
existing element, at the head of the list, or at the end of the list. A list may be traversed in either direction. A *POBJ_LIST_HEAD* structure is declared as
follows:

```c
#define POBJ_LIST_HEAD(HEADNAME, TYPE)
struct HEADNAME
{
	TOID(TYPE) pe_first;
	PMEMmutex lock;
};
```

In the macro definitions, *TYPE* is the name of a user-defined structure, that must contain a field of type *POBJ_LIST_ENTRY*. The argument *HEADNAME* is the
name of a user-defined structure that must be declared using the macro *POBJ_LIST_HEAD*. See the examples below for further explanation of how these macros are
used.

The macro *POBJ_LIST_ENTRY* declares a structure that connects the elements in the list.

```c
#define POBJ_LIST_ENTRY(TYPE)
struct
{
	TOID(TYPE) pe_next;
	TOID(TYPE) pe_prev;
};
```


```c
POBJ_LIST_FIRST(POBJ_LIST_HEAD *head)
```

The macro **POBJ_LIST_FIRST**() returns the first element on the list referenced by *head*. If the list is empty **OID_NULL** is returned.

```c
POBJ_LIST_LAST(POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_LAST**() returns the last element on the list referenced by *head*. If the list is empty **OID_NULL** is returned.

```c
POBJ_LIST_EMPTY(POBJ_LIST_HEAD *head)
```

The macro **POBJ_LIST_EMPTY**() evaluates to 1 if the list referenced by *head* is empty. Otherwise, 0 is returned.

```c
POBJ_LIST_NEXT(TOID elm, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_NEXT**() returns the element next to the element *elm*.

```c
POBJ_LIST_PREV(TOID elm, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_PREV**() returns the element preceding the element *elm*.

```c
POBJ_LIST_FOREACH(TOID var, POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_FOREACH**() traverses the list referenced by *head* assigning a handle to each element in turn to *var* variable.

```c
POBJ_LIST_FOREACH_REVERSE(TOID var, POBJ_LIST_HEAD *head, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_FOREACH_REVERSE**() traverses the list referenced by *head* in reverse order, assigning a handle to each element in turn to *var* variable.
The *field* argument is the name of the field of type *POBJ_LIST_ENTRY* in the element structure.

```c
POBJ_LIST_INSERT_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_INSERT_HEAD**() inserts the element *elm* at the head of the list referenced by *head*.

```c
POBJ_LIST_INSERT_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_INSERT_TAIL**() inserts the element *elm* at the end of the list referenced by *head*.

```c
POBJ_LIST_INSERT_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, TOID elm, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_INSERT_AFTER**() inserts the element *elm* into the list referenced by *head* after the element *listelm*. If *listelm* value is **TOID_NULL**,
the object is inserted at the end of the list.

```c
POBJ_LIST_INSERT_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, TOID elm, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_INSERT_BEFORE**() inserts the element *elm* into the list referenced by *head* before the element *listelm*. If *listelm* value is
**TOID_NULL**, the object is inserted at the head of the list.

```c
POBJ_LIST_INSERT_NEW_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
```

The macro **POBJ_LIST_INSERT_NEW_HEAD**() atomically allocates a new object of size *size* and inserts it at the head of the list referenced by *head*. The newly
allocated object is also added to the internal object container associated with a type number which is retrieved from the typed *OID* of the first element on
list.

```c
POBJ_LIST_INSERT_NEW_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
```

The macro **POBJ_LIST_INSERT_NEW_TAIL**() atomically allocates a new object of size *size* and inserts it at the tail of the list referenced by *head*. The newly
allocated object is also added to the internal object container associated with with a type number which is retrieved from the typed *OID* of the first element
on list.

```c
POBJ_LIST_INSERT_NEW_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
```

The macro **POBJ_LIST_INSERT_NEW_AFTER**() atomically allocates a new object of size *size* and inserts it into the list referenced by *head* after the element
*listelm*. If *listelm* value is **TOID_NULL**, the object is inserted at the end of the list. The newly allocated object is also added to the internal object
container associated with with a type number which is retrieved from the typed *OID* of the first element on list.

```c
POBJ_LIST_INSERT_NEW_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID listelm, POBJ_LIST_ENTRY FIELD, size_t size,
	pmemobj_constr constructor, void *arg)
```

The macro **POBJ_LIST_INSERT_NEW_BEFORE**() atomically allocates a new object of size *size* and inserts it into the list referenced by *head* before the element
*listelm*. If *listelm* value is **TOID_NULL**, the object is inserted at the head of the list. The newly allocated object is also added to the internal object
container associated with with a type number which is retrieved from the typed *OID* of the first element on list.

```c
POBJ_LIST_REMOVE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_REMOVE**() removes the element *elm* from the list referenced by *head*.

```c
POBJ_LIST_REMOVE_FREE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	TOID elm, POBJ_LIST_ENTRY FIELD)
```

The macro **POBJ_LIST_REMOVE_FREE**() removes the element *elm* from the list referenced by *head* and frees the memory space represented by this element.

```c
POBJ_LIST_MOVE_ELEMENT_HEAD(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID elm, POBJ_LIST_ENTRY FIELD,
	POBJ_LIST_ENTRY field_new)
```

The macro **POBJ_LIST_MOVE_ELEMENT_HEAD**() moves the element *elm* from the list referenced by *head* to the head of the list *head_new*. The *field* and
*field_new* arguments are the names of the fields of type *POBJ_LIST_ENTRY* in the element structure that are used to connect the elements in both lists.

```c
POBJ_LIST_MOVE_ELEMENT_TAIL(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID elm, POBJ_LIST_ENTRY FIELD,
	POBJ_LIST_ENTRY field_new)
```

The macro **POBJ_LIST_MOVE_ELEMENT_TAIL**() moves the element *elm* from the list referenced by *head* to the end of the list *head_new*. The *field* and
*field_new* arguments are the names of the fields of type *POBJ_LIST_ENTRY* in the element structure that are used to connect the elements in both lists.

```c
POBJ_LIST_MOVE_ELEMENT_AFTER(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID listelm, TOID elm,
	POBJ_LIST_ENTRY FIELD, POBJ_LIST_ENTRY field_new)
```

The macro **POBJ_LIST_MOVE_ELEMENT_AFTER**() atomically removes the element *elm* from the list referenced by *head* and inserts it into the list referenced by
*head_new* after the element *listelm*. If *listelm* value is *TOID_NULL*, the object is inserted at the end of the list. The *field* and *field_new* arguments
are the names of the fields of type *POBJ_LIST_ENTRY* in the element structure that are used to connect the elements in both lists.

```c
POBJ_LIST_MOVE_ELEMENT_BEFORE(PMEMobjpool *pop, POBJ_LIST_HEAD *head,
	POBJ_LIST_HEAD *head_new, TOID listelm, TOID elm,
	POBJ_LIST_ENTRY FIELD, POBJ_LIST_ENTRY field_new)
```

The macro **POBJ_LIST_MOVE_ELEMENT_BEFORE**() atomically removes the element *elm* from the list referenced by *head* and inserts it into the list referenced by
*head_new* before the element *listelm*. If *listelm* value is **TOID_NULL**, the object is inserted at the head of the list. The *field* and *field_new*
arguments are the names of the fields of type *POBJ_LIST_ENTRY* in the element structure that are used to connect the elements in both lists.


# TRANSACTIONAL OBJECT MANIPULATION #

The functions described in sections **NON-TRANSACTIONAL ATOMIC ALLOCATIONS** and **NON-TRANSACTIONAL PERSISTENT ATOMIC LISTS** only guarantee the atomicity in
scope of a single operation on an object. In case of more complex changes, involving multiple operations on an object, or allocation and modification of
multiple objects; data consistency and fail-safety may be provided only by using *atomic transactions*.

A transaction is defined as series of operations on persistent memory objects that either all occur, or nothing occurs. In particular, if the execution of a
transaction is interrupted by a power failure or a system crash, it is guaranteed that after system restart, all the changes made as a part of the uncompleted
transaction will be rolled-back, restoring the consistent state of the memory pool from the moment when the transaction was started.

Note that transactions do not provide the atomicity with respect to other threads. All the modifications performed within the transactions are immediately
visible to other threads, and it is the responsibility of a programer to implement a proper thread synchronization mechanism.

Each transaction is visible only to the thread that started it. No other threads can add operations, commit or abort the transaction initiated by another thread. There may be multiple open transactions from multiple threads on a single memory pool at the same time.

Please see the **CAVEATS** section for known limitations of the transactional API.

```c
enum tx_stage pmemobj_tx_stage(void);
```

The **pmemobj_tx_stage**() function returns the stage of the current transaction in the thread. Stages are changed only by the **pmemobj_tx_\***() functions.
The transaction stages are defined as follows:

+ **TX_STAGE_NONE** - no open transaction in this thread
+ **TX_STAGE_WORK** - transaction in progress
+ **TX_STAGE_ONCOMMIT** - successfully committed
+ **TX_STAGE_ONABORT** - starting the transaction failed or transaction aborted
+ **TX_STAGE_FINALLY** - ready for clean up

```c
int pmemobj_tx_begin(PMEMobjpool *pop, jmp_buf *env, ...);
```

The **pmemobj_tx_begin**() function starts a new transaction in the current thread. The caller may use *env* argument to provide a pointer to the information of a calling environment to be restored in case of transaction abort. This information must be filled by a caller, using **setjmp**(3) macro.


Optionally, a list of parameters for the transaction may be provided as the following arguments. Each parameter consists of a type and type-specific number
of values. Currently there are 4 types:

+ **TX_PARAM_NONE**, used as a termination marker (no following value)
+ **TX_PARAM_MUTEX**, followed by one pmem-resident PMEMmutex
+ **TX_PARAM_RWLOCK**, followed by one pmem-resident PMEMrwlock
+ (EXPERIMENTAL) **TX_PARAM_CB**, followed by a callback function of type pmemobj_tx_callback and a void pointer (so 2 values)

Using **TX_PARAM_MUTEX** or **TX_PARAM_RWLOCK** means that at the beginning of a transaction specified lock will be acquired. In case of **TX_PARAM_RWLOCK**
it's a write lock. It is guaranteed that **pmemobj_tx_begin**() will grab all locks prior to successful completion and they will be held by the current thread
until the outermost transaction is finished. Locks are taken in the order from left to right. To avoid deadlocks, user must take care of the proper order of locks.

**TX_PARAM_CB** registers a specified callback function to be executed at each transaction stage. For **TX_STAGE_WORK** it's executed before commit, for all other
stages as a first operation after stage change. It will also be called after each transaction - in such case *stage* parameter will be set to **TX_STAGE_NONE**.
pmemobj_tx_callback must be compatible with:

```void func(PMEMobjpool *pop, enum pobj_tx_stage stage, void *arg)```

*pop* is a pool identifier used in **pmemobj_tx_begin**(), *stage* is a current transaction stage and *arg* is a second parameter of **TX_PARAM_CB**.
This mechanism can be deemed as an alternative method for executing code between stages (instead of **TX_ONCOMMIT**,
**TX_ONABORT**, etc).

Note that **TX_PARAM_CB** does not replace **TX_ONCOMMIT**/**TX_ONABORT**/etc. macros. They can be used together - a callback will be executed *before* **TX_ONCOMMIT**/**TX_ONABORT**/etc. section.

**pmemobj_tx_begin**() can also be called within an open transaction on the same memory pool if the current stage of the open transaction is in the **TX_STAGE_NONE** or **TX_STAGE_WORK**. In that case the call does not start any new transaction.
Instead, the statements of the inner transaction become statements of proper stages of the outer transaction, committing the inner transaction does not commit the outer transaction, and errors in the inner transaction are propagated up to the outermost level resulting in the interruption of the entire transaction.
If the call succeed, the transaction stage changes to **TX_STAGE_WORK** and the function returns zero. Otherwise, thes stage changes to **TX_STAGE_ONABORT** and an error number is returned.
Calling **pmemobj_tx_begin**() when the open transaction is in a stage other than **TX_STAGE_NONE** and **TX_STAGE_WORK** fails and aborts the whole transaction.

There are two significant differences between adding code to the **TX_ONCOMMIT**/**TX_ONABORT**/etc. sections of the inner transaction and adding stage callbacks to the inner transaction:

+  Registered callback functions are executed only in the most outer transaction (even if registered in the inner one).

+  There can be only one callback in the whole transaction (it can't be changed in the inner transaction).

**TX_PARAM_CB** can be used when the code dealing with transaction stage changes is shared between multiple users or when it must be executed only in the outer transaction. For example it can be very useful when application must synchronize persistent and transient state.

To use a transaction, a *transaction context* must be present in the current thread. If none exists, a default one is automatically created.
The transaction context is reused within a thread until either a) the application closes or the thread finishes, or b) it is explicitly changed using **pmemobj_tx_ctx_set**().

```c
int pmemobj_tx_ctx_new();
int pmemobj_tx_ctx_delete(struct pobj_tx_ctx *ctx);
int pmemobj_tx_ctx_set(struct pobj_tx_ctx *new_ctx, struct pobj_tx_ctx **old_ctx);
```

**pmemobj_tx_ctx_new**() creates a new transaction context.

**pmemobj_tx_ctx_delete**() frees the given transaction context.

**pmemobj_tx_ctx_set**() sets the current transaction context to *new_ctx*, storing the previous context under the *old_ctx* pointer unless the latter is NULL.
The *new_ctx* argument cannot be NULL.

When **pmemobj_tx_begin**() is called within an open transaction on the same pool (as described earlier) the transaction context is reused.
If the user creates a new transaction context using **pmemobj_tx_ctx_new**() and changes the current context to it using **pmemobj_tx_ctx_set**(), a subsequent call to **pmemobj_tx_begin**() on any memory pool starts a new transaction which is independent from transactions started with the previous context.
The transaction started this way is a full-fledged transaction, even if started within an already open transaction on the same or a different memory pool, regardless of the stage of the outer transaction.
This allows a user to manage more than one active transactions at a time in a thread.

When application closes or a thread finishes, the current transaction context is automatically freed. Contexts created by the user and changed explicitly, have to be freed by the user.

Here is an example of nesting transactions on different pools:

```c
TX_BEGIN(popA) {
	... /* do sth on the pool A */
	struct pobj_tx_ctx *ctxB = pmemobj_tx_ctx_new();
	if (ctxB == NULL) {
		/* handle error */
	}
	struct pobj_tx_ctx *old_ctx;
	if (pmemobj_tx_ctx_set(ctxB, &old_ctx)) { /* change the transaction context to a new one */
		/* handle error */
	}
	TX_BEGIN(popB) { /* start a nested transaction on the pool B with a new context */
		/* do sth on the pool B */
	} TX_END
	if (pmemobj_tx_ctx_set(old_ctx, &ctxB)) { /* restore the previous transaction context */
		/* handle error */
	}
	if (pmemobj_tx_ctx_delete(ctxB)) { /* free the not needed context */
		/* handle error */
	}
	... /* do sth else on the pool A */
} TX_ONABORT {
	...
	struct pobj_tx_ctx *ctxC = pmemobj_tx_ctx_new();
	if (ctxC == NULL) {
		/* handle error */
	}
	struct pobj_tx_ctx *old_ctx;
	if (pmemobj_tx_ctx_set(ctxC, &old_ctx)) {
		/* handle error */
	}
	TX_BEGIN(popC) {
		/* do sth on the pool C */
	} TX_END
	if (pmemobj_tx_ctx_set(old_ctx, &ctxC)) {
		/* handle error */
	}
	if (pmemobj_tx_ctx_delete(ctxC)) { /* free the not needed context */
		/* handle error */
	}
	...
} TX_END
```

```c
int pmemobj_tx_lock(enum tx_lock lock_type, void *lockp);
```

The **pmemobj_tx_lock**() function grabs a lock pointed by *lockp* and adds it to the current transaction. The lock type is specified by *lock_type*
(**TX_LOCK_MUTEX** or **TX_LOCK_RWLOCK**) and the pointer to the *lockp* of *PMEMmutex* or *PMEMrwlock* type. If successful, *lockp* is added to transaction,
locked and function returns zero. Otherwise, stage changes to **TX_STAGE_ONABORT** and an error number is returned. In case of *PMEMrwlock* *lock_type* function
acquires a write lock. This function must be called during **TX_STAGE_WORK**.

```c
void pmemobj_tx_abort(int errnum);
```

The **pmemobj_tx_abort**() aborts the current transaction and causes transition to **TX_STAGE_ONABORT**. This function must be called during **TX_STAGE_WORK**. If
the passed *errnum* is equal to zero, it shall be set to **ECANCELED**.

```c
void pmemobj_tx_commit(void);
```

The **pmemobj_tx_commit**() function commits the current open transaction and causes transition to **TX_STAGE_ONCOMMIT** stage. If called in context of the
outermost transaction, all the changes may be considered as durably written upon successful completion. This function must be called during **TX_STAGE_WORK**.

```c
int pmemobj_tx_end(void);
```

The **pmemobj_tx_end**() function performs a clean up of a current transaction. If called in context of the outermost transaction, it releases all the locks
acquired by **pmemobj_tx_begin**() for outer and nested transactions. The **pmemobj_tx_end**() function can be called during **TX_STAGE_NONE** if transitioned
to this stage using **pmemobj_tx_process**(). If not already in **TX_STAGE_NONE** state, it causes the transition to **TX_STAGE_NONE**.
In case of the nested transaction, it returns to the context of the outer transaction with **TX_STAGE_WORK** stage without releasing any locks. Must always be
called for each **pmemobj_tx_begin**(), even if starting the transaction failed. This function must *not* be called during **TX_STAGE_WORK**. If transaction
was successful, returns 0. Otherwise returns error code set by **pmemobj_tx_abort**(). Note that **pmemobj_tx_abort**() can be called internally by the library.

```c
int pmemobj_tx_errno(void);
```

The **pmemobj_tx_errno**() function returns the error code of the last transaction.

```c
void pmemobj_tx_process(void);
```

The **pmemobj_tx_process**() function performs the actions associated with current stage of the transaction, and makes the transition to the next stage. It
must be called in transaction. Current stage must always be obtained by a call to **pmemobj_tx_stage**(). The **pmemobj_tx_process**() performs the following
transitions in the transaction stage flow:

+ **TX_STAGE_WORK** -> **TX_STAGE_ONCOMMIT**
+ **TX_STAGE_ONABORT** -> **TX_STAGE_FINALLY**
+ **TX_STAGE_ONCOMMIT** -> **TX_STAGE_FINALLY**
+ **TX_STAGE_FINALLY** -> **TX_STAGE_NONE**
+ **TX_STAGE_NONE** -> **TX_STAGE_NONE**

The **pmemobj_tx_process**() must not be called after calling **pmemobj_tx_end**() for the outermost transaction.

```c
int pmemobj_tx_add_range(PMEMoid oid, uint64_t off, size_t size);
```

The **pmemobj_tx_add_range**() takes a "snapshot" of the memory block of given *size*, located at given offset *off* in the object specified by *oid* and saves
it to the undo log. The application is then free to directly modify the object in that memory range. In case of a failure or abort, all the changes within this
range will be rolled-back. The supplied block of memory has to be within the pool registered in the transaction. If successful, returns zero. Otherwise, state
changes to **TX_STAGE_ONABORT** and an error number is returned. This function must be called during **TX_STAGE_WORK**.

```c
int pmemobj_tx_xadd_range(PMEMoid oid, uint64_t off, size_t size, uint64_t flags);
```

The **pmemobj_tx_xadd_range**() function behaves exactly the same as **pmemobj_tx_add_range**() when *flags* equals zero.
*flags* is a bitmask of the following values:

+ **POBJ_XADD_NO_FLUSH** - skip flush on commit (when application deals with flushing or uses pmemobj_memcpy_persist)

```c
int pmemobj_tx_add_range_direct(const void *ptr, size_t size);
```

The **pmemobj_tx_add_range_direct**() behaves the same as **pmemobj_tx_add_range**() with the exception that it operates on virtual memory addresses and not
persistent memory objects. It takes a "snapshot" of a persistent memory block of given *size*, located at the given address *ptr* in the virtual memory
space and saves it to the undo log. The application is then free to directly modify the object in that memory range. In case of a failure or abort, all the
changes within this range will be rolled-back. The supplied block of memory has to be within the pool registered in the transaction. If successful, returns
zero. Otherwise, state changes to **TX_STAGE_ONABORT** and an error number is returned. This function must be called during **TX_STAGE_WORK**.

```c
int pmemobj_tx_xadd_range_direct(const void *ptr, size_t size);
```

The **pmemobj_tx_xadd_range_direct**() function behaves exactly the same as **pmemobj_tx_add_range_direct**() when *flags* equals zero.
*flags* is a bitmask of the following values:

+ **POBJ_XADD_NO_FLUSH** - skip flush on commit (when application deals with flushing or uses pmemobj_memcpy_persist)

```c
PMEMoid pmemobj_tx_alloc(size_t size, uint64_t type_num);
```

The **pmemobj_tx_alloc**() transactionally allocates a new object of given *size* and *type_num*. In contrast to the non-transactional allocations, the objects
are added to the internal object containers of given *type_num* only after the transaction is committed, making the objects visible to the **POBJ_FOREACH_\***()
macros. If successful, returns a handle to the newly allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is
set appropriately. If *size* equals 0, **OID_NULL** is returned and *errno* is set appropriately. This function must be called during **TX_STAGE_WORK**.

```c
PMEMoid pmemobj_tx_zalloc(size_t size, uint64_t type_num);
```

The **pmemobj_tx_zalloc**() function transactionally allocates new zeroed object of given *size* and *type_num*. If successful, returns a handle to the newly allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately. If *size* equals 0, **OID_NULL** is returned and *errno* is set appropriately. This function must be called during **TX_STAGE_WORK**.

```c
PMEMoid pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags);
```

The **pmemobj_tx_xalloc**() function transactionally allocates a new object of given *size* and *type_num*. The *flags* argument is a bitmask of the following values:

+ **POBJ_XALLOC_ZERO** - zero the object (equivalent of pmemobj_tx_zalloc)
+ **POBJ_XALLOC_NO_FLUSH** - skip flush on commit (when application deals with flushing or uses pmemobj_memcpy_persist)

If successful, returns a handle to the newly allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately. If *size* equals 0, **OID_NULL** is returned and *errno* is set appropriately. This function must be called during **TX_STAGE_WORK**.

```c
PMEMoid pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num);
```

The **pmemobj_tx_realloc**() function transactionally resizes an existing object to the given *size* and changes its type to *type_num*. If *oid* is
**OID_NULL**, then the call is equivalent to *pmemobj_tx_alloc(pop, size, type_num)*. If *size* is equal to zero and *oid* is not **OID_NULL**, then the call is
equivalent to *pmemobj_tx_free(oid)*. If the new size is larger than the old size, the added memory will *not* be initialized. If successful, returns returns a
handle to the resized object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately. Note that the object
handle value may change in result of reallocation. This function must be called during **TX_STAGE_WORK**.

```c
PMEMoid pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num);
```

The **pmemobj_tx_zrealloc**() function transactionally resizes an existing object to the given *size* and changes its type to *type_num*. If the new size is
larger than the old size, the extended new space is zeroed. If successful, returns returns a handle to the resized object. Otherwise, stage changes to
**TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately. Note that the object handle value may change in result of reallocation. This
function must be called during **TX_STAGE_WORK**.

```c
PMEMoid pmemobj_tx_strdup(const char *s, uint64_t type_num);
```

The **pmemobj_tx_strdup**() function transactionally allocates a new object containing a duplicate of the string *s* and assigns it a type *type_num*. If
successful, returns a handle to the newly allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set
appropriately. This function must be called during **TX_STAGE_WORK**.

```c
PMEMoid pmemobj_tx_wcsdup(const wchar_t *s, uint64_t type_num);
```

The **pmemobj_tx_wcsdup**() function transactionally allocates a new object containing a duplicate of the wide character string *s* and assigns it a type *type_num*. If
successful, returns a handle to the newly allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set
appropriately. This function must be called during **TX_STAGE_WORK**.

```c
int pmemobj_tx_free(PMEMoid oid);
```

The **pmemobj_tx_free**() function transactionally frees an existing object referenced by *oid*. If successful, returns zero. Otherwise, stage changes to
**TX_STAGE_ONABORT** and an error number is returned. This function must be called during **TX_STAGE_WORK**.

In addition to the above API, the **libpmemobj** offers a more intuitive method of building transactions using a set of macros described below. When using
macros, the complete transaction flow looks like this:

```c
TX_BEGIN(Pop) {
	/* the actual transaction code goes here... */
} TX_ONCOMMIT {
	/*
	 * optional - executed only if the above block
	 * successfully completes
	 */
} TX_ONABORT {
	/*
	 * optional - executed only if starting the transaction fails,
	 * or if transaction is aborted by an error or a call to
	 * pmemobj_tx_abort()
	 */
} TX_FINALLY {
	/*
	 * optional - if exists, it is executed after
	 * TX_ONCOMMIT or TX_ONABORT block
	 */
} TX_END /* mandatory */
```

```c
TX_BEGIN_PARAM(PMEMobjpool *pop, ...)
TX_BEGIN_CB(PMEMobjpool *pop, cb, arg, ...) (EXPERIMENTAL)
TX_BEGIN(PMEMobjpool *pop)
```

The **TX_BEGIN_PARAM**(), **TX_BEGIN_CB**() and **TX_BEGIN**() macros start a new transaction in the same way as **pmemobj_tx_begin**(), except that instead
of the environment buffer provided by a caller, they set up the local *jmp_buf* buffer and use it to catch the transaction abort. The **TX_BEGIN**() macro
starts a transaction without any options. **TX_BEGIN_PARAM** may be used in case when there is a need to grab locks prior to starting a transaction (like
for a multi-threaded program) or set up transaction stage callback. **TX_BEGIN_CB** is just a wrapper around **TX_BEGIN_PARAM** that validates callback
signature. (For compatibility there is also **TX_BEGIN_LOCK** macro which is an alias for **TX_BEGIN_PARAM**). Each of those macros shall be followed by
a block of code with all the operations that are to be performed atomically.

```c
TX_ONABORT
```

The **TX_ONABORT** macro starts a block of code that will be executed only if starting the transaction fails due to an error in **pmemobj_tx_begin**(), or if the
transaction is aborted. This block is optional, but in practice it should not be omitted. If it's desirable to crash the application when transaction aborts
and there is no **TX_ONABORT** section, application can define **POBJ_TX_CRASH_ON_NO_ONABORT** macro before inclusion of **\<libpmemobj.h\>**. It provides default
**TX_ONABORT** section which just calls **abort**(3).

```c
TX_ONCOMMIT
```

The **TX_ONCOMMIT** macro starts a block of code that will be executed only if the transaction is successfully committed, which means that the execution of code
in **TX_BEGIN**() block has not been interrupted by an error or by a call to **pmemobj_tx_abort**(). This block is optional.

```c
TX_FINALLY
```

The **TX_FINALLY** macro starts a block of code that will be executed regardless of whether the transaction is committed or aborted. This block is optional.

```c
TX_END
```

The **TX_END** macro cleans up and closes the transaction started by **TX_BEGIN**() / **TX_BEGIN_PARAM**() / **TX_BEGIN_CB**() macros. It is mandatory
to terminate each transaction with this macro. If the transaction was aborted, *errno* is set appropriately.

Similarly to the macros controlling the transaction flow, the **libpmemobj** defines a set of macros that simplify the transactional operations on persistent
objects. Note that those macros operate on typed object handles, thus eliminating the need to specify the size of the object, or the size and offset of the
field in the user-defined structure that is to be modified.

```c
TX_ADD_FIELD(TOID o, FIELD)
```

The **TX_ADD_FIELD**() macro saves in the undo log the current value of given *FIELD* of the object referenced by a handle *o*. The application is then free to
directly modify the specified *FIELD*. In case of a failure or abort, the saved value will be restored.

```c
TX_XADD_FIELD(TOID o, FIELD, uint64_t flags)
```

The **TX_XADD_FIELD**() macro works exactly like **TX_ADD_FIELD** when *flags* equals 0. The *flags* argument is a bitmask of values described in
**pmemobj_tx_xadd_range** section.

```c
TX_ADD(TOID o)
```

The **TX_ADD**() macro takes a "snapshot" of the entire object referenced by object handle *o* and saves it in the undo log. The object size is determined from
its *TYPE*. The application is then free to directly modify the object. In case of a failure or abort, all the changes within the object will be rolled-back.

```c
TX_XADD(TOID o, uint64_t flags)
```

The **TX_XADD**() macro works exactly like **TX_ADD** when *flags* equals 0. The *flags* argument is a bitmask of values described in
**pmemobj_tx_xadd_range** section.

```c
TX_ADD_FIELD_DIRECT(TYPE *p, FIELD)
```

The **TX_ADD_FIELD_DIRECT**() macro saves in the undo log the current value of given *FIELD* of the object referenced by (direct) pointer *p*. The application
is then free to directly modify the specified *FIELD*. In case of a failure or abort, the saved value will be restored.

```c
TX_XADD_FIELD_DIRECT(TYPE *p, FIELD, uint64_t flags)
```

The **TX_XADD_FIELD_DIRECT**() macro works exactly like **TX_ADD_FIELD_DIRECT** when *flags* equals 0. The *flags* argument is a bitmask of values described in
**pmemobj_tx_xadd_range_direct** section.

```c
TX_ADD_DIRECT(TYPE *p)
```

The **TX_ADD_DIRECT**() macro takes a "snapshot" of the entire object referenced by (direct) pointer *p* and saves it in the undo log. The object size is
determined from its *TYPE*. The application is then free to directly modify the object. In case of a failure or abort, all the changes within the object will
be rolled-back.

```c
TX_XADD_DIRECT(TYPE *p, uint64_t flags)
```

The **TX_XADD_DIRECT**() macro works exactly like **TX_ADD_DIRECT** when *flags* equals 0. The *flags* argument is a bitmask of values described in
**pmemobj_tx_xadd_range_direct** section.

```c
TX_SET(TOID o, FIELD, VALUE)
```

The **TX_SET**() macro saves in the undo log the current value of given *FIELD* of the object referenced by a handle *o*, and then set its new *VALUE*. In case of
a failure or abort, the saved value will be restored.

```c
TX_SET_DIRECT(TYPE *p, FIELD, VALUE)
```

The **TX_SET_DIRECT**() macro saves in the undo log the current value of given *FIELD* of the object referenced by (direct) pointer *p*, and then set its new
*VALUE*. In case of a failure or abort, the saved value will be restored.

```c
TX_MEMCPY(void *dest, const void *src, size_t num)
```

The **TX_MEMCPY**() macro saves in the undo log the current content of *dest* buffer and then overwrites the first *num* bytes of its memory area with the data
copied from the buffer pointed by *src*. In case of a failure or abort, the saved value will be restored.

```c
TX_MEMSET(void *dest, int c, size_t num)
```

The **TX_MEMSET**() macro saves in the undo log the current content of *dest* buffer and then fills the first *num* bytes of its memory area with the constant byte
*c*. In case of a failure or abort, the saved value will be restored.

```c
TX_NEW(TYPE)
```

The **TX_NEW**() macro transactionally allocates a new object of given *TYPE* and assigns it a type number read from the typed *OID*. The allocation size is
determined from the size of the user-defined structure *TYPE*. If successful and called during **TX_STAGE_WORK** it returns a handle to the newly allocated
object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately.

```c
TX_ALLOC(TYPE, size_t size)
```

The **TX_ALLOC**() macro transactionally allocates a new object of given *TYPE* and assigns it a type number read from the typed *OID*. The allocation size is
passed by *size* parameter. If successful and called during **TX_STAGE_WORK** it returns a handle to the newly allocated object. Otherwise, stage changes to
**TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately.

```c
TX_ZNEW(TYPE)
```

The **TX_ZNEW**() macro transactionally allocates a new zeroed object of given *TYPE* and assigns it a type number read from the typed *OID*. The allocation
size is determined from the size of the user-defined structure *TYPE*. If successful and called during **TX_STAGE_WORK** it returns a handle to the newly
allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately.

```c
TX_ZALLOC(TYPE, size_t size)
```

The **TX_ZALLOC**() macro transactionally allocates a new zeroed object of given *TYPE* and assigns it a type number read from the typed *OID*. The allocation
size is passed by *size* argument. If successful and called during **TX_STAGE_WORK** it returns a handle to the newly allocated object. Otherwise, stage changes
to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately.

```c
TX_XALLOC(TYPE, size_t size, uint64_t flags)
```

The **TX_XALLOC**() macro transactionally allocates a new object of given *TYPE* and assigns it a type number read from the typed *OID*. The allocation size is passed by *size* argument. The *flags* argument is a bitmask of values described in **pmemobj_tx_xalloc** section. If successful and called during **TX_STAGE_WORK** it returns a handle to the newly allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately.

```c
TX_REALLOC(TOID o, size_t size)
```

The **TX_REALLOC**() macro transactionally resizes an existing object referenced by a handle *o* to the given *size*. If successful and called during
**TX_STAGE_WORK** it returns a handle to the reallocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set
appropriately.

```c
TX_ZREALLOC(TOID o, size_t size)
```

The **TX_ZREALLOC**() macro transactionally resizes an existing object referenced by a handle *o* to the given *size*. If the new size is larger than the old
size, the extended new space is zeroed. If successful and called during **TX_STAGE_WORK** it returns a handle to the reallocated object. Otherwise, stage changes
to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and *errno* is set appropriately.

```c
TX_STRDUP(const char *s, uint64_t type_num)
```

The **TX_STRDUP**() macro transactionally allocates a new object containing a duplicate of the string *s* and assigns it a type *type_num*. If successful and
called during **TX_STAGE_WORK** it returns a handle to the newly allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and
*errno* is set appropriately.

```c
TX_WCSDUP(const wchar_t *s, uint64_t type_num)
```

The **TX_WCSDUP**() macro transactionally allocates a new object containing a duplicate of the wide character string *s* and assigns it a type *type_num*. If successful and
called during **TX_STAGE_WORK** it returns a handle to the newly allocated object. Otherwise, stage changes to **TX_STAGE_ONABORT**, **OID_NULL** is returned, and
*errno* is set appropriately.

```c
TX_FREE(TOID o)
```

The **TX_FREE**() transactionally frees the memory space represented by an object handle *o*. If *o* is **OID_NULL**, no operation is performed. If successful
and called during **TX_STAGE_WORK** it returns zero. Otherwise, stage changes to **TX_STAGE_ONABORT** and an error number is returned.


# CAVEATS #

The transaction flow control is governed by the **setjmp**(3)/**longjmp**(3) macros and they are used in both the macro and function flavors of the API. The
transaction will longjmp on transaction abort. This has one major drawback which is described in the ISO C standard subsection 7.13.2.1. It says that **the
values of objects of automatic storage duration that are local to the function containing the setjmp invocation that do not have volatile-qualified type and
have been changed between the setjmp invocation and longjmp call are indeterminate.**

The following example illustrates the issue described above.

```c
int *bad_example_1 = (int *)0xBAADF00D;
int *bad_example_2 = (int *)0xBAADF00D;
int *bad_example_3 = (int *)0xBAADF00D;
int * volatile good_example = (int *)0xBAADF00D;

TX_BEGIN(pop) {
	bad_example_1 = malloc(sizeof(int));
	bad_example_2 = malloc(sizeof(int));
	bad_example_3 = malloc(sizeof(int));
	good_example = malloc(sizeof(int));

	/* manual or library abort called here */
	pmemobj_tx_abort(EINVAL);
} TX_ONCOMMIT {
	/*
	 * This section is longjmp-safe
	 */
} TX_ONABORT {
	/*
	 * This section is not longjmp-safe
	 */
	free(good_example); /* OK */
	free(bad_example_1); /* undefined behavior */
} TX_FINALLY {
	/*
	 * This section is not longjmp-safe on transaction abort only
	 */
	free(bad_example_2); /* undefined behavior */
} TX_END

free(bad_example_3); /* undefined behavior */
```

Objects which are not volatile-qualified, are of automatic storage duration and have been changed between the invocations of **setjmp**(3) and **longjmp**(3)
(that also means within the work section of the transaction after **TX_BEGIN**()) should not be used after a transaction abort or should be used with utmost care.
This also includes code after the **TX_END** macro.

**libpmemobj** is not cancellation-safe. The pool will never be corrupted because of canceled thread, but other threads may stall waiting on locks taken by
that thread. If application wants to use **pthread_cancel**(3), it must disable cancellation before calling **libpmemobj** APIs (see
**pthread_setcancelstate**(3) with **PTHREAD_CANCEL_DISABLE**) and re-enable it after. Deferring cancellation (**pthread_setcanceltype**(3) with
**PTHREAD_CANCEL_DEFERRED**) is not safe enough, because **libpmemobj** internally may call functions that are specified as cancellation points in POSIX.

**libpmemobj** relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**()) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.

# LIBRARY API VERSIONING #

This section describes how the library API is versioned, allowing applications to work with an evolving API.

```c
const char pmemobj_check_version(
	unsigned major_required,
	unsigned minor_required);
```

The **pmemobj_check_version**() function is used to see if the installed **libpmemobj** supports the version of the library API required by an application. The
easiest way to do this is for the application to supply the compile-time version information, supplied by defines in **\<libpmemobj.h\>**, like this:

```c
reason = pmemobj_check_version(PMEMOBJ_MAJOR_VERSION,
                               PMEMOBJ_MINOR_VERSION);
if (reason != NULL) {
	/* version check failed, reason string tells you why */
}
```

Any mismatch in the major version number is considered a failure, but a library with a newer minor version number will pass this check since increasing minor
versions imply backwards compatibility.

An application can also check specifically for the existence of an interface by checking for the version where that interface was introduced. These versions
are documented in this man page as follows: unless otherwise specified, all interfaces described here are available in version 1.0 of the library. Interfaces
added after version 1.0 will contain the text *introduced in version x.y* in the section of this manual describing the feature.

When the version check performed by **pmemobj_check_version**() is successful, the return value is NULL. Otherwise the return value is a static string
describing the reason for failing the version check. The string returned by **pmemobj_check_version**() must not be modified or freed.


# MANAGING LIBRARY BEHAVIOR #

The library entry points described in this section are less commonly used than the previous sections.

```c
void pmemobj_set_funcs(
	void *(*malloc_func)(size_t size),
	void (*free_func)(void *ptr),
	void *(*realloc_func)(void *ptr, size_t size),
	char *(*strdup_func)(const char *s));
```

The **pmemobj_set_funcs**() function allows an application to override memory allocation calls used internally by **libpmemobj**. Passing in NULL for any of
the handlers will cause the **libpmemobj** default function to be used. The library does not make heavy use of the system malloc functions, but it does
allocate approximately 4-8 kilobytes for each memory pool in use.

```c
int pmemobj_check(const char *path, const char *layout);
```

The **pmemobj_check**() function performs a consistency check of the file indicated by *path* and returns 1 if the memory pool is found to be consistent. Any
inconsistencies found will cause **pmemobj_check**() to return 0, in which case the use of the file with **libpmemobj** will result in undefined behavior. The
debug version of **libpmemobj** will provide additional details on inconsistencies when **PMEMOBJ_LOG_LEVEL** is at least 1, as described in the **DEBUGGING AND
ERROR HANDLING** section below. **pmemobj_check**() will return -1 and set *errno* if it cannot perform the consistency check due to other errors.
**pmemobj_check**() opens the given *path* read-only so it never makes any changes to the file. This function is not supported on Device DAX.


# DEBUGGING AND ERROR HANDLING #

Two versions of **libpmemobj** are typically available on a development system. The normal version, accessed when a program is linked using the **-lpmemobj**
option, is optimized for performance. That version skips checks that impact performance and never logs any trace information or performs any run-time
assertions. If an error is detected during the call to **libpmemobj** function, an application may retrieve an error message describing the reason of failure
using the following function:

```c
const char *pmemobj_errormsg(void);
```

The **pmemobj_errormsg**() function returns a pointer to a static buffer containing the last error message logged for current thread. The error message may
include description of the corresponding error code (if *errno* was set), as returned by **strerror**(3). The error message buffer is thread-local; errors
encountered in one thread do not affect its value in other threads. The buffer is never cleared by any library function; its content is significant only when
the return value of the immediately preceding call to **libpmemobj** function indicated an error, or if *errno* was set. The application must not modify or
free the error message string, but it may be modified by subsequent calls to other library functions.

A second version of **libpmemobj**, accessed when a program uses the libraries under **/usr/lib/nvml_debug**, contains run-time assertions and trace points.
The typical way to access the debug version is to set the environment variable **LD_LIBRARY_PATH** to **/usr/lib/nvml_debug** or **/usr/lib64/nvml_debug**
depending on where the debug libraries are installed on the system. The trace points in the debug version of the library are enabled using the environment
variable **PMEMOBJ_LOG_LEVEL** which can be set to the following values:

+ **0** - This is the default level when **PMEMOBJ_LOG_LEVEL** is not set. No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged (in addition to returning the *errno*-based errors as usual). The same information may be
retrieved using **pmemobj_errormsg**().

+ **2** - A trace of basic operations is logged.

+ **3** - This level enables a very verbose amount of function call tracing in the library.

+ **4** - This level enables voluminous and fairly obscure tracing information that is likely only useful to the **libpmemobj** developers.

The environment variable **PMEMOBJ_LOG_FILE** specifies a file name where all logging information should be written. If the last character in the name is
"-", the PID of the current process will be appended to the file name when the log file is created. If **PMEMOBJ_LOG_FILE** is not set, the logging output
goes to stderr.

Setting the environment variable **PMEMOBJ_LOG_LEVEL** has no effect on the non-debug version of **libpmemobj**.
See also **libpmem**(3) to get information about other environment variables affecting **libpmemobj** behavior.

**libpmemobj** by default supports up to 1024 parallel transactions / allocations.
For debugging purposes it is possible to decrease this value by writing
a desired limit to the **PMEMOBJ_NLANES** environment variable.

# CONTROL AND STATISTICS #

The library provides a uniform interface that allows to impact its behavior as
well as reason about its internals.

There are two main functions to that interface:
```c
int pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg);
int pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg);
```

The *name* argument specifies an entry point as defined in the CTL namespace
specification. The entry point description specifies whether the extra *arg* is
required.
Those two parameters together create a CTL query. The *pop* argument is optional if
the entry point resides in a global namespace (i.e. shared for all the pools).
The functions themselves are thread-safe and most of the entry points are too.
If there are special conditions in which an entry point has to be called, they
are explicitly stated in its description.
The functions propagate the return value of the entry point. If either the name
or the provided arguments are invalid, -1 is returned.

Entry points are leafs of a tree-like structure. Each one can read from the
internal state, write to the internal state or do both.

The CTL namespace is organized in a tree structure. Starting from the root,
each node can be either internal, containing other elements, or a leaf.
Internal nodes themselves can only contain other nodes and cannot be entry
points. There are two types of those nodes: named and indexed. Named nodes have
string identifiers. Indexed nodes represent an abstract array index and have an
associated string identifier. The index itself is user provided. A collection of
indexes present on the path of an entry point is provided to the handler
functions as name and index pairs.

The entry points are listed in the following format:

name | r(ead)w(rite) | global/- | read argument type | write argument type | config argument type

description...

# CTL NAMESPACE #

prefault.at_create | rw | global | int | int | boolean

If set, every single page of the pool will be touched and written to, in order
to trigger page allocation. This can be used to minimize performance impact of
pagefaults. Affects only the **pmemobj_create()** function.

Always returns 0.

prefault.at_open | rw | global | int | int | boolean

As above, but affects **pmemobj_open()** function.

tx.debug.skip_expensive_checks | rw | - | int | int | boolean

Turns off some expensive checks performed by transaction module in "debug"
builds. Ignored in "release" builds.

tx.cache.size | rw | - | long long | long long | integer

Size in bytes of the transaction snapshot cache size. The bigger it is the
frequency of persistent allocations is lower, but at the cost of higher
fixed cost.

This should be set to roughly the sum of sizes of the snapshotted regions in
an average transaction in the pool.

This value must be a in a range between 0 and **PMEMOBJ_MAX_ALLOC_SIZE**.
If the current threshold is larger than the new cache size, the threshold will
be made equal to the new size.

This entry point is not thread safe and should not be modified if there are any
transactions currently running.

Returns 0 if successful, -1 otherwise.

tx.cache.threshold | rw | - | long long | long long | integer

Threshold in bytes to which the snapshots will use the cache. All bigger
snapshots will trigger a persistent allocation.

This value must be a in a range between 0 and **tx.cache.size**.

This entry point is not thread safe and should not be modified if there are any
transactions currently running.

Returns 0 if successful, -1 otherwise.

tx.post_commit.queue_depth | rw | - | int | int | integer

Controls the depth of the post-commit tasks queue. A post-commit task is the
collection of work items that need to be performed on the persistent state after
a successfully completed transaction. This includes freeing no longer needed
objects and cleaning up various caches. By default, this queue does not exist
and the post-commit task is executed synchronously in the same thread that
ran the transaction. By changing this parameter, one can offload this task to
a separate worker. If the queue is full, the algorithm, instead of waiting,
performs the post-commit in the current thread.

The task is performed on a finite resource (lanes, of which there are 1024),
and if the worker threads that process this queue don't keep up with the
demand, regular threads might start to block waiting for that resource. This
will happen if the queue depth value is too large.

As a general rule, this value should be set to around: 1024 minus the average
number of threads in the application (not counting the post-commit workers).
But this may vary from workload to workload.

The queue depth value must also be a power of two.

This entry point is not thread-safe and must be called when no transactions are
currently being executed.

Returns 0 if successful, -1 otherwise.

tx.post_commit.worker | r- | - | void * | - | -

The worker function that one needs to launch in a thread to perform asynchronous
processing of post-commit tasks. It returns only after a stop entry point is
called. There might be many worker threads at a time. If there's no work to be
done, this function sleeps instead of polling.

Always returns 0.

tx.post_commit.stop | r- | - | void * | - | -

This function forces all the post-commit worker functions to exit and return
control back to the calling thread. This should be called before the application
terminates and the post commit worker threads needs to be shutdown.

After the invocation of this entry point, the post-commit task queue can no
longer be used. If there's a need to restart the worker threads after a stop,
the tx.post_commit.queue_depth needs to be set again.

This entry point must be called when no transactions are currently being
executed.

Always returns 0.

# CTL external configuration #

In addition to direct function call, each write entry point can also be set
using two alternative methods.

The first one is to load configuration directly from a **PMEMOBJ_CONF**
environment variable. Properly formatted ctl config string is a single-line
sequence of queries separated by ';':

```
query0;query1;...;queryN
```

A single query is constructed from the name of the ctl write entry point and
the argument, separated by '=':

```
entry_point=entry_point_argument
```

The entry point argument type is defined by the entry point itself, but there
are few predefined primitives:

	*) integer: represented by a sequence of [0-9] characters that form
		a single number.
	*) boolean: represented by a single character: y/n/Y/N/0/1, each
		corresponds to true or false. If the argument contains any
		trailing characters, they are ignored.
	*) string: a simple sequence of characters.

There are also complex argument types that are formed from the primitives
separated by a ',':

```
first_arg,second_arg
```

In summary, a full configuration sequence looks like this:

```
(first_entry_point)=(arguments, ...);...;(last_entry_point)=(arguments, ...);
```

As an example, to set both prefault at_open and at_create variables:
```

PMEMOBJ_CONF="prefault.at_open=1;prefault.at_create=1"
```

The second method of loading an external configuration is to set the
**PMEMOBJ_CONF_FILE** environment variable to point to a file that contains
a sequence of ctl queries. The parsing rules are all the same, but the file
can also contain white-spaces and comments.

To create a comment, simply use '#' anywhere in a line and everything
afterwards, until a new line '\n', will be ignored.

An example configuration file:

```
#########################
# My pmemobj configuration
#########################
#
# Global settings:
prefault. # modify the behavior of pre-faulting
	at_open = 1; # prefault when the pool is opened

prefault.
	at_create = 0; # but don't prefault when it's created

# Per-pool settings:
# ...

```


# EXAMPLE #

See <http://pmem.io/nvml/libpmemobj> for examples using the **libpmemobj** API.


# ACKNOWLEDGEMENTS #

**libpmemobj** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**mmap**(2), **munmap**(2), **msync**(2), **pthread_mutex**(3),
**pthread_rwlock**(3), **pthread_cond**(3), **strerror**(3), **libpmemblk**(3),
**libpmemlog**(3), **libpmem**(3), **libvmem**(3), **ndctl-create-namespace**(1)
and **<http://pmem.io>**
