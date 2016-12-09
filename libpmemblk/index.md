---
title: libpmemblk
layout: nvml
---

#### The libpmemblk library

**libpmemblk** implements a pmem-resident array of blocks,
all the same size, where a block is updated atomically with
respect to power failure or program interruption (no torn
blocks).

This library is provided for cases requiring large arrays
of objects at least 512 bytes each.  Most
developers will find higher level libraries like
[libpmemobj](../libpmemobj) to be more generally useful.

Man pages that contains a list of the interfaces provided:

* Man page for [libpmemblk current master](../manpages/master/libpmemblk.3.html)
* Latest releases:
   * [libpmemblk version 1.2](../manpages/v1.2/libpmemblk.3.html)
   * [libpmemblk version 1.1](../manpages/v1.1/libpmemblk.3.html)
   * [libpmemblk version 1.0](../manpages/v1.0/libpmemblk.3.html)

#### libpmemblk Examples

**More Detail Coming Soon**

<code data-gist-id='andyrudoff/b3e569c479c3b7120875' data-gist-file='manpage.c' data-gist-line='37-96' data-gist-highlight-line='43' data-gist-hide-footer='true'></code>
