---
layout: manual
Content-Style: 'text/css'
title: _MP(RUNTIME_NEW, 3)
collection: miniasync
header: RUNTIME_NEW
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (runtime_new.3 -- man page for miniasync runtime_new operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**runtime_new**(), **runtime_delete**() - allocate or free runtime structure

# SYNOPSIS #

```c
#include <libminiasync.h>

struct runtime;

struct runtime *runtime_new(void);
void runtime_delete(struct runtime *runtime);
```

For general description of runtime API, see **miniasync_runtime**(7).

# DESCRIPTION #

The **runtime_new**() function allocates and initializes a new runtime structure.
Runtime can be used for optimized future polling.

The **runtime_delete**() function frees and finalizes the runtime structure pointed
by *runtime*.

## RETURN VALUE ##

The **runtime_new**() function returns a pointer to new *struct runtime* structure
or a *NULL* if the allocation or initialization of *struct runtime* failed.

The **runtime_delete**() function does not return any value.

# SEE ALSO #

**miniasync**(7), **miniasync_runtime**(3) and **<https://pmem.io>**
