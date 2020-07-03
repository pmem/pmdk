---
title: libpmem
layout: pmdk
---

#### The libpmem library

**libpmem** provides low level persistent memory support.
In particular, support for the persistent memory instructions
for flushing changes to pmem is provided.

This library is provided for software which tracks every store
to pmem and needs to flush those changes to durability.  Most
developers will find higher level libraries like
[libpmemobj](../libpmemobj) to be much more convenient.

Man pages that contains a list of the **Linux** interfaces provided:

* Man page for <a href="../manpages/linux/master/libpmem/{{ page.title }}.7.html">{{ page.title }} current master</a>

Man pages that contains a list of the **Windows** interfaces provided:

* Man page for <a href="../manpages/windows/master/libpmem/{{ page.title }}.7.html">{{ page.title }} current master</a>

#### libpmem Examples

**The Basics**

If you've decided to handle persistent memory allocation
and consistency across program interruption yourself, you will
find the functions in libpmem useful.  It is important to
understand that programming to raw pmem means you must create
your own transactions or convince yourself you don't care if
a system or program crash leaves your pmem files in an inconsistent
state.  Libraries like [libpmemobj](../libpmemobj) provide transactional
interfaces by building on these libpmem functions, but the interfaces
in libpmem are **non-transactional**.

To illustrate the basics, let's walk through the man page example first:

<code data-gist-id='andyrudoff/15bda69da7fe77e8469b' data-gist-file='manpage.c' data-gist-line='37-45' data-gist-highlight-line='45' data-gist-hide-footer='true'></code>

The example starts, as shown above, by including the necessary
headers.  Line 45 (the highlighted line) shows the header file
you need to include to use libpmem: `libpmem.h`.

<code data-gist-id='andyrudoff/15bda69da7fe77e8469b' data-gist-file='manpage.c' data-gist-line='47-48' data-gist-highlight-line='48' data-gist-hide-footer='true'></code>

For this simple example, we're just going to hard code a pmem file
size of 4 kilobytes.

<code data-gist-id='andyrudoff/15bda69da7fe77e8469b' data-gist-file='manpage.c' data-gist-line='50-74' data-gist-highlight-line='70' data-gist-hide-footer='true'></code>

The lines above create the file, make sure 4k is allocated, and
map the file into memory.  This illustrates one of the helper
functions in libpmem: `pmem_map()` which takes a file descriptor
and calls `mmap(2)` to memory map the entire file.  Calling `mmap()`
directly will work just fine -- the main advantage of `pmem_map()`
is that it tries to find an address where mapping is likely to use
large page mappings, for better performance when using large ranges
of pmem.

Note that once the pmem file is mapped into memory, it is not
necessary to keep the file descriptor open.

Since the system calls for memory mapping persistent memory
are the same as the POSIX calls for memory mapping any file,
you may want to write your code to run correctly when given
either a pmem file or a file on a traditional file system.
For many decades it has been the case that changes written
to a memory mapped range of a file may not be persistent until
flushed to the media.  One common way to do this is using
the POSIX call `msync(2)`.  If you write your program to memory
map a file and use `msync()` every time you want to flush the
changes media, it will work correctly for pmem as well as files
on a traditional file system.  However, you may find your program
performs better if you detect pmem explicitly and use libpmem
to flush changes in that case.

<code data-gist-id='andyrudoff/15bda69da7fe77e8469b' data-gist-file='manpage.c' data-gist-line='76-77' data-gist-highlight-line='77' data-gist-hide-footer='true'></code>

The libpmem function `pmem_is_pmem()` can be used to determine
if the memory in the given range is really persistent memory or if
it is just a memory mapped file on a traditional file system.  Using
this call in your program will allow you to decide what to do when
given a non-pmem file.  Your program could decide to print an error
message and exit (for example: "ERROR: This program only works on pmem").
But it seems more likely you will want to save the result of
`pmem_is_pmem()` as shown above, and then use that flag to decide
what to do when flushing changes to persistence as later in
this example program.

<code data-gist-id='andyrudoff/15bda69da7fe77e8469b' data-gist-file='manpage.c' data-gist-line='79-80' data-gist-highlight-line='80' data-gist-hide-footer='true'></code>

The novel thing about pmem is you can copy to it directly, like any
memory.  The `strcpy()` call shown on line 80 above is just the usual
_libc_ function that stores a string to memory.  If this example program
were to be interrupted either during or just after the `strcpy()` call,
you can't be sure which parts of the string made it all the way to the
media.  It might be none of the string, all of the string, or somewhere
in-between.  In addition, there's no guarantee the string will make it to
the media in the order it was stored!  For longer ranges, it is just as
likely that portions copied later make it to the media before earlier
portions.  (So don't write code like the example above and then expect
to check for zeros to see how much of the string was written.)

How can a string get stored in seemingly random order?
The reason is that until a flush function like `msync()`
has returned successfully, the normal cache pressure that happens
on an active system can push changes out to the media at any time
in any order.  Most processors have barrier instructions (like
`SFENCE` on the Intel platform) but those instructions deal with
ordering in the visibility of stores to other threads, not with
the order that changes reach persistence.  The only barriers for
flushing to persistence are functions like `msync()` or `pmem_persist()`
as shown below.

<code data-gist-id='andyrudoff/15bda69da7fe77e8469b' data-gist-file='manpage.c' data-gist-line='82-86' data-gist-highlight-line='84' data-gist-hide-footer='true'></code>

As shown above, this example uses the `is_pmem` flag it saved from the
previous call to `pmem_is_pmem()`.  This is the recommended way to
use this information rather than calling `pmem_is_pmem()` each time
you want to make changes durable.  That's because `pmem_is_pmem()`
can have a high overhead, having to search through data structures to
ensure the entire range is really persistent memory.

For true pmem, the highlighted line 84 above is the most optimal way
to flush changes to persistence.  `pmem_persist()` will, if possible,
perform the flush directly from user space, without calling into the
OS.  This is made possible on the Intel platform using instructions like
`CLWB` and `CLFLUSHOPT` which are
[described in Intel's manuals](https://software.intel.com/sites/default/files/managed/0d/53/319433-022.pdf).
Of course you are free to use these instructions directly in your
program, but the program will crash with an _undefined opcode_ if
you try to use the instructions on a platform that doesn't support
them.  This is where libpmem helps you out, by checking the platform
capabilities on start-up and choosing the best instructions for each
operation it supports.

The above example also uses `pmem_msync()` for the non-pmem case
instead of calling `msync(2)` directly.  For convenience, the
`pmem_msync()` call is a small wrapper around `msync()` that ensures
the arguments are aligned, as requirement of POSIX.

Buildable source for the
[libpmem manpage.c](https://github.com/pmem/pmdk/tree/master/src/examples/libpmem)
example above is available in the PMDK repository.

**Copying to Persistent Memory**

Another feature of libpmem is a set of routines for optimally copying
to persistent memory.  These functions perform the same functions as
the _libc_ functions `memcpy()`, `memset()`, and `memmove()`, but they
are optimized for copying to pmem.  On the Intel platform, this is done
using the _non-temporal_ store instructions which bypass the processor
caches (eliminating the need to flush that portion of the data path).

The first copy example, called *simple_copy*, illustrates how
`pmem_memcpy()` is used.

<code data-gist-id='andyrudoff/15bda69da7fe77e8469b' data-gist-file='simple_copy.c' data-gist-line='97-109' data-gist-highlight-line='105' data-gist-hide-footer='true'></code>

The highlighted line, line 105 above, shows how `pmem_memcpy()` is
used just like `memcpy(3)` except that when the destination is pmem,
libpmem handles flushing the data to persistence as part of the copy.

Buildable source for the
[libpmem simple_copy.c](https://github.com/pmem/pmdk/tree/master/src/examples/libpmem)
example above is available in the PMDK repository.

**Separating the Flush Steps**

There are two steps in flushing to persistence.  The first
step is to flush the processor caches, or bypass them entirely
as explained in the previous example.  The second step is to
wait for any hardware buffers to drain, to ensure writes have
reached the media.  These steps are performed together when
`pmem_persist()` is called, or they can be called individually
by calling `pmem_flush()` for the first step and `pmem_drain()`
for the second.  Note that either of these steps may be
unnecessary on a given platform, and the library knows how
to check for that and do the right thing.  For example, on
Intel platforms with eADR, `pmem_flush()` is an empty function.

When does it make sense to break flushing into steps?  This example,
called *full_copy* illustrates one reason you might do this.  Since
the example copies data using multiple calls to `memcpy()`, it
uses the version of libpmem copy that only performs the flush, postponing
the final drain step to the end.  This works because unlike the flush
step, the drain step does not take an address range -- it is a system-wide
drain operation so can happen at the end of the loop that copies
individual blocks of data.

<code data-gist-id='andyrudoff/15bda69da7fe77e8469b' data-gist-file='full_copy.c' data-gist-line='54-76' data-gist-highlight-line='65,75' data-gist-hide-footer='true'></code>

As each block is copied, line 65 in the above example copies a block of
data to pmem, effectively flushing it from the processor caches.  But
rather than waiting for the hardware queues to drain each time, that
step is saved until the end, as shown on line 75 above.

Buildable source for the
[libpmem full_copy.c](https://github.com/pmem/pmdk/tree/master/src/examples/libpmem)
example above is available in the PMDK repository.
