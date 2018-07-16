---
layout: manual
Content-Style: 'text/css'
title: _MP(PMREORDER, 1)
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
is very time consuming, it is recommended to use as few stores as
possible in test workloads.

**pmreorder** core functionality is still under construction.


# OPTIONS #

`-h, --help`
Prints synopsis and list of options.

`-l <store_log>, --logfile <store_log>`
The pmemcheck log file to process.

`-c <prog|lib>, --checker <prog|lib>`
Consistency checker type.

`-p <path>, --path <path>`
Path to the consistency checker.

`-n <name>, --name <name>`
The symbol name of the consistency checking function
in the library. Valid only if the checker type is `lib`.

`-o <pmreorder_output>, --output <pmreorder_output>`
Set the logger output file.

`-e <debug|info|warning|error|critical>,
 --output_level <debug|info|warning|error|critical>`
Set the output log level.

`-r  <NoReorderNoCheck|
      NoReorderDoCheck|
      ReorderFull|
      ReorderPartial|
      ReorderAccumulative>,
 --default_engine  <NoReorderNoCheck|
		    NoReorderDoCheck|
		    ReorderFull|
		    ReorderPartial|
		    ReorderAccumulative>`
Set the default reorder engine. Default value is `NoReorderNoCheck`.

`-x <cli_macros|config_file>, --extended_macros <cli_macros|config_file>`
Assigne an engine types to the defined marker.


# ENGINES #

By default, the **NoReorderNoCheck** engine is used,
which means that for each set of stores, the tool
pass-through all sequences of stores noreordered
and does not run consistency checker on them.

To enable different types of the reorder engine and
begin proper reordering tests, a number of other
engines was developed:

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

+ **ReorderPartial** - checks consistency on 3 randomly selected sequences
from a set of 1000 combination without repetition of the original log.

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

When pass engine as `-r` option it will be used for each
logged set of stores.
Additionaly use `-x` parameter to switch engines separately
for any marked code sections.
For more details about `-x` extended macros functionality see section
INSTRUMENTATION below.


# INSTRUMENTATION #

Base functionality of *pmreorder* tool is markers support.
User can define his own value and mark selected sections
to be reordered in customizable way.

For this purposes valgrind expose generic macro:

+ **VALGRIND_EMIT_LOG(value)**

It emits any log to *store_log* during pmemcheck processing.
Parameter *value* is a string variable derfined by user.
For more details about pmemcheck execution see
PMEMCHECK STORE LOG section belowe.

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

There is a few rules for macros creation:

+ Valid macro can have any name,
but begin and end section have to match.
+ Macro must have `.BEGIN` or `.END` suffix.
+ Macros can't be crossed.

To already defined markers you can assing proper engine type
and pass this configuration to **pmreorder** tool
using `-x` paramater.

There are two ways to set extend macros options:

+ Using command line interface in format:
```PMREORDER_MARKER_NAME1.BEGIN=ReorderName1,PMREORDER_MARKER_NAME2.BEGIN=ReorderName2```

+ Using configutation file in .json format:
```
{
    "PMREORDER_MARKER_NAME1.BEGIN"="ReorderName1",
    "PMREORDER_MARKER_NAME2.BEGIN"="ReorderName2"
}
```

For more details about available
engines types see ENGINES section above.


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
	test_binary.exe writer_parameter
```

For further details of pmemcheck parameters see [pmemcheck documentation](https://github.com/pmem/valgrind/blob/pmem-3.13/pmemcheck/docs/pmc-manual.xml)


# EXAMPLE #

```
python pmreorder.py \
		-l store_log.log \
		-r NoReorderDoCheck \
		-o pmreorder_out.log \
		-c prog \
		-x PMREORDER_MARKER_NAME.BEGIN=ReorderPartial \
		-p test_binary.exe checker_parameter
```

If any issue occurs during **pmreorder** analysis,
inconsistent stores are logged to pmreorder_out.log.


# SEE ALSO #

**<http://pmem.io>**
