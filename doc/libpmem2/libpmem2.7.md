---
layout: manual
Content-Style: 'text/css'
title: _MP(LIBPMEM2, 7)
collection: libpmem2
header: PMDK
date: pmem2 API version 1.0
...

[comment]: <> (Copyright 2019, Intel Corporation)

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

[comment]: <> (libpmem2.7 -- man page for libpmem2)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[CAVEATS](#caveats)<br />
[ENVIRONMENT](#environment)<br />
[DEBUGGING](#debugging)<br />
[EXAMPLE](#example)<br />
[ACKNOWLEDGEMENTS](#acknowledgements)<br />
[SEE ALSO](#see-also)


# NAME #

**libpmem2** - persistent memory support library (EXPERIMENTAL)


# SYNOPSIS #

```c
#include <libpmem2.h>
cc ... -lpmem2
```


# DESCRIPTION #

# CAVEATS #

# ENVIRONMENT #


# DEBUGGING #

Two versions of **libpmem2** are typically available on a development
system. The normal version, accessed when a program is linked using the
**-lpmem2** option, is optimized for performance. That version skips checks
that impact performance and never logs any trace information or performs any
run-time assertions.

A second version of **libpmem2**, accessed when a program uses the libraries
under _DEBUGLIBPATH(), contains run-time assertions and trace points. The
typical way to access the debug version is to set the environment variable
**LD_LIBRARY_PATH** to _LDLIBPATH(). Debugging output is
controlled using the following environment variables. These variables have
no effect on the non-debug version of the library.

+ **PMEM2_LOG_LEVEL**

The value of **PMEM2_LOG_LEVEL** enables trace points in the debug version
of the library, as follows:

+ **0** - This is the default level when **PMEM2_LOG_LEVEL** is not set.
No log messages are emitted at this level.

+ **1** - Additional details on any errors detected are logged, in addition
to returning the *errno*-based errors as usual. The same information
may be retrieved using _UW(pmem2_errormsg).

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing in the
library.

+ **4** - Enables voluminous and fairly obscure tracing
information that is likely only useful to the **libpmem2** developers.

Unless **PMEM2_LOG_FILE** is set, debugging output is written to *stderr*.

+ **PMEM2_LOG_FILE**

Specifies the name of a file where
all logging information should be written. If the last character in the name
is "-", the *PID* of the current process will be appended to the file name when
the log file is created. If **PMEM2_LOG_FILE** is not set, output is
written to *stderr*.


# EXAMPLE #

# ACKNOWLEDGEMENTS #

**libpmem2** builds on the persistent memory programming model recommended
by the SNIA NVM Programming Technical Work Group:
<http://snia.org/nvmp>


# SEE ALSO #

**libpmem**(7), **libpmemblk**(7), **libpmemlog**(7), **libpmemobj**(7)
and **<http://pmem.io>**
