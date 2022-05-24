---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["libpmemset.7.html"]
title: "libpmemset | PMDK"
header: "pmemset API version 1.0"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2020, Intel Corporation)

[comment]: <> (libpmemset.7 -- man page for libpmemset)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[DEBUGGING](#debugging)<br />
[SEE ALSO](#see-also)

# NAME #

**libpmemset** - provides core functionality any persistent application should reasonably have (EXPERIMENTAL)

# SYNOPSIS #

```c
#include <libpmemset.h>
cc ... -lpmemset -lpmem2
```

# DESCRIPTION #

**libpmemset** is still in progress.

# DEBUGGING #

+ **PMEMSET_LOG_LEVEL**

Value assigned to the **PMEMSET_LOG_LEVEL** controls the level of log details presented
in the debug version of the library, as follows:

+ **0** - This is the default level of logging, when **PMEMSET_LOG_LEVEL** is not set.
No log messages are presented at this level.

+ **1** - Additional details on any errors detected are logged, in addition
to returning the *errno*-based errors as usual. The same information
may be retrieved using _UW(pmemset_errormsg).

+ **2** - A trace of basic operations is logged.

+ **3** - Enables a very verbose amount of function call tracing in the
library.

+ **4** - Enables voluminous and fairly obscure tracing
information that is likely only useful to the **libpmemset** developers.

Unless **PMEMSET_LOG_FILE** is set, debugging output is written to *stderr*.

+ **PMEMSET_LOG_FILE**

Specifies the name of a file where all logging information should be written.
If **PMEMSET_LOG_FILE** is not set, output is written to *stderr*.

# SEE ALSO #

**<https://pmem.io>**
