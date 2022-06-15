---
layout: manual
Content-Style: 'text/css'
title: _MP(FUTURE_CONTEXT_GET_SIZE, 3)
collection: miniasync
header: FUTURE_CONTEXT_GET_SIZE
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (future_context_get_size.3 -- man page for miniasync future_context_get_size operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**future_context_get_size**() - get the combined size of future data and output

# SYNOPSIS #

```c
#include <libminiasync.h>

struct future_context;
size_t future_context_get_size(struct future_context *context);
```

For general description of future API, see **miniasync_future**(7).

# DESCRIPTION #

The **future_context_get_size**() function reads the combined size of the data and output
structures from the future context *context*. Data structure type and output structure type
correspond respectively to the *\_data_type* and *\_output_type* parameters that are provided
to the **FUTURE(_name, _data_type, _output_type)** macro during future creation.

## RETURN VALUE ##

The **future_context_get_size**() function returns combined size of the future data and output
structures.

# SEE ALSO #

**miniasync**(7), **miniasync_future**(7) and **<https://pmem.io>**
