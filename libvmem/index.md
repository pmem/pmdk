---
title: libvmem
layout: pmdk
---

#### The libvmem library

**libvmem** supports the traditional _malloc_/_free_
interfaces on a memory mapped file.  This allows the
use of persistent memory as volatile memory, for cases
where the pool of persistent memory is useful to an
application, but when the application doesn't need
it to be persistent.

>NOTE:
Since persistent memory support
has been integrated into [libmemkind](https://github.com/memkind/memkind),
that library is the **recommended** choice for any new volatile usages,
since it combines support for multiple types of volatile memory into
a single, convenient API.

Man pages that contains a list of the **Linux** interfaces provided:

* Man page for <a href="../../vmem/manpages/linux/master/libvmem/{{ page.title }}.7.html">{{ page.title }} current master</a>


Man pages that contains a list of the **Windows** interfaces provided:

* Man page for <a href="../../vmem/manpages/windows/master/libvmem/{{ page.title }}.7.html">{{ page.title }} current master</a>

#### libvmem Examples

**More Detail Coming Soon**

<code data-gist-id='andyrudoff/02a10ca6b9ab7d07922b' data-gist-file='manpage.c' data-gist-line='37-66' data-gist-highlight-line='40' data-gist-hide-footer='true'></code>
