---
layout: manual
Content-Style: 'text/css'
title: PMEMLOG_TELL
collection: libpmemlog
header: PMDK
date: pmemlog API version 1.1
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

[comment]: <> (pmemlog_tell.3 -- man page for pmemlog_tell, pmemlog_rewind and pmemlog_walk functions)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemlog_tell**(), **pmemlog_rewind**(),
**pmemlog_walk**() - checks current write point for the log or walks through the log


# SYNOPSIS #

```c
#include <libpmemlog.h>

long long pmemlog_tell(PMEMlogpool *plp);
void pmemlog_rewind(PMEMlogpool *plp);
void pmemlog_walk(PMEMlogpool *plp, size_t chunksize,
	int (*process_chunk)(const void *buf, size_t len, void *arg),
	void *arg);
```

# DESCRIPTION #

The **pmemlog_tell**() function returns the current write point for the log,
expressed as a byte offset into the usable log space in the memory pool.
This offset starts off as zero on a newly-created log,
and is incremented by each successful append operation.
This function can be used to determine how much data is currently in the log.

The **pmemlog_rewind**() function resets the current write point for the log to zero.
After this call, the next append adds to the beginning of the log.

The **pmemlog_walk**() function walks through the log *plp*, from beginning to
end, calling the callback function *process_chunk* for each *chunksize* block
of data found. The argument *arg* is also passed to the callback to help
avoid the need for global state. The *chunksize* argument is useful for logs
with fixed-length records and may be specified as 0 to cause a single call
to the callback with the entire log contents passed as the *buf* argument. The
*len* argument tells the *process_chunk* function how much data *buf* is
holding. The callback function should return 1 if **pmemlog_walk**() should
continue walking through the log, or 0 to terminate the walk. The callback
function is called while holding **libpmemlog**(7) internal locks that make
calls atomic, so the callback function must not try to append to the log itself
or deadlock will occur.


# RETURN VALUE #

On success, **pmemlog_tell**() returns the current write point for the log.
On error, it returns -1 and sets *errno* appropriately.

The **pmemlog_rewind**() and **pmemlog_walk**() functions return no value.


# SEE ALSO #

**libpmemlog**(7) and **<http://pmem.io>**
