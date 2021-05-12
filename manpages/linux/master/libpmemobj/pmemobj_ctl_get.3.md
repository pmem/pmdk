---
layout: manual
Content-Style: 'text/css'
title: PMEMOBJ_CTL_GET
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause
[comment]: <> (Copyright 2017-2021, Intel Corporation)

[comment]: <> (pmemobj_ctl_get.3 -- man page for libpmemobj CTL)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[CTL NAMESPACE](#ctl-namespace)<br />
[CTL EXTERNAL CONFIGURATION](#ctl-external-configuration)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_ctl_get**(),
**pmemobj_ctl_set**(),
**pmemobj_ctl_exec**()
- Query and modify libpmemobj internal behavior (EXPERIMENTAL)

# SYNOPSIS #

```c
#include <libpmemobj.h>

int pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
int pmemobj_ctl_exec(PMEMobjpool *pop, const char *name, void *arg); (EXPERIMENTAL)
```



# DESCRIPTION #

The **pmemobj_ctl_get**(), **pmemobj_ctl_set**() and **pmemobj_ctl_exec**()
functions provide a uniform interface for querying and modifying the internal
behavior of **libpmemobj**(7) through the control (CTL) namespace.

The *name* argument specifies an entry point as defined in the CTL namespace
specification. The entry point description specifies whether the extra *arg* is
required. Those two parameters together create a CTL query. The functions and
the entry points are thread-safe unless
indicated otherwise below. If there are special conditions for calling an entry
point, they are explicitly stated in its description. The functions propagate
the return value of the entry point. If either *name* or *arg* is invalid, -1
is returned.

If the provided ctl query is valid, the CTL functions will always return 0
on success and -1 on failure, unless otherwise specified in the entry point
description.

See more in **pmem_ctl**(5) man page.

# CTL NAMESPACE #

prefault.at_create | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is created, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the **pmemobj_create**() function.

prefault.at_open | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is opened, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the **pmemobj_open**() function.

sds.at_create | rw | global | int | int | - | boolean

If set, force-enables or force-disables SDS feature during pool creation.
Affects only the **pmemobj_create**() function. See **pmempool_feature_query**(3)
for information about SDS (SHUTDOWN_STATE) feature.

copy_on_write.at_open | rw | global | int | int | - | boolean

If set, pool is mapped in such a way that modifications don't reach the
underlying medium. From the user's perspective this means that when the pool
is closed all changes are reverted. This feature is not supported for pools
located on Device DAX.

tx.debug.skip_expensive_checks | rw | - | int | int | - | boolean

Turns off some expensive checks performed by the transaction module in "debug"
builds. Ignored in "release" builds.

tx.debug.verify_user_buffers | rw | - | int | int | - | boolean

Enables verification of user buffers provided by
**pmemobj_tx_log_append_buffer**(3) API. For now the only verified aspect
is whether the same buffer is used simultaneously in 2 or more transactions
or more than once in the same transaction. This value should not be modified
at runtime if any transaction for the current pool is in progress.

tx.cache.size | rw | - | long long | long long | - | integer

Size in bytes of the transaction snapshot cache. In a larger cache the
frequency of persistent allocations is lower, but with higher fixed cost.

This should be set to roughly the sum of sizes of the snapshotted regions in
an average transaction in the pool.

This entry point is not thread safe and should not be modified if there are any
transactions currently running.

This value must be a in a range between 0 and **PMEMOBJ_MAX_ALLOC_SIZE**,
otherwise this entry point will fail.

tx.cache.threshold | rw | - | long long | long long | - | integer

This entry point is deprecated.
All snapshots, regardless of the size, use the transactional cache.

tx.post_commit.queue_depth | rw | - | int | int | - | integer

This entry point is deprecated.

tx.post_commit.worker | r- | - | void * | - | - | -

This entry point is deprecated.

tx.post_commit.stop | r- | - | void * | - | - | -

This entry point is deprecated.

heap.narenas.automatic | r- | - | unsigned | - | - | -

Reads the number of arenas used in automatic scheduling of memory operations
for threads. By default, this value is equal to the number of available processors.
An arena is a memory management structure which enables concurrency by taking
exclusive ownership of parts of the heap and allowing associated threads to allocate
without contention.

heap.narenas.total | r- | - | unsigned | - | - | -

Reads the number of all created arenas. It includes automatic arenas
created by default and arenas created using heap.arena.create CTL.

heap.narenas.max | rw- | - | unsigned | unsigned | - | -

Reads or writes the maximum number of arenas that can be created.
This entry point is not thread-safe with regards to heap
operations (allocations, frees, reallocs).

heap.arena.[arena_id].size | r- | - | uint64_t | - | - | -

Reads the total amount of memory in bytes which is currently
exclusively owned by the arena. Large differences in this value between
arenas might indicate an uneven scheduling of memory resources.
The arena id cannot be 0.

heap.thread.arena_id | rw- | - | unsigned | unsigned | - | -

Reads the index of the arena assigned to the current thread or
assigns arena with specific id to the current thread.
The arena id cannot be 0.

heap.arena.create | --x | - | - | - | unsigned | -

Creates and initializes one new arena in the heap.
This entry point reads an id of the new created arena.

Newly created arenas by this CTL are inactive, which means that
the arena will not be used in the automatic scheduling of
memory requests. To activate the new arena, use heap.arena.[arena_id].automatic CTL.

Arena created using this CTL can be used for allocation by explicitly
specifying the *arena_id* for **POBJ_ARENA_ID(id)** flag in
**pmemobj_tx_xalloc**()/**pmemobj_xalloc**()/**pmemobj_xreserve()** functions.

By default, the number of arenas is limited to 1024.

heap.arena.[arena_id].automatic | rw- | - | boolean | boolean | - | -

Reads or modifies the state of the arena.
If set, the arena is used in automatic scheduling of memory operations for threads.
This should be set to false if the application wants to manually manage allocator
scalability through explicitly assigning arenas to threads by using heap.thread.arena_id.
The arena id cannot be 0 and at least one automatic arena must exist.

heap.arenas_assignment_type | rw | global | `enum pobj_arenas_assignment_type` | `enum pobj_arenas_assignment_type` | - | string

Reads or modifies the behavior of arenas assignment for threads. By default,
each thread is assigned its own arena from the pool of automatic arenas
(described earlier). This consumes one TLS key from the OS for every open
pool. Applications that wish to avoid this behavior can instead rely on one
global arena assignment per pool. This might limits scalability if not using
arenas explicitly.

The argument for this CTL is an enum with the following types:

 - **POBJ_ARENAS_ASSIGNMENT_THREAD_KEY**, string value: `thread`.
	Default, threads use individually assigned arenas.
 - **POBJ_ARENAS_ASSIGNMENT_GLOBAL**, string value: `global`.
	Threads use one global arena.

Changing this value has no impact on already open pools. It should typically be
set at the beginning of the application, before any pools are opened or created.

heap.alloc_class.[class_id].desc | rw | - | `struct pobj_alloc_class_desc` |
`struct pobj_alloc_class_desc` | - | integer, integer, integer, string

Describes an allocation class. Allows one to create or view the internal
data structures of the allocator.

Creating custom allocation classes can be beneficial for both raw allocation
throughput, scalability and, most importantly, fragmentation. By carefully
constructing allocation classes that match the application workload,
one can entirely eliminate external and internal fragmentation. For example,
it is possible to easily construct a slab-like allocation mechanism for any
data structure.

The `[class_id]` is an index field. Only values between 0-254 are valid.
If setting an allocation class, but the `class_id` is already taken, the
function will return -1.
The values between 0-127 are reserved for the default allocation classes of the
library and can be used only for reading.

The recommended method for retrieving information about all allocation classes
is to call this entry point for all class ids between 0 and 254 and discard
those results for which the function returns an error.

This entry point takes a complex argument.

```
struct pobj_alloc_class_desc {
	size_t unit_size;
	size_t alignment;
	unsigned units_per_block;
	enum pobj_header_type header_type;
	unsigned class_id;
};
```

The first field, `unit_size`, is an 8-byte unsigned integer that defines the
allocation class size. While theoretically limited only by
**PMEMOBJ_MAX_ALLOC_SIZE**, for most workloads this value should be between
8 bytes and 2 megabytes.

The `alignment` field specifies the user data alignment of objects allocated
using the class. If set, must be a power of two and an even divisor of unit
size. Alignment is limited to maximum of 2 megabytes.
All objects have default alignment of 64 bytes, but the user data alignment
is affected by the size of the chosen header.

The `units_per_block` field defines how many units a single block of memory
contains. This value will be adjusted to match the internal size of the
block (256 kilobytes or a multiple thereof). For example, given a class with
a `unit_size` of 512 bytes and a `units_per_block` of 1000, a single block of
memory for that class will have 512 kilobytes.
This is relevant because the bigger the block size, the less frequently blocks
need to be fetched, resulting in lower contention on global heap state.
If the CTL call is being done at runtime, the `units_per_block` variable of the
provided alloc class structure is modified to match the actual value.

The `header_type` field defines the header of objects from the allocation class.
There are three types:

 - **POBJ_HEADER_LEGACY**, string value: `legacy`. Used for allocation classes
	prior to version 1.3 of the library. Not recommended for use.
	Incurs a 64 byte metadata overhead for every object.
	Fully supports all features.
 - **POBJ_HEADER_COMPACT**, string value: `compact`. Used as default for all
	predefined allocation classes.
	Incurs a 16 byte metadata overhead for every object.
	Fully supports all features.
 - **POBJ_HEADER_NONE**, string value: `none`. Header type that
	incurs no metadata overhead beyond a single bitmap entry. Can be used
	for very small allocation classes or when objects must be adjacent to
	each other.
	This header type does not support type numbers (type number is always
	0) or allocations that span more than one unit.

The `class_id` field is an optional, runtime-only variable that allows the
user to retrieve the identifier of the class. This will be equivalent to the
provided `[class_id]`. This field cannot be set from a config file.

The allocation classes are a runtime state of the library and must be created
after every open. It is highly recommended to use the configuration file to
store the classes.

This structure is declared in the `libpmemobj/ctl.h` header file. Please refer
to this file for an in-depth explanation of the allocation classes and relevant
algorithms.

Allocation classes constructed in this way can be leveraged by explicitly
specifying the class using **POBJ_CLASS_ID(id)** flag in **pmemobj_tx_xalloc**()/**pmemobj_xalloc**()
functions.

Example of a valid alloc class query string:
```
heap.alloc_class.128.desc=500,0,1000,compact
```
This query, if executed, will create an allocation class with an id of 128 that
has a unit size of 500 bytes, has at least 1000 units per block and uses
a compact header.

For reading, function returns 0 if successful, if the allocation class does
not exist it sets the errno to **ENOENT** and returns -1;

This entry point can fail if any of the parameters of the allocation class
is invalid or if exactly the same class already exists.

heap.alloc_class.new.desc | -w | - | - | `struct pobj_alloc_class_desc` | - | integer, integer, integer, string

Same as `heap.alloc_class.[class_id].desc`, but instead of requiring the user
to provide the class_id, it automatically creates the allocation class with the
first available identifier.

This should be used when it's impossible to guarantee unique allocation class
naming in the application (e.g. when writing a library that uses libpmemobj).

The required class identifier will be stored in the `class_id` field of the
`struct pobj_alloc_class_desc`.

stats.enabled | rw | - | enum pobj_stats_enabled | enum pobj_stats_enabled | - |
string

Enables or disables runtime collection of statistics. There are two types of
statistics: persistent and transient ones. Persistent statistics survive pool
restarts, whereas transient ones don't. Statistics are not recalculated after
enabling; any operations that occur between disabling and re-enabling will not
be reflected in subsequent values.

Only transient statistics are enabled by default. Enabling persistent statistics
may have non-trivial performance impact.

stats.heap.curr_allocated | r- | - | uint64_t | - | - | -

Reads the number of bytes currently allocated in the heap. If statistics were
disabled at any time in the lifetime of the heap, this value may be
inaccurate.

This is a persistent statistic.

stats.heap.run_allocated | r- | - | uint64_t | - | - | -

Reads the number of bytes currently allocated using run-based allocation
classes, i.e., huge allocations are not accounted for in this statistic.
This is useful for comparison against stats.heap.run_active to estimate the
ratio between active and allocated memory.

This is a transient statistic and is rebuilt every time the pool is opened.

stats.heap.run_active | r- | - | uint64_t | - | - | -

Reads the number of bytes currently occupied by all run memory blocks, including
both allocated and free space, i.e., this is all the all space that's not
occupied by huge allocations.

This value is a sum of all allocated and free run memory. In systems where
memory is efficiently used, `run_active` should closely track
`run_allocated`, and the amount of active, but free, memory should be minimal.

A large relative difference between active memory and allocated memory is
indicative of heap fragmentation. This information can be used to make
a decision to call **pmemobj_defrag()**(3) if the fragmentation looks to be high.

However, for small heaps `run_active` might be disproportionately higher than
`run_allocated` because the allocator typically activates a significantly larger
amount of memory than is required to satisfy a single request in the
anticipation of future needs. For example, the first allocation of 100 bytes
in a heap will trigger activation of 256 kilobytes of space.

This is a transient statistic and is rebuilt lazily every time the pool
is opened.

heap.size.granularity | rw- | - | uint64_t | uint64_t | - | long long

Reads or modifies the granularity with which the heap grows when OOM.
Valid only if the poolset has been defined with directories.

A granularity of 0 specifies that the pool will not grow automatically.

This entry point can fail if the granularity value is non-zero and smaller
than *PMEMOBJ_MIN_PART*.

heap.size.extend | --x | - | - | - | uint64_t | -

Extends the heap by the given size. Must be larger than *PMEMOBJ_MIN_PART*.

This entry point can fail if the pool does not support extend functionality or
if there's not enough space left on the device.

debug.heap.alloc_pattern | rw | - | int | int | - | -

Single byte pattern that is used to fill new uninitialized memory allocation.
If the value is negative, no pattern is written. This is intended for
debugging, and is disabled by default.

# CTL EXTERNAL CONFIGURATION #

In addition to direct function call, each write entry point can also be set
using two alternative methods.

The first method is to load a configuration directly from the **PMEMOBJ_CONF**
environment variable.

The second method of loading an external configuration is to set the
**PMEMOBJ_CONF_FILE** environment variable to point to a file that contains
a sequence of ctl queries.

See more in **pmem_ctl**(5) man page.

# SEE ALSO #

**libpmemobj**(7), **pmem_ctl**(5) and **<https://pmem.io>**
