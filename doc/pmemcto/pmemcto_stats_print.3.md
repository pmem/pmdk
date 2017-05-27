---
layout: manual
Content-Style: 'text/css'
title: PMEMCTO_STATS_PRINT(3)
header: NVM Library
date: libpmemcto API version 1.0
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

[comment]: <> (pmemcto_stats_print.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

pmemcto_stats_print -- write human-readable memory pool statistics


# SYNOPSIS #

```c
#include <libpmemcto.h>

void pmemcto_stats_print(PMEMctopool *pcp, const char *opts);

```

# DESCRIPTION #

The **pmemcto_stats_print**() function produces messages containing statistics
about the given memory pool.  The output is printed using **libpmemcto**'s
internal *print_func* function (see **pmemcto_set_funcs**(3)).  That means
the output typically appears on *stderr* unless the caller supplies
a replacement *print_func* or sets the environment variable
**PMEMCTO_LOG_FILE** to direct output elsewhere.  The *opts* string can either
be NULL or it can contain a list of options that change the stats printed.
General information that never changes during execution can be omitted
by specifying "g" as a character within the opts string.  The characters
"m" and "a" can be specified to omit merged arena and per arena statistics,
respectively; "b" and "l" can be specified to omit per size class statistics
for bins and large objects, respectively.  Unrecognized characters are silently
ignored.  Note that thread caching may prevent some statistics from being
completely up to date.
See **jemalloc**(3) for more detail (the description of the available *opts*
above was taken from that man page).


# ACKNOWLEDGEMENTS #

**libpmemcto** depends on jemalloc, written by Jason Evans, to do the heavy
lifting of managing dynamic memory allocation. See:
<http://www.canonware.com/jemalloc>

**libpmemcto** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**libpmemcto**(3), **jemalloc**(3) and **<http://pmem.io>**
