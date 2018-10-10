---
layout: manual
Content-Style: 'text/css'
title: PMREORDER
collection: pmreorder
header: PMDK
date: pmreorder version 1.5
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

[comment]: <> (pmreorder.1 -- man page for pmreorder)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[OPTIONS](#options)<br />
[ENGINES](#engines)<br />
[INSTRUMENTATION](#instrumentation)<br />
[PMEMCHECK STORE LOG](#pmemcheck-store-log)<br />
[ENVIRONMENT](#environment)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmreorder** - performs a persistent consistency check
		 using a store reordering mechanism


# SYNOPSIS #

```
$ python pmreorder <options>
```


# DESCRIPTION #

The pmreorder tool is a collection of python scripts designed
to parse and replay operations logged by pmemcheck -
a persistent memory checking tool.

Pmreorder performs the store reordering between persistent
memory barriers - a sequence of flush-fence operations.
It uses a consistency checking routine provided in the
command line options to check whether files are in a
consistent state.

Considering that logging, replaying and reordering of operations
are very time consuming, it is recommended to use as few stores as
possible in test workloads.


# OPTIONS #

`-h, --help`

Prints synopsis and list of options.

`-l <store_log>, --logfile <store_log>`

The pmemcheck log file to process.

`-c <prog|lib>, --checker <prog|lib>`

Consistency checker type.

`-p <path>, --path <path>`

Path to the consistency checker. Checker function has to return 0 for consistent cases and 1 otherwise.

`-n <name>, --name <name>`

The symbol name of the consistency checking function
in the library. Valid only if the checker type is `lib`.

`-o <pmreorder_output>, --output <pmreorder_output>`

Set the logger output file.

`-e <debug|info|warning|error|critical>,
 --output-level <debug|info|warning|error|critical>`

Set the output log level.

`-r  <NoReorderNoCheck|
      NoReorderDoCheck|
      ReorderFull|
      ReorderPartial|
      ReorderAccumulative|
      ReorderReverseAccumulative>,
 --default-engine  <NoReorderNoCheck|
		    NoReorderDoCheck|
		    ReorderFull|
		    ReorderPartial|
		    ReorderAccumulative|
		    ReorderReverseAccumulative>`

Set the initial reorder engine. Default value is `NoReorderNoCheck`.

`-x <cli_macros|config_file>, --extended-macros <cli_macros|config_file>`

Assign an engine types to the defined marker.


# ENGINES #

By default, the **NoReorderNoCheck** engine is used,
which means that for each set of stores, the tool
will pass-through all sequences of stores not reordered
and will not run consistency checker on them.

To enable different types of the reorder engine and
begin proper reordering tests, a number of other
engines exist:

+ **NoReorderDoCheck** - pass-through of unchanged operations.
Checks correctness of the stores as they were logged.
Useful for operations that do not require fail safety.

```
Example:
        input: (a, b, c)
        output: (a, b, c)
```

+ **ReorderAccumulative** - checks correctness on a growing
subset of the original sequence.

```
Example:
        input: (a, b, c)
        output:
               ()
               (a)
               (a, b)
               (a, b, c)
```

+ **ReorderReverseAccumulative** - checks correctness on a reverted growing
subset of the original sequence.

```
Example:
        input: (a, b, c)
        output:
               ()
               (c)
               (c, b)
               (c, b, a)
```

+ **ReorderPartial** - checks consistency on 3 randomly selected sequences
from a set of 1000 combinations of the original log, without repetitions.

```
 Example:
         input: (a, b, c)
         output:
                (b, c)
                (b)
                (a, b, c)
```

+ **ReorderFull** - for each set of stores generates and checks consistency
of all possible store permutations.
This might prove to be very computationally expensive for most workloads.
It can be useful for critical sections of code with limited number of stores.

```
 Example:
        input: (a, b, c)
        output:
               ()
               (a)
               (b)
               (c)
               (a, b)
               (a, c)
               (b, a)
               (b, c)
               (c, a)
               (c, b)
               (a, b, c)
               (a, c, b)
               (b, a, c)
               (b, c, a)
               (c, a, b)
               (c, b, a)
```

When the engine is passed with an `-r` option, it will be used
for each logged set of stores.
Additionally, the `-x` parameter can be used to switch engines
separately for any marked code sections.
For more details about `-x` extended macros functionality see section
INSTRUMENTATION below.


# INSTRUMENTATION #

The core of **pmreorder** is based on user-provided named markers.
Sections of code can be 'marked' depending on their importance,
and the degree of reordering can be customized by the use of various
provided engines.

For this purpose, Valgrind's pmemcheck tool exposes a
generic marker macro:

+ **VALGRIND_EMIT_LOG(value)**

It emits log to *store_log* during pmemcheck processing.
*value* is a user-defined marker name.
For more details about pmemcheck execution see
PMEMCHECK STORE LOG section below.

Example:
```
main.c
.
.
.
VALGRIND_EMIT_LOG("PMREORDER_MEMSET_PERSIST.BEGIN");

pmem_memset_persist(...);

VALGRIND_EMIT_LOG("PMREORDER_MEMSET_PERSIST.END");
.
.
.
```

There are a few rules for macros creation:

+ Valid macro can have any name,
but begin and end section have to match -
they are case sensitive.
+ Macro must have `.BEGIN` or `.END` suffix.
+ Macros can't be crossed.

Defined markers can be assigned engines types and configured
through the **pmreorder** tool using the `-x` parameter.

There are two ways to set macro options:

+ Using command line interface in format:
```PMREORDER_MARKER_NAME1=ReorderName1,PMREORDER_MARKER_NAME2=ReorderName2```

+ Using configutation file in .json format:
```
{
    "PMREORDER_MARKER_NAME1"="ReorderName1",
    "PMREORDER_MARKER_NAME2"="ReorderName2"
}
```

For more details about available
engines types, see ENGINES section above.

**libpmemobj**(7) also provides set of macros that allows change
reordering engine on library or function level:

`<library_name|api_function_name>`

Example of configutation on function level:
```
{
    "pmemobj_open"="NoReorderNoCheck",
    "pmemobj_memcpy_persist"="ReorderPartial"
}
```

Example of configutation on library level
(affecting all library functions):
```
{
    "libpmemobj"="NoReorderNoCheck"
}
```

List of marked **libpmemobj**(7) API functions:

```
pmemobj_alloc
pmemobj_cancel
pmemobj_check
pmemobj_close
pmemobj_create
pmemobj_ctl_exec
pmemobj_ctl_set
pmemobj_free
pmemobj_list_insert
pmemobj_list_insert_new
pmemobj_list_move
pmemobj_list_remove
pmemobj_memcpy
pmemobj_memmove
pmemobj_memset
pmemobj_memcpy_persist
pmemobj_memset_persist
pmemobj_open
pmemobj_publish
pmemobj_realloc
pmemobj_reserve
pmemobj_root
pmemobj_root_construct
pmemobj_strdup
pmemobj_tx_abort
pmemobj_tx_add_range
pmemobj_tx_add_range_direct
pmemobj_tx_alloc
pmemobj_tx_commit
pmemobj_tx_free
pmemobj_tx_publish
pmemobj_tx_realloc
pmemobj_tx_strdup
pmemobj_tx_wcsdup
pmemobj_tx_xadd_range
pmemobj_tx_xadd_range_direct
pmemobj_tx_xalloc
pmemobj_tx_zalloc
pmemobj_tx_zrealloc
pmemobj_wcsdup
pmemobj_xalloc
pmemobj_xreserve
pmemobj_zalloc
pmemobj_zrealloc
```


# PMEMCHECK STORE LOG #

To generate *store_log* for **pmreorder** run pmemcheck
with additional parameters:

```
valgrind \
	--tool=pmemcheck \
	-q \
	--log-stores=yes \
	--print-summary=no \
	--log-file=store_log.log \
	--log-stores-stacktraces=yes \
	--log-stores-stacktraces-depth=2 \
	--expect-fence-after-clflush=yes \
	test_binary writer_parameter
```

For further details of pmemcheck parameters see [pmemcheck documentation](https://github.com/pmem/valgrind/blob/pmem-3.13/pmemcheck/docs/pmc-manual.xml)


# ENVIRONMENT #

By default all logging from PMDK libraries is disabled.
To enable API macros logging set environment variable:

+ **PMREORDER_EMIT_LOG**=1


# EXAMPLE #

```
python pmreorder.py \
		-l store_log.log \
		-r NoReorderDoCheck \
		-o pmreorder_out.log \
		-c prog \
		-x PMREORDER_MARKER_NAME=ReorderPartial \
		-p checker_binary checker_parameter
```

Checker binary will be used to run consistency checks on
"store_log.log", output of pmemcheck tool. Any inconsistent
stores found during **pmreorder** analysis will be logged
to `pmreorder_out.log`.


# SEE ALSO #

**<http://pmem.io>**
