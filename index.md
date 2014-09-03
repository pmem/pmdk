---
title: NVM Library
layout: nvml
---

The NVM Library is a library for using memory-mapped persistence,
optimized specifically for _persistent memory_.  The source is in this
[GitHub repository](https://github.com/pmem/nvml/).

**Note: The NVM Library is still under development and is not
yet ready for production use.**

The NVM Library is current delivered as two actual libraries:

#### libpmem

The **libpmem** library provides persistent memory aware APIs for
some common tasks.  The APIs cover these areas:
* pmem basics: identifying pmem and APIs to flush it
* pmem transactions: **API still being designed -- stay tuned**
* pmem block: support arrays of atomically writable blocks
* pmem log: support pmem-resident log files

See the [man page](libpmem.3.html) for the current API descriptions.

#### libvmem

The **libvmem** library turns a pool of persistent memory into a
volatile memory pool, similar to the system heap but kept separate
and with its own malloc-style API.

See the [man page](libvmem.3.html) for details.

#### More coming soon...
