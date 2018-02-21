---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMOBJ_CTL_GET, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.2
...

[comment]: <> (Copyright 2017-2018, Intel Corporation)

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

[comment]: <> (pmemobj_ctl_get.3 -- man page for libpmemobj CTL)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[CTL NAMESPACE](#ctl-namespace)<br />
[CTL EXTERNAL CONFIGURATION](#ctl-external-configuration)<br />
[SEE ALSO](#see-also)<br />


# NAME #

_UW(pmemobj_ctl_get),
_UW(pmemobj_ctl_set),
_UW(pmemobj_ctl_exec)
-- Query and modify libpmemobj internal behavior


# SYNOPSIS #

```c
#include <libpmemobj.h>

_UWFUNCR2(int, pmemobj_ctl_get, PMEMobjpool *pop, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
_UWFUNCR2(int, pmemobj_ctl_set, PMEMobjpool *pop, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
_UWFUNCR2(int, pmemobj_ctl_exec, PMEMobjpool *pop, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
```

_UNICODE()


# DESCRIPTION #

The _UW(pmemobj_ctl_get), _UW(pmemobj_ctl_set) and _UW(pmemobj_ctl_exec)
functions provide a uniform interface for querying and modifying the internal
behavior of **libpmemobj** through the control (CTL) namespace.

The CTL namespace is organized in a tree structure. Starting from the root,
each node can be either internal, containing other elements, or a leaf.
Internal nodes themselves can only contain other nodes and cannot be entry
points. There are two types of those nodes: *named* and *indexed*. Named nodes
have string identifiers. Indexed nodes represent an abstract array index and
have an associated string identifier. The index itself is provided by the user.
A collection of indexes present on the path of an entry point is provided to
the handler functions as name and index pairs.

The *name* argument specifies an entry point as defined in the CTL namespace
specification. The entry point description specifies whether the extra *arg* is
required. Those two parameters together create a CTL query. The *pop* argument
is optional if the entry point resides in a global namespace (i.e., is shared
for all the pools). The functions and the entry points are thread-safe unless
indicated otherwise below. If there are special conditions for calling an entry
point, they are explicitly stated in its description. The functions propagate
the return value of the entry point. If either *name* or *arg* is invalid, -1
is returned.

Entry points are the leaves of the CTL namespace structure. Each entry point
can read from the internal state, write to the internal state,
exec a function or a combination of these operations.

If an entry point takes a complex structure for its argument, the C `struct`
will contain `size` of the argument structure as its first field.
This variable is used for versioning. Normally, setting it takes the form of:
```
[arg].size = sizeof(struct [arg_type]);
```

The entry points are listed in the following format:

name | r(ead)w(rite)x(ecute) | global/- | read argument type | write argument type | exec argument type | config argument type

description...


# CTL NAMESPACE #

prefault.at_create | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is created, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the _UW(pmemobj_create) function.

Always returns 0.

prefault.at_open | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is opened, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the _UW(pmemobj_open) function.

Always returns 0.

tx.debug.skip_expensive_checks | rw | - | int | int | - | boolean

Turns off some expensive checks performed by the transaction module in "debug"
builds. Ignored in "release" builds.

tx.cache.size | rw | - | long long | long long | - | integer

Size in bytes of the transaction snapshot cache. In a larger cache the
frequency of persistent allocations is lower, but with higher fixed cost.

This should be set to roughly the sum of sizes of the snapshotted regions in
an average transaction in the pool.

This value must be a in a range between 0 and **PMEMOBJ_MAX_ALLOC_SIZE**.
If the current threshold is larger than the new cache size, the threshold will
be made equal to the new size.

This entry point is not thread safe and should not be modified if there are any
transactions currently running.

Returns 0 if successful, -1 otherwise.

tx.cache.threshold | rw | - | long long | long long | - | integer

Threshold in bytes, below which snapshots will use the cache. All larger
snapshots will trigger a persistent allocation.

This value must be a in a range between 0 and **tx.cache.size**.

This entry point is not thread safe and should not be modified if there are any
transactions currently running.

Returns 0 if successful, -1 otherwise.

tx.post_commit.queue_depth | rw | - | int | int | - | integer

Controls the depth of the post-commit tasks queue. A post-commit task is the
collection of work items that need to be performed on the persistent state after
a successfully completed transaction. This includes freeing no longer needed
objects and cleaning up various caches. By default, this queue does not exist
and the post-commit task is executed synchronously in the same thread that
ran the transaction. By changing this parameter, one can offload this task to
a separate worker. If the queue is full, the algorithm, instead of waiting,
performs the post-commit in the current thread.

The task is performed on a finite resource (lanes, of which there are 1024),
and if the worker threads that process this queue are unable to keep up with
the demand, regular threads might start to block waiting for that resource.
This will happen if the queue depth value is too large.

As a general rule, this value should be set to approximately 1024 minus the
average number of threads in the application (not counting the post-commit
workers); however, this may vary from workload to workload.

The queue depth value must also be a power of two.

This entry point is not thread-safe and must be called when no transactions are
currently being executed.

Returns 0 if successful, -1 otherwise.

tx.post_commit.worker | r- | - | void * | - | - | -

The worker function launched in a thread to perform asynchronous processing
of post-commit tasks. This function returns only after a stop entry point is
called. There may be many worker threads at a time. If there is no work to be
done, this function sleeps instead of polling.

Always returns 0.

tx.post_commit.stop | r- | - | void * | - | - | -

This function forces all the post-commit worker functions to exit and return
control back to the calling thread. This should be called before the application
terminates and the post commit worker threads need to be shutdown.

After the invocation of this entry point, the post-commit task queue can no
longer be used. If worker threads must be restarted after a stop,
the tx.post_commit.queue_depth needs to be set again.

This entry point must be called when no transactions are currently being
executed.

Always returns 0.

heap.alloc_class.[class_id].desc | rw | - | `struct pobj_alloc_class_desc` |
`struct pobj_alloc_class_desc` | - | integer, integer, string

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
	size_t size;
	size_t unit_size;
	unsigned units_per_block;
	enum pobj_header_type header_type;
	unsigned class_id;
};
```

The `size` field is the size of the argument structure. Can be set by:
```
[class_desc].size = sizeof(struct pobj_alloc_class_desc);
```
This field must not be present in a config string.

The first real field, `unit_size`, is an 8-byte unsigned integer that defines the
allocation class size. While theoretically limited only by
**PMEMOBJ_MAX_ALLOC_SIZE**, for most workloads this value should be between
8 bytes and 2 megabytes.

The `units_per_block` field defines how many units a single block of memory
contains. This value will be rounded up to match the internal size of the
block (256 kilobytes or a multiple thereof). For example, given a class with
a `unit_size` of 512 bytes and a `units_per_block` of 1000, a single block of
memory for that class will have 512 kilobytes.
This is relevant because the bigger the block size, the less frequently blocks
need to be fetched, resulting in lower contention on global heap state.
Keep in mind that object allocation is tracked in a bitmap with a limited
number of entries, making it inefficient to create allocation classes smaller
than 128 bytes.

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
heap.alloc_class.128.desc=500,1000,compact
```
This query, if executed, will create an allocation class with an id of 128 that
has a unit size of 500 bytes, has at least 1000 units per block and uses
a compact header.

For reading, function returns 0 if successful, if the allocation class does
not exist it sets the errno to **ENOENT** and returns -1;

For writing, function returns 0 if the allocation class has been successfully
created, -1 otherwise.

heap.alloc_class.new.desc | -w | - | - | `struct pobj_alloc_class_desc` | - | integer, integer, string

Same as `heap.alloc_class.[class_id].desc`, but instead of requiring the user
to provide the class_id, it automatically creates the allocation class with the
first available identifier.

This should be used when it's impossible to guarantee unique allocation class
naming in the application (e.g. when writing a library that uses libpmemobj).

The required class identifier will be stored in the `class_id` field of the
`struct pobj_alloc_class_desc`.

This function returns 0 if the allocation class has been successfully created,
-1 otherwise.

stats.enabled | rw | - | int | int | - | boolean

Enables or disables runtime collection of statistics. Statistics are not
recalculated after enabling; any operations that occur between disabling and
re-enabling will not be reflected in subsequent values.

Statistics are disabled by default. Enabling them may have non-trivial
performance impact.

Always returns 0.

stats.heap.curr_allocated | r- | - | int | - | - | -

Returns the number of bytes currently allocated in the heap. If statistics were
disabled at any time in the lifetime of the heap, this value may be
inaccurate.

heap.size.granularity | rw- | - | uint64_t | uint64_t | - | long long

Reads or modifies the granularity with which the heap grows when OOM.
Valid only if the poolset has been defined with directories.

A granularity of 0 specifies that the pool will not grow automatically.

This function returns 0 if the granularity value is 0, or is larger than
*PMEMOBJ_MIN_PART*, -1 otherwise.

heap.size.extend | --x | - | - | - | uint64_t | -

Extends the heap by the given size. Must be larger than *PMEMOBJ_MIN_PART*.

This function returns 0 if successful, -1 otherwise.

# CTL EXTERNAL CONFIGURATION #

In addition to direct function call, each write entry point can also be set
using two alternative methods.

The first method is to load a configuration directly from the **PMEMOBJ_CONF**
environment variable. A properly formatted ctl config string is a single-line
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
are three predefined primitives:

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

Complex argument types never contain the size of the argument structure.

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


# SEE ALSO #

**libpmemobj**(7) and **<http://pmem.io>**
