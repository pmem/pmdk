---
title: libpmem2
layout: pmdk
---

#### The libpmem2 library

**libpmem2** provides low-level persistent memory support.
In particular, support for the persistent memory instructions
for flushing changes to pmem is provided.

This library is provided for software that tracks every store
to pmem and needs to flush those changes to durability.  Most
developers will find higher level libraries like
[libpmemobj](../libpmemobj) to be much more convenient.

Man pages that contain a list of the **Linux** interfaces provided:

* Man page for <a href="../manpages/linux/master/libpmem2/{{ page.title }}.7.html">{{ page.title }} current master</a>

Man pages that contain a list of the **Windows** interfaces provided:

* Man page for <a href="../manpages/windows/master/libpmem2/{{ page.title }}.7.html">{{ page.title }} current master</a>

#### libpmem2 Example

**The Basics**

If you've decided to handle persistent memory allocation
and consistency across program interruption yourself, you will
find the functions in libpmem2 useful. It is important to
understand that programing to raw pmem means you must create
your own transactions or convince yourself you don't care if
a system or program crash leaves your pmem files in an inconsistent
state. Interfaces in the libpmem2 are **non-transactional**, but
can be used to build transactional interfaces like [libpmemobj](../libpmemobj).

To illustrate the basics, let's walk through the man page example first:

<code data-gist-id='wlemkows/c7dc06875ee4aa9857020eba01114e75' data-gist-file='basic.c' data-gist-line='8-18' data-gist-highlight-line='18' data-gist-hide-footer='true'></code>

The example starts, as shown above, by including the necessary
headers. Line 18 (the highlighted line) shows the header file
you need to include to use libpmem2: `libpmem2.h`.

For this simple example, we're going to write a string to a
memory-mapped file. First, we need to prepare a config and mapping
based on the source file.

<code data-gist-id='wlemkows/c7dc06875ee4aa9857020eba01114e75' data-gist-file='basic.c' data-gist-line='20-58' data-gist-highlight-line='39,44,49,55' data-gist-hide-footer='true'></code>

The lines above create the file, prepare config and source structures,
set required granularity and map the file into memory. 

This illustrates basic functions in the libpmem2:
First of them, `pmem2_config_new()`, creates config struct that
will be used for mapping. Config is an object with which we define
the parameters of the target mapping to be created.

In this example, we only set the granularity
in the config, other values remain default.
The granularity is the only required argument in the config.
In addition to the granularity setting, libpmem2 provides
multiple optional functions to configure target
mapping, eg. `pmem2_config_set_length()` to set length which will be used for
mapping, or `pmem2_config_set_offset` which will be used to map the contents
from the specified location of the source.

The second highlighted line contains a call to `pmem2_source_from_fd()`,
which takes a file descriptor and create a new instance of the source
structure that describes the data source used for mapping.
In this particular example, mapping source comes from a file descriptor,
however, libpmem2 also provides functions to use a source from the file handle
or create an annonymous map.

The next key step in this example is to set the granularity using
`pmem2_config_set_required_store_granularity`.
Granularity must be one of the three values: `PMEM2_GRANULARITY_BYTE`,
`PMEM2_GRANULARITY_CACHE_LINE`, `PMEM2_GRANULARITY_PAGE`.
In this case, we set a maximum permitted granularity to `PMEM2_GRANULARITY_PAGE`.
Logically, by setting the granularity to page, the application indicates that it's going to continue functioning even if the underlying device is block-based.

More detailed informations about granularity concept and each option
can be found  <a href="../manpages/linux/master/libpmem2/{{ page.title }}.7.html">here.</a>

The last step is to create a mapping using described above
config and source. Underlying function of `pmem2_map()` calls `mmap(2)`
on POSIX or `CreateFileMapping()` on Windows to memory map the entire file
concerning the permissible granularity.

When the mapping is created we are ready to write into it.

<code data-gist-id='wlemkows/c7dc06875ee4aa9857020eba01114e75' data-gist-file='basic.c' data-gist-line='60-63' data-gist-highlight-line='63' data-gist-hide-footer='true'></code>

Using the getters like `pmem2_map_get_size`, `pmem2_map_get_addres`
we can easy read information about created mapping.

The novel thing about pmem is you can copy to it directly, like any
memory.  The `strcpy()` call shown on line 63 above is just the usual
_libc_ function that stores a string to memory. If this example program
were to be interrupted either during or just after the `strcpy()` call,
you can't be sure which parts of the string made it all the way to the
media. It might be none of the string, all of the string, or somewhere
in-between. Also, there's no guarantee the string will make it to
the media in the order it was stored! For longer ranges, it is just as
likely that portions copied later make it to the media before earlier
portions. (So don't write code like the example above and then expect
to check for zeros to see how much of the string was written.)

How can a string get stored in a seemingly random order?
The reason is that until a flush function like `msync()`
has returned successfully, the normal cache pressure that happens
on an active system can push changes out to the media at any time
in any order. Most processors have barrier instructions (like
`SFENCE` on the Intel platform) but those instructions deal with
ordering in the visibility of stores to other threads, not with
the order that changes reach persistence.
The only barrier for flushing to persistence is function returned by
`pmem2_get_persist_fn()` as shown below.

<code data-gist-id='wlemkows/c7dc06875ee4aa9857020eba01114e75' data-gist-file='basic.c' data-gist-line='65-66' data-gist-highlight-line='65' data-gist-hide-footer='true'></code>

The libpmem2 function `pmem2_get_persist_fn` automatically decides what's
the most appropriate mechanism for flushing data onto the underlying storage.
This means that if the mapping does not support user-space flushing,
the persist function will fall back to using the OS primitives for synchronizing
data.

The `persist()` function above will, if possible,
perform the flush directly from user space, without calling into the
OS. This is made possible on the Intel platform using instructions like
`CLWB` and `CLFLUSHOPT` which are
[described in Intel's manuals](https://software.intel.com/sites/default/files/managed/0d/53/319433-022.pdf).
Of course, you are free to use these instructions directly in your
program, but the program will crash with an _undefined opcode_ if
you try to use the instructions on a platform that doesn't support
them. This is where libpmem2 helps you out, by checking the platform
capabilities on start-up and choosing the best instructions for each
operation it supports.

<code data-gist-id='wlemkows/c7dc06875ee4aa9857020eba01114e75' data-gist-file='basic.c' data-gist-line='68-71' data-gist-hide-footer='true'></code>

To avoid leaks in our example the last thing to do is unmap the mapping,
free source and config structures and close file descriptor.

Buildable source for the
[libpmem2 basic.c](https://github.com/pmem/pmdk/tree/master/src/examples/libpmem2)
example above is available in the PMDK repository.


**Copying to Persistent Memory**

Another feature of libpmem2 is a set of routines for optimally copying
to persistent memory.  These functions perform the same functions as
the _libc_ functions `memcpy()`, `memset()`, and `memmove()`, but they
are optimized for copying to pmem.  On the Intel platform, this is done
using the _non-temporal_ store instructions which bypass the processor
caches (eliminating the need to flush that portion of the data path).

Below example illustrates that the following code:

```c
        memset(dest, c, len);
        pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
        persist_fn(dest, len);
```

is functionally equivalent to:

```c
        pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
        memset_fn(dest, c, len);
```

The second part of the code above shows how `memset_fn()` is
used just like `memset(3)` except that libpmem2 handles flushing
the data to persistence as part of the set.

**Separating the Flush Steps**

There are two steps in flushing to persistence.  The first
step is to flush the processor caches, or bypass them entirely
as explained in the previous example. The second step is to
wait for any hardware buffers to drain, to ensure writes have
reached the media. These steps are performed together when function
returned by `pmem2_get_persist_fn()` is called:
```c
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	persist_fn(addr, len);
```
or they can be called individually by calling function from `pmem2_get_flush_fn()` for the
first step and function from `pmem2_get_drain_fn()` for the second:
```c
	pmem2_flush_fn flush_fn = pmem2_get_flush_fn(map);
	pmem2_drain_fn drain_fn = pmem2_get_drain_fn(map);

	flush_fn(addr, len);
	drain_fn();
```

Note that either of these steps may be
unnecessary on a given platform, and the library knows how
to check for that and do the right thing. For example, on
Intel platforms with eADR, `flusn_fn()` is an empty function.

When does it make sense to break flushing into steps?
For example, if a program is using multiple calls to `memcpy()`, it
can copy data and only performs the flush, postponing
the final drain step to the end. This works because unlike the flush
step, the drain step does not take an address range - it is a system-wide
drain operation so can happen at the end of the loop that copies
individual blocks of data.
