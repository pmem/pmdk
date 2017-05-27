---
layout: manual
Content-Style: 'text/css'
title: PMEMCTO_ERRORMSG(3)
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

[comment]: <> (pmemcto_errormsg.3 -- man page for libpmemcto)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)<br />


# NAME #

pmemcto_errormsg -- return string describing last error


# SYNOPSIS #

```c
#include <libpmemcto.h>

const char *pmemcto_errormsg(void);
```

# DESCRIPTION #

The **pmemcto_errormsg**() function returns a pointer to a static buffer
containing the last error message logged for current thread.
The error message may include description of the corresponding error code
(if *errno* was set), as returned by **strerror**(3).
The error message buffer is thread-local; errors encountered in one thread
do not affect its value in other threads.  The buffer is never cleared by any
library function; its content is significant only when the return value of
the immediately preceding call to **libpmemcto** function indicated an error,
or if *errno* was set.  The application must not modify or free the error
message string, but it may be modified by subsequent calls to other library
functions.


# RETURN VALUE #

The **pmemcto_errormsg**() function returns a pointer to a string that
describes the last error encountered by **libpmemcto** for current thread.


# ACKNOWLEDGEMENTS #

**libpmemcto** depends on jemalloc, written by Jason Evans, to do the heavy
lifting of managing dynamic memory allocation. See:
<http://www.canonware.com/jemalloc>

**libpmemcto** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**libpmemcto**(3), **errno**(3), **strerror**(3),
**jemalloc**(3) and **<http://pmem.io>**
