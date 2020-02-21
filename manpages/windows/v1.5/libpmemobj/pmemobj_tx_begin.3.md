---
layout: manual
Content-Style: 'text/css'
title: PMEMOBJ_TX_BEGIN
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (Copyright 2017, Intel Corporation)

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

[comment]: <> (pmemobj_tx_begin.3 -- man page for transactional object manipulation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[CAVEATS](#caveats)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemobj_tx_stage**(),

**pmemobj_tx_begin**(), **pmemobj_tx_lock**(),
**pmemobj_tx_abort**(), **pmemobj_tx_commit**(),
**pmemobj_tx_end**(), **pmemobj_tx_errno**(),
**pmemobj_tx_process**(),

**TX_BEGIN_PARAM**(), **TX_BEGIN_CB**(),
**TX_BEGIN**(), **TX_ONABORT**,
**TX_ONCOMMIT**, **TX_FINALLY**, **TX_END**
- transactional object manipulation


# SYNOPSIS #

```c
#include <libpmemobj.h>

enum pobj_tx_stage pmemobj_tx_stage(void);

int pmemobj_tx_begin(PMEMobjpool *pop, jmp_buf *env, enum pobj_tx_param, ...);
int pmemobj_tx_lock(enum tx_lock lock_type, void *lockp);
void pmemobj_tx_abort(int errnum);
void pmemobj_tx_commit(void);
int pmemobj_tx_end(void);
int pmemobj_tx_errno(void);
void pmemobj_tx_process(void);

TX_BEGIN_PARAM(PMEMobjpool *pop, ...)
TX_BEGIN_CB(PMEMobjpool *pop, cb, arg, ...)
TX_BEGIN(PMEMobjpool *pop)
TX_ONABORT
TX_ONCOMMIT
TX_FINALLY
TX_END
```


# DESCRIPTION #

The non-transactional functions and macros described in **pmemobj_alloc**(3),
**pmemobj_list_insert**(3) and **POBJ_LIST_HEAD**(3) only guarantee the
atomicity of a single operation on an object. In case of more complex changes
involving multiple operations on an object, or allocation and modification
of multiple objects, data consistency and fail-safety may be provided only
by using *atomic transactions*.

A transaction is defined as series of operations on persistent memory
objects that either all occur, or nothing occurs. In particular,
if the execution of a transaction is interrupted by a power failure
or a system crash, it is guaranteed that after system restart,
all the changes made as a part of the uncompleted transaction
will be rolled back, restoring the consistent state of the memory
pool from the moment when the transaction was started.

Note that transactions do not provide atomicity with respect
to other threads. All the modifications performed within the transactions
are immediately visible to other threads. Therefore it is the responsibility
of the application to implement a proper thread synchronization mechanism.

Each thread may have only one transaction open at a time, but that
transaction may be nested. Nested transactions are flattened. Committing
the nested transaction does not commit the outer transaction; however, errors
in the nested transaction are propagated up to the outermost level, resulting
in the interruption of the entire transaction.

Each transaction is visible only for the thread that started it.
No other threads can add operations, commit or abort the transaction
initiated by another thread. Multiple threads may have transactions open on a
given memory pool at the same time.

Please see the **CAVEATS** section below for known limitations of the
transactional API.

The **pmemobj_tx_stage**() function returns the current *transaction stage*
for a thread. Stages are changed only by the **pmemobj_tx_\***() functions.
Transaction stages are defined as follows:

+ **TX_STAGE_NONE** - no open transaction in this thread

+ **TX_STAGE_WORK** - transaction in progress

+ **TX_STAGE_ONCOMMIT** - successfully committed

+ **TX_STAGE_ONABORT** - starting the transaction failed or transaction aborted

+ **TX_STAGE_FINALLY** - ready for clean up

The **pmemobj_tx_begin**() function starts a new transaction in the current
thread. If called within an open transaction, it starts a nested transaction.
The caller may use the *env* argument to provide a pointer to a
calling environment to be restored in case of transaction abort. This
information must be provided by the caller using the **setjmp**(3) macro.

A new transaction may be started only if the current stage is **TX_STAGE_NONE**
or **TX_STAGE_WORK**. If successful, the *transaction stage* changes to
**TX_STAGE_WORK**. Otherwise, the stage is changed to **TX_STAGE_ONABORT**.

Optionally, a list of parameters for the transaction may be provided.
Each parameter consists of a type followed by a type-specific number
of values. Currently there are 4 types:

+ **TX_PARAM_NONE**, used as a termination marker. No following value.

+ **TX_PARAM_MUTEX**, followed by one value, a pmem-resident PMEMmutex

+ **TX_PARAM_RWLOCK**, followed by one value, a pmem-resident PMEMrwlock

+ **TX_PARAM_CB**, followed by two values: a callback function
of type *pmemobj_tx_callback*, and a void pointer

Using **TX_PARAM_MUTEX** or **TX_PARAM_RWLOCK** causes the specified lock to
be acquired at the beginning of the transaction. **TX_PARAM_RWLOCK** acquires
the lock for writing. It is guaranteed that **pmemobj_tx_begin**() will acquire
all locks prior to successful completion, and they will be held by the current
thread until the outermost transaction is finished. Locks are taken in order
from left to right. To avoid deadlocks, the user is responsible for proper
lock ordering.

**TX_PARAM_CB** registers the specified callback function to be executed at
each transaction stage. For **TX_STAGE_WORK**, the callback is executed prior
to commit. For all other stages, the callback is executed as the first
operation after a stage change. It will also be called after each transaction;
in this case the *stage* parameter will be set to **TX_STAGE_NONE**.
*pmemobj_tx_callback* must be compatible with:

```void func(PMEMobjpool *pop, enum pobj_tx_stage stage, void *arg)```

*pop* is a pool identifier used in **pmemobj_tx_begin**(), *stage* is a current
transaction stage and *arg* is the second parameter of **TX_PARAM_CB**.
Without considering transaction nesting, this mechanism can be considered an
alternative method for executing code between stages (instead of
**TX_ONCOMMIT**, **TX_ONABORT**, etc). However, there are 2 significant
differences when nested transactions are used:

+ The registered function is executed only in the outermost transaction,
even if registered in an inner transaction.

+ There can be only one callback in the entire transaction, that is, the
callback cannot be changed in an inner transaction.

Note that **TX_PARAM_CB** does not replace the **TX_ONCOMMIT**, **TX_ONABORT**,
etc. macros. They can be used together: the callback will be executed *before*
a **TX_ONCOMMIT**, **TX_ONABORT**, etc. section.

**TX_PARAM_CB** can be used when the code dealing with transaction stage
changes is shared between multiple users or when it must be executed only
in the outer transaction. For example it can be very useful when the
application must synchronize persistent and transient state.

The **pmemobj_tx_lock**() function acquires the lock *lockp* of type
*lock_type* and adds it to the current transaction. *lock_type* may be
**TX_LOCK_MUTEX** or **TX_LOCK_RWLOCK**; *lockp* must be of type
*PMEMmutex* or *PMEMrwlock*, respectively. If *lock_type* is **TX_LOCK_RWLOCK**
the lock is acquired for writing. If the lock is not successfully
acquired, the function returns an error number. This function must be
called during **TX_STAGE_WORK**.

**pmemobj_tx_abort**() aborts the current transaction and causes a transition
to **TX_STAGE_ONABORT**. If *errnum* is equal to 0, the transaction
error code is set to **ECANCELED**; otherwise, it is set to *errnum*.
This function must be called during **TX_STAGE_WORK**.

The **pmemobj_tx_commit**() function commits the current open transaction and
causes a transition to **TX_STAGE_ONCOMMIT**. If called in the context of the
outermost transaction, all the changes may be considered as durably written
upon successful completion. This function must be called during
**TX_STAGE_WORK**.

The **pmemobj_tx_end**() function performs a cleanup of the current
transaction. If called in the context of the outermost transaction, it releases
all the locks acquired by **pmemobj_tx_begin**() for outer and nested
transactions. If called in the context of a nested transaction, it returns
to the context of the outer transaction in **TX_STAGE_WORK**, without releasing
any locks. The **pmemobj_tx_end**() function can be called during
**TX_STAGE_NONE** if transitioned to this stage using **pmemobj_tx_process**().
If not already in **TX_STAGE_NONE**, it causes the transition to
**TX_STAGE_NONE**.  **pmemobj_tx_end** must always be called for each
**pmemobj_tx_begin**(), even if starting the transaction failed. This function
must *not* be called during **TX_STAGE_WORK**.

The **pmemobj_tx_errno**() function returns the error code of the last transaction.

The **pmemobj_tx_process**() function performs the actions associated with the
current stage of the transaction, and makes the transition to the next stage.
It must be called in a transaction. The current stage must always be obtained
by a call to **pmemobj_tx_stage**(). **pmemobj_tx_process**() performs
the following transitions in the transaction stage flow:

+ **TX_STAGE_WORK** -> **TX_STAGE_ONCOMMIT**

+ **TX_STAGE_ONABORT** -> **TX_STAGE_FINALLY**

+ **TX_STAGE_ONCOMMIT** -> **TX_STAGE_FINALLY**

+ **TX_STAGE_FINALLY** -> **TX_STAGE_NONE**

+ **TX_STAGE_NONE** -> **TX_STAGE_NONE**

**pmemobj_tx_process**() must not be called after calling **pmemobj_tx_end**()
for the outermost transaction.

In addition to the above API, **libpmemobj**(7) offers a more intuitive method
of building transactions using the set of macros described below. When using
these macros, the complete transaction flow looks like this:

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
TX_BEGIN_CB(PMEMobjpool *pop, cb, arg, ...)
TX_BEGIN(PMEMobjpool *pop)
```

The **TX_BEGIN_PARAM**(), **TX_BEGIN_CB**() and **TX_BEGIN**() macros start
a new transaction in the same way as **pmemobj_tx_begin**(), except that instead
of the environment buffer provided by a caller, they set up the local *jmp_buf*
buffer and use it to catch the transaction abort. The **TX_BEGIN**() macro
starts a transaction without any options. **TX_BEGIN_PARAM** may be used when
there is a need to acquire locks prior to starting a transaction (such as
for a multi-threaded program) or set up a transaction stage callback.
**TX_BEGIN_CB** is just a wrapper around **TX_BEGIN_PARAM** that validates
the callback signature. (For compatibility there is also a **TX_BEGIN_LOCK**
macro, which is an alias for **TX_BEGIN_PARAM**). Each of these macros must be
followed by a block of code with all the operations that are to be performed
atomically.

The **TX_ONABORT** macro starts a block of code that will be executed only
if starting the transaction fails due to an error in **pmemobj_tx_begin**(),
or if the transaction is aborted. This block is optional, but in practice
it should not be omitted. If it is desirable to crash the application when a
transaction aborts and there is no **TX_ONABORT** section, the application can
define the **POBJ_TX_CRASH_ON_NO_ONABORT** macro before inclusion of
**\<libpmemobj.h\>**. This provides a default **TX_ONABORT** section which
just calls **abort**(3).

The **TX_ONCOMMIT** macro starts a block of code that will be executed only
if the transaction is successfully committed, which means that the execution
of code in the **TX_BEGIN**() block has not been interrupted by an error or by
a call to **pmemobj_tx_abort**(). This block is optional.

The **TX_FINALLY** macro starts a block of code that will be executed regardless
of whether the transaction is committed or aborted. This block is optional.

The **TX_END** macro cleans up and closes the transaction started by the
**TX_BEGIN**() / **TX_BEGIN_PARAM**() / **TX_BEGIN_CB**() macros.
It is mandatory to terminate each transaction with this macro. If the transaction
was aborted, *errno* is set appropriately.


# RETURN VALUE #

The **pmemobj_tx_stage**() function returns the stage of the current transaction
stage for a thread.

On success, **pmemobj_tx_begin**() returns 0. Otherwise, an error number is
returned.

The **pmemobj_tx_begin**() and **pmemobj_tx_lock**() functions return
zero if *lockp* is successfully added to the transaction.
Otherwise, an error number is returned.

The **pmemobj_tx_abort**() and **pmemobj_tx_commit**() functions return no value.

The **pmemobj_tx_end**() function returns 0 if the transaction was successful.
Otherwise it returns the error code set by **pmemobj_tx_abort**().
Note that **pmemobj_tx_abort**() can be called internally by the library.

The **pmemobj_tx_errno**() function returns the error code of the last transaction.

The **pmemobj_tx_process**() function returns no value.


# CAVEATS #

Transaction flow control is governed by the **setjmp**(3) and **longjmp**(3)
macros, and they are used in both the macro and function flavors of the API.
The transaction will longjmp on transaction abort. This has one major drawback,
which is described in the ISO C standard subsection 7.13.2.1. It says that
**the values of objects of automatic storage duration that are local to the
function containing the setjmp invocation that do not have volatile-qualified
type and have been changed between the setjmp invocation and longjmp call are
indeterminate.**

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

Objects which are not volatile-qualified, are of automatic storage duration
and have been changed between the invocations of **setjmp**(3) and
**longjmp**(3) (that also means within the work section of the transaction
after **TX_BEGIN**()) should not be used after a transaction abort, or should
be used with utmost care. This also includes code after the **TX_END** macro.

**libpmemobj**(7) is not cancellation-safe. The pool will never be corrupted
because of a canceled thread, but other threads may stall waiting on locks
taken by that thread. If the application wants to use **pthread_cancel**(3),
it must disable cancellation before calling any **libpmemobj**(7) APIs (see
**pthread_setcancelstate**(3) with **PTHREAD_CANCEL_DISABLE**), and re-enable
it afterwards. Deferring cancellation (**pthread_setcanceltype**(3) with
**PTHREAD_CANCEL_DEFERRED**) is not safe enough, because **libpmemobj**(7)
internally may call functions that are specified as cancellation points in POSIX.

**libpmemobj**(7) relies on the library destructor being called from the main
thread. For this reason, all functions that might trigger destruction (e.g.
**dlclose**(3)) should be called in the main thread. Otherwise some of the
resources associated with that thread might not be cleaned up properly.


# SEE ALSO #

**dlclose**(3), **longjmp**(3), **pmemobj_tx_add_range**(3),
**pmemobj_tx_alloc**(3), **pthread_setcancelstate**(3),
**pthread_setcanceltype**(3), **setjmp**(3),
**libpmemobj**(7) and **<http://pmem.io>**
