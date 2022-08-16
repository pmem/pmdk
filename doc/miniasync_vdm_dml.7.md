---
layout: manual
Content-Style: 'text/css'
title: _MP(MINIASYNC_VDM_DML, 7)
collection: miniasync_dml
header: MINIASYNC_VDM_DML
secondary_title: miniasync_dml
...

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2022, Intel Corporation)

[comment]: <> (miniasync_vdm_dml.7 -- man page for miniasync-vdm-dml API)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**miniasync_vdm_dml** - **DML** implementation of **miniasync**(7) virtual data mover

# SYNOPSIS #

```c
#include <libminiasync.h>
#include <libminiasync-vdm-dml.h>
```

For general description of virtual data mover API, see **miniasync**(7).

# DESCRIPTION #

**DML** data mover is an implementation of the virtual data mover based on
the *Data Mover Library*(**DML**). Every **DML** data mover operation
executes under the control of **DML**.

**DML** data mover is separated from the primary **miniasync**(7) library. It's encapsulated
by the **miniasync-vdm-dml**(7) library that has dependency on the **DML** library.
For information about **miniasync_vdm_dml**() library compilation, see *extras/dml/README.md* file.

**DML** data mover supports offloading certain computations to the hardware
accelerators (e.g. Intel® Data Streaming Accelerator). To use this feature, make
sure that **DML** library is installed with **DML_HW** option.
For more information about **DML**, see **<https://github.com/intel/DML>**.
An example of **DML** data mover API usage with flags can be found in **EXAMPLE** section.

When the future is polled for the first time the data mover operation will be executed
asynchronously under the control of **DML** library. **DML** data mover does not
block the calling thread.

To create a new **DML** data mover instance, use **data_mover_dml_new**(3) function.

**DML** data mover provides the following flags:

* **VDM_F_MEM_DURABLE** - write to destination is identified as write to durable memory

**DML** data mover supports following operations:

* **vdm_memcpy**(3) - memory copy operation
* **vdm_memmove**(3) - memory move operation
* **vdm_memset**(3) - memory set operation
* **vdm_flush**(3) - cache flush operation

**DML** data mover does not support notifier feature. For more information about
notifiers, see **miniasync_future**(7).

# EXAMPLE #

Example usage of **DML** data mover **vdm_memcpy**(3) operation with
**MINIASYNC_DML_F_MEM_DURABLE** flag:
```c
struct data_mover_dml *dmd = data_mover_dml_new();
struct vdm *dml_mover = data_mover_dml_get_vdm(dmd);
struct vdm_memcpy_future memcpy_fut = vdm_memcpy(dml_mover, dest, src,
		copy_size, MINIASYNC_DML_F_MEM_DURABLE);
```

# SEE ALSO #

**data_mover_dml_new**(3), **data_mover_dml_get_vdm**(3),
**vdm_flush**(3), **vdm_memcpy**(3), **vdm_memmove**(3), **vdm_memset**(3),
**miniasync**(7), **miniasync_future**(7), **miniasync_vdm**(7),
**<https://github.com/intel/DML>** and **<https://pmem.io>**
