---
layout: manual
Content-Style: 'text/css'
title: _MP(PMEMBLK_CTL_GET, 3)
collection: libpmemblk
header: PMDK
date: pmemblk API version 1.1
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

[comment]: <> (pmemblk_ctl_get.3 -- man page for libpmemblk CTL)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[CTL NAMESPACE](#ctl-namespace)<br />
[CTL EXTERNAL CONFIGURATION](#ctl-external-configuration)<br />
[SEE ALSO](#see-also)<br />


# NAME #

_UW(pmemblk_ctl_get),
_UW(pmemblk_ctl_set),
_UW(pmemblk_ctl_exec)
-- Query and modify libpmemblk internal behavior (EXPERIMENTAL)


# SYNOPSIS #

```c
#include <libpmemblk.h>

_UWFUNCR2(int, pmemblk_ctl_get, PMEMblkpool *pbp, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
_UWFUNCR2(int, pmemblk_ctl_set, PMEMblkpool *pbp, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
_UWFUNCR2(int, pmemblk_ctl_exec, PMEMblkpool *pbp, *name, void *arg,
	=q= (EXPERIMENTAL)=e=)
```

_UNICODE()


# DESCRIPTION #

The _UW(pmemblk_ctl_get), _UW(pmemblk_ctl_set) and _UW(pmemblk_ctl_exec)
functions provide a uniform interface for querying and modifying the internal
behavior of **libpmemblk**(7) through the control (CTL) namespace.

See more in **ctl**(5) man page.


# CTL NAMESPACE #

prefault.at_create | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is created, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the _UW(pmemblk_create) function.

Always returns 0.

prefault.at_open | rw | global | int | int | - | boolean

If set, every page of the pool will be touched and written to when the pool
is opened, in order to trigger page allocation and minimize the performance
impact of pagefaults. Affects only the _UW(pmemblk_open) function.

Always returns 0.

# CTL EXTERNAL CONFIGURATION #

In addition to direct function call, each write entry point can also be set
using two alternative methods.

The first method is to load a configuration directly from the **PMEMBLK_CONF**
environment variable. A properly formatted ctl config string is a single-line
sequence of queries separated by ';':

```
query0;query1;...;queryN
```

A single query is constructed from the name of the ctl write entry point and
the argument, separated by '=':

```
entry_point=entry_point_argument
```

The entry point argument type is defined by the entry point itself, but there
are three predefined primitives:

	*) integer: represented by a sequence of [0-9] characters that form
		a single number.
	*) boolean: represented by a single character: y/n/Y/N/0/1, each
		corresponds to true or false. If the argument contains any
		trailing characters, they are ignored.
	*) string: a simple sequence of characters.

There are also complex argument types that are formed from the primitives
separated by a ',':

```
first_arg,second_arg
```

In summary, a full configuration sequence looks like this:

```
(first_entry_point)=(arguments, ...);...;(last_entry_point)=(arguments, ...);
```

As an example, to set both prefault at_open and at_create variables:
```

PMEMBLK_CONF="prefault.at_open=1;prefault.at_create=1"
```

The second method of loading an external configuration is to set the
**PMEMBLK_CONF_FILE** environment variable to point to a file that contains
a sequence of ctl queries. The parsing rules are all the same, but the file
can also contain white-spaces and comments.

To create a comment, simply use '#' anywhere in a line and everything
afterwards, until a new line, will be ignored.

An example configuration file:

```
#########################
# My pmemblk configuration
#########################
#
# Global settings:
prefault. # modify the behavior of pre-faulting
	at_open = 1; # prefault when the pool is opened

prefault.
	at_create = 0; # but don't prefault when it's created

# Per-pool settings:
# ...

```


# SEE ALSO #

**libpmemblk**(7), **pmemblk_ctl_exec**(3), **pmemblk_ctl_get**(3),
**pmemblk_ctl_set**(3) and **<http://pmem.io>**
