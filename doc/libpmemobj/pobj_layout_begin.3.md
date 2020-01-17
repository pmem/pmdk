---
layout: manual
Content-Style: 'text/css'
title: _MP(POBJ_LAYOUT_BEGIN, 3)
collection: libpmemobj
header: PMDK
date: pmemobj API version 2.3
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

[comment]: <> (pobj_layout_begin.3 -- man page for declaration of pool's layout)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**POBJ_LAYOUT_BEGIN**(), **POBJ_LAYOUT_TOID**(),
**POBJ_LAYOUT_ROOT**(), **POBJ_LAYOUT_NAME**(),
**POBJ_LAYOUT_END**(), **POBJ_LAYOUT_TYPES_NUM**()
- persistent memory transactional object store layout

# SYNOPSIS #

```c
#include <libpmemobj.h>

POBJ_LAYOUT_BEGIN(layout)
POBJ_LAYOUT_TOID(layout, TYPE)
POBJ_LAYOUT_ROOT(layout, ROOT_TYPE)
POBJ_LAYOUT_NAME(layout)
POBJ_LAYOUT_END(layout)
POBJ_LAYOUT_TYPES_NUM(layout)
```

# DESCRIPTION #

**libpmemobj**(7) defines a set of macros for convenient declaration of a
pool's layout. The layout declaration consists of declarations of a number of
used types. The declared types will be assigned consecutive type numbers.
Declared types may be used in conjunction with type safety macros (see
**TOID_DECLARE**(3)). Once created, the layout declaration must not be changed
unless any new types are added at the end of the existing layout declaration.
Modifying any existing declaration may lead to changes in the type numbers of
declared types, which in consequence may cause data corruption.

The **POBJ_LAYOUT_BEGIN**() macro indicates a begin of declaration of layout.
The *LAYOUT* argument is a name of layout. This argument must be passed to all macros
related to the declaration of layout.

The **POBJ_LAYOUT_TOID**() macro declares a typed *OID* for type passed as *TYPE* argument
inside the declaration of layout. All types declared using this macro are assigned with
consecutive type numbers. This macro must be used between the **POBJ_LAYOUT_BEGIN**()
and **POBJ_LAYOUT_END**() macros, with the same name passed as *LAYOUT* argument.

The **POBJ_LAYOUT_ROOT**() macro declares a typed *OID* for type passed as *ROOT_TYPE*
argument inside the declaration of layout. The typed *OID* will be assigned with type number
for root object **POBJ_ROOT_TYPE_NUM**.

The **POBJ_LAYOUT_END**() macro ends the declaration of layout.

The **POBJ_LAYOUT_NAME**() macro returns the name of layout as a null-terminated string.

The **POBJ_LAYOUT_TYPES_NUM**() macro returns number of types declared using the **POBJ_LAYOUT_TOID**()
macro within the layout declaration.

# EXAMPLE #

This is an example of layout declaration:

```c
POBJ_LAYOUT_BEGIN(mylayout);
POBJ_LAYOUT_ROOT(mylayout, struct root);
POBJ_LAYOUT_TOID(mylayout, struct node);
POBJ_LAYOUT_TOID(mylayout, struct foo);
POBJ_LAYOUT_END(mylayout);

struct root
{
	TOID(struct node) node;
};

struct node
{
	TOID(struct node) next;
	TOID(struct foo) foo;
};
```

The name of layout and the number of declared types can be retrieved using the following code:

```c
const char *layout_name = POBJ_LAYOUT_NAME(mylayout);
int num_of_types = POBJ_LAYOUT_TYPES_NUM(mylayout);
```

# SEE ALSO #

**TOID_DECLARE**(3), **libpmemobj**(7) and **<https://pmem.io>**
