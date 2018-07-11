---
layout: manual
Content-Style: 'text/css'
title: _MP(CTL, 5)
collection: ctl
header: PMDK
date: ctl API version 1.4
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

[comment]: <> (ctl.5 -- man page that describes format of pool set file)

[NAME](#name)<br />
[DESCRIPTION](#description)<br />
[SEE ALSO](#see-also)<br />


# NAME #

ctl -- interface for examination and modification of the library's internal state.


# DESCRIPTION #

The CTL namespace is organized in a tree structure. Starting from the root,
each node can be either internal, containing other elements, or a leaf.
Internal nodes themselves can only contain other nodes and cannot be entry
points. There are two types of those nodes: *named* and *indexed*. Named nodes
have string identifiers. Indexed nodes represent an abstract array index and
have an associated string identifier. The index itself is provided by the user.
A collection of indexes present on the path of an entry point is provided to
the handler functions as name and index pairs.

The *name* argument specifies an entry point as defined in the CTL namespace
specification. The entry point description specifies whether the extra *arg* is
required. Those two parameters together create a CTL query. The functions and the entry points are thread-safe unless
indicated otherwise below. If there are special conditions for calling an entry
point, they are explicitly stated in its description. The functions propagate
the return value of the entry point. If either *name* or *arg* is invalid, -1
is returned.

Entry points are the leaves of the CTL namespace structure. Each entry point
can read from the internal state, write to the internal state,
exec a function or a combination of these operations.

The entry points are listed in the following format:

name | r(ead)w(rite)x(ecute) | global/- | read argument type | write argument type | exec argument type | config argument type

A description of **ctl** functions can be found on the following
manual pages:
**libpmemblk_ctl_get**(3), **libpmemlog_ctl_get**(3), **libpmemobj_ctl_get**(3)

description...


# SEE ALSO #

**libpmemblk_ctl_get**(3), **libpmemlog_ctl_get**(3), **libpmemobj_ctl_get**(3)
and **<http://pmem.io>**
