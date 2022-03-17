---
layout: manual
Content-Style: 'text/css'
title: _MP(DATA_MOVER_DML_NEW, 3)
collection: miniasync
header: DATA_MOVER_DML_NEW
secondary_title: miniasync
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (data_mover_dml_new.3 -- man page for miniasync data_mover_dml_new operation)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[RETURN VALUE](#return-value)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**data_mover_dml_new**(), **data_mover_dml_delete**() - allocate or free **DML**
data mover structure

# SYNOPSIS #

```c
#include <libminiasync.h>

struct data_mover_dml;

struct data_mover_dml *data_mover_dml_new(void);
void data_mover_dml_delete(struct data_mover_dml *dmd);
```

For general description of **DML** data mover API, see **miniasync_vdm_dml**(7).

# DESCRIPTION #

The **data_mover_dml_new**() function allocates and initializes a new **DML** data mover structure.

The **data_mover_dml_delete**() function frees and finalizes the **DML** data mover structure
pointed by *dmd*.

# RETURN VALUE #

The **data_mover_dml_new**() function returns a pointer to *struct data_mover_dml* structure or
**NULL** if the allocation or initialization failed.

The **data_mover_dml_delete**() function does not return any value.

# SEE ALSO #

**miniasync**(7), **miniasync_vdm_dml**(7),
**<https://github.com/intel/DML>** and **<https://pmem.io>**
