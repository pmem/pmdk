---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMLOG_CTL_GET, 3)
collection: libpmemlog
header: PMDK
date: pmemlog API version 1.1
...

[comment]: <> (Copyright 2018, Intel Corporation)

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

[comment]: <> (pmemlog_ctl_get.3 -- man page for libpmemlog CTL)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[CTL NAMESPACE](#ctl-namespace)<br />
[CTL EXTERNAL CONFIGURATION](#ctl-external-configuration)<br />
[SEE ALSO](#see-also)<br />


# NAME #

_UW(pmemlog_ctl_get),
_UW(pmemlog_ctl_set),
_UW(pmemlog_ctl_exec)
-- Query and modify libpmemlog internal behavior (EXPERIMENTAL)


# SYNOPSIS #

```c
#include <libpmemlog.h>

_UWFUNCR2(int, pmemlog_ctl_get, PMEMlogpool *plp, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
_UWFUNCR2(int, pmemlog_ctl_set, PMEMlogpool *plp, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
_UWFUNCR2(int, pmemlog_ctl_exec, PMEMlogpool *plp, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
```

_UNICODE()


# DESCRIPTION #

The _UW(pmemlog_ctl_get), _UW(pmemlog_ctl_set) and _UW(pmemlog_ctl_exec)
functions provide a uniform interface for querying and modifying the internal
behavior of **libpmemlog**(7) through the control (CTL) namespace.

See more in **pmem_ctl**(5) man page.


# CTL NAMESPACE #

prefault.at_create | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is created, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the _UW(pmemlog_create) function.

Always returns 0.

prefault.at_open | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is opened, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the _UW(pmemlog_open) function.

Always returns 0.

# CTL EXTERNAL CONFIGURATION #

In addition to direct function call, each write entry point can also be set
using two alternative methods.

The first method is to load a configuration directly from the **PMEMLOG_CONF**
environment variable.

The second method of loading an external configuration is to set the
**PMEMLOG_CONF_FILE** environment variable to point to a file that contains
a sequence of ctl queries.

See more in **pmem_ctl**(5) man page.

# SEE ALSO #

**libpmemlog**(7), **pmem_ctl**(5) and **<http://pmem.io>**
