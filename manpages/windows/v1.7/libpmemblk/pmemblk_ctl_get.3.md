---
layout: manual
Content-Style: 'text/css'
title: PMEMBLK_CTL_GET
collection: libpmemblk
header: PMDK
date: pmemblk API version 1.1
...

[comment]: <> (Copyright 2018-2019, Intel Corporation)

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

[comment]: <> (pmemblk_ctl_get.3 -- man page for libpmemblk CTL)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[CTL NAMESPACE](#ctl-namespace)<br />
[CTL EXTERNAL CONFIGURATION](#ctl-external-configuration)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmemblk_ctl_getU**()/**pmemblk_ctl_getW**(),
**pmemblk_ctl_setU**()/**pmemblk_ctl_setW**(),
**pmemblk_ctl_execU**()/**pmemblk_ctl_execW**()
- Query and modify libpmemblk internal behavior (EXPERIMENTAL)


# SYNOPSIS #

```c
#include <libpmemblk.h>

int pmemblk_ctl_getU(PMEMblkpool *pbp, const char *name, void *arg); (EXPERIMENTAL)
int pmemblk_ctl_getW(PMEMblkpool *pbp, const wchar_t *name, void *arg); (EXPERIMENTAL)
int pmemblk_ctl_setU(PMEMblkpool *pbp, const char *name, void *arg); (EXPERIMENTAL)
int pmemblk_ctl_setW(PMEMblkpool *pbp, const wchar_t *name, void *arg); (EXPERIMENTAL)
int pmemblk_ctl_execU(PMEMblkpool *pbp, const char *name, void *arg); (EXPERIMENTAL)
int pmemblk_ctl_execW(PMEMblkpool *pbp, const wchar_t *name, void *arg); (EXPERIMENTAL)
```


>NOTE: The PMDK API supports UNICODE. If the **PMDK_UTF8_API** macro is
defined, basic API functions are expanded to the UTF-8 API with postfix *U*.
Otherwise they are expanded to the UNICODE API with postfix *W*.


# DESCRIPTION #

The **pmemblk_ctl_getU**()/**pmemblk_ctl_getW**(), **pmemblk_ctl_setU**()/**pmemblk_ctl_setW**() and **pmemblk_ctl_execU**()/**pmemblk_ctl_execW**()
functions provide a uniform interface for querying and modifying the internal
behavior of **libpmemblk**(7) through the control (CTL) namespace.

The *name* argument specifies an entry point as defined in the CTL namespace
specification. The entry point description specifies whether the extra *arg* is
required. Those two parameters together create a CTL query. The functions and
the entry points are thread-safe unless
indicated otherwise below. If there are special conditions for calling an entry
point, they are explicitly stated in its description. The functions propagate
the return value of the entry point. If either *name* or *arg* is invalid, -1
is returned.

If the provided ctl query is valid, the CTL functions will always return 0
on success and -1 on failure, unless otherwise specified in the entry point
description.

See more in **pmem_ctl**(5) man page.


# CTL NAMESPACE #

prefault.at_create | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is created, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the **pmemblk_createU**()/**pmemblk_createW**() function.

Always returns 0.

prefault.at_open | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is opened, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the **pmemblk_openU**()/**pmemblk_openW**() function.

Always returns 0.

sds.at_create | rw | global | int | int | - | boolean

If set, force-enables or force-disables SDS feature during pool creation.
Affects only the **pmemblk_createU**()/**pmemblk_createW**() function. See **pmempool_feature_query**(3)
for information about SDS (SHUTDOWN_STATE) feature.

Always returns 0.

copy_on_write.at_open | rw | global | int | int | - | boolean

If set, pool is mapped in such a way that modifications don't reach the
underlying medium. From the user's perspective this means that when the pool
is closed all changes are reverted. This feature is not supported for pools
located on Device DAX.

Always returns 0.

# CTL EXTERNAL CONFIGURATION #

In addition to direct function call, each write entry point can also be set
using two alternative methods.

The first method is to load a configuration directly from the **PMEMBLK_CONF**
environment variable.

The second method of loading an external configuration is to set the
**PMEMBLK_CONF_FILE** environment variable to point to a file that contains
a sequence of ctl queries.

See more in **pmem_ctl**(5) man page.


# SEE ALSO #

**libpmemblk**(7), **pmem_ctl**(5) and **<http://pmem.io>**
