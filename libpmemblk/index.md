---
title: libpmemblk
layout: pmdk
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

Man pages that contains a list of the **Linux** interfaces provided:

* Man page for <a href="../manpages/linux/master/libpmemblk/{{ page.title }}.7.html">{{ page.title }} current master</a>


Man pages that contains a list of the **Windows** interfaces provided:

* Man page for <a href="../manpages/windows/master/libpmemblk/{{ page.title }}.7.html">{{ page.title }} current master</a>

#### libpmemblk Examples

**More Detail Coming Soon**

<code data-gist-id='andyrudoff/b3e569c479c3b7120875' data-gist-file='manpage.c' data-gist-line='37-96' data-gist-highlight-line='43' data-gist-hide-footer='true'></code>
