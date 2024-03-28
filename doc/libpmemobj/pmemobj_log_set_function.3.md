---
draft: false
slider_enable: true
description: ""
disclaimer: "The contents of this web site and the associated <a href=\"https://github.com/pmem\">GitHub repositories</a> are BSD-licensed open source."
aliases: ["pmemobj_log_set_function.3.html"]
title: "libpmemobj | PMDK"
header: "pmemobj API version 2.3"
---

[comment]: <> (SPDX-License-Identifier: BSD-3-Clause)
[comment]: <> (Copyright 2024, Intel Corporation)

[comment]: <> (pmemobj_log_set_function.3 -- set the logging function)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[THE DEFAULT LOGGING FUNCTION](#the-default-logging-function)<br />
[RETURN VALUE](#return-value)<br />
[ERRORS](#errors)<br />
[NOTE](#note)<br />
[SEE ALSO](#see-also)<br />

# NAME #

**pmemobj_log_set_function**() - set the logging function

# SYNOPSIS #

```c
	#include <libpmemobj.h>

	typedef void pmemobj_log_function(
		enum pmemobj_log_level level,
		const char *file_name,
		unsigned line_no,
		const char *function_name,
		const char *message);

	int pmemobj_log_set_function(pmemobj_log_function *log_function);
```

# DESCRIPTION #

**pmemobj_log_set_function**() allows choosing the function which will get all
the generated logging messages. The *log_function* can be either
**PMEMOBJ_LOG_USE_DEFAULT_FUNCTION** which will use the default logging function
(built into the library) or a pointer to a user-defined function.

The parameters of a user-defined log function are as follows:

 - *level* - the log level of the message (see **pmemobj_log_set_threshold**(3)
   for available log levels)
 - *file_name* - name of the source file where the message is coming from.
   It could be set to NULL and in such case, neither *line_no* nor *function_name*
   are provided.
 - *line_no* - the source file line where the message is coming from
 - *function_name* - the function name where the message is coming from
 - *message* - the message string

# THE DEFAULT LOGGING FUNCTION #

The library provides the default logging function which writes messages to
**syslog**(3) and to **stderr**(3). This function is enabled during library
initialization and can be restored using **PMEMOBJ_LOG_USE_DEFAULT_FUNCTION** value
as the *log_function* argument.

The default logging function sinks all the logging messages into **syslog**(3)
unconditionally. Additionally, it sinks a logging message into **stderr**(3)
if its logging level is not less severe than indicated by
the **PMEMOBJ_LOG_THRESHOLD_AUX** threshold's value and it is not of level
**PMEMOBJ_LOG_LEVEL_HARK**.

Note that the **PMEMOBJ_LOG_THRESHOLD** threshold's value applies to
the default logging function. Please see the notes below.

# RETURN VALUE #

**pmemobj_log_set_function**() function returns 0 on success or returns
a non-zero value and sets errno on failure. On failure, the logging
function remains unchanged.

# ERRORS #

**pmemobj_log_set_function**() can set the following errno values on fail:

 - EAGAIN - multiple threads attempted to change the logging function concurrently.
   A retry may fix the problem. This error is not expected when the function is
   called from just one thread at a time.

# NOTE #

The logging messages of level less severe than indicated by
the **PMEMOBJ_LOG_THRESHOLD** threshold's value won't make it to the logging function
no matter whether the logging function is the default logging function or user-provided.

The user-defined function must be thread-safe.

The library will call user defined function before returning from **pmemobj_log_set_function**()
to deliver basic information about the library:

 - "src version: 2.y.z"
 - "compiled with support for shutdown state"
 - "compiled with libndctl 63+"

# SEE ALSO #

**pmemobj_log_get_threshold**(3), **pmemobj_log_set_threshold**(3).
