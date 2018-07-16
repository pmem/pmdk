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

**pmreorder** -- performs persistent consistency check
		 using store reordering mechanism


# SYNOPSIS #

```
$ python pmreorder <options>
```


# DESCRIPTION #

The pmreorder tool is a collection of python scripts designed
to parse and replay operations logged by pmemcheck -
a persistent memory checking tool.

Pmreorder performs store reordering between persistent
memory barriers - a sequence of flush-fence operations.
It uses the consistency checking routine provided in the
command line options to check whether the files are in a
consistent state.

Considering that logging of operations under Valgrind, and its
then reply and reorder, is very time consuming, it is recommended
to use as few stores as possible in test workloads.


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

`-t <print|file>, --output_type <print|file>`
Set the logger output type. Default value is `print`.

`-o <pmreorder_output>, --output <pmreorder_output>`
Set the logger output file.
(If `-t` option is set to `file`).

`-e <debug|info|warning|error|critical>,
 --output_level <debug|info|warning|error|critical>`
Set the output log level.

`-r  <full|noreorder|accumulative|partial>,
 --default_engine  <full|noreorder|accumulative|partial>`
Set the default reorder engine. Default value is `full`.


# ENGINES #

By default, the full reorder engine is used, which means that
for each set of stores, the tool generates and checks consistency
of all possible store permutations. This might prove to be very
computationally expensive for most workloads.
In order to facilitate tests that will finish in a reasonable
time, but with reduced number of test sequences, a number of other
engines was developed:

+ **noreorder** - pass-through of unchanged operations.
Checks correctness of the stores as they were logged.
Useful for operations that do not require fail safety.

```
Example:
        input: (a, b, c)
        output: (a, b, c)
```

+ **accumulative** - checks correctness on a growing
subset of the original sequence.

```
Example:
        input: (a, b, c)
        output:
               ()
               ('a')
               ('a', 'b')
               ('a', 'b', 'c')
```

+ **partial** - checks consistency on 3 randomly selected sequences
from a set of 1000 combination without replacement of the original log.

```
 Example:
         input: (a, b, c)
         output:
                ('b', 'c')
                ('b',)
                ('a', 'b', 'c')
```


# INSTRUMENTATION #

Another way to switch types of reordering is to use Valgrind-pmemcheck
instrumentation in the tested program.

Exposed macros:

+ **VALGRIND_LOG_STORES**

Start logging memory operations.

+ **VALGRIND_NO_LOG_STORES**

Stop logging memory operations.

+ **VALGRIND_FULL_REORDED**

Issues a *full* reorder engine marker.

+ **VALGRIND_PARTIAL_REORDER**

Issues a *partial* reorder engine marker.

+ **VALGRIND_ONLY_FAULT**

Issues a log to disable reordering.
For more details see *noreorder* engine in ENGINES section above.

+ **VALGRIND_STOP_REORDER_FAULT**

Issues log to disable reordering as **VALGRIND_ONLY_FAULT**,
but additionally tells to skip the following stores and does not
execute consistency checker.

+ **VALGRIND_DEFAULT_REORDED**

Issues a log to set the default reorder engine.
(Previously set with --default_engine or -r option,
see OPTIONS section above).

For more details about mentioned engines see ENGINES section above.


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
		-r partial \
		-t file \
		-o pmreorder_out.log \
		-c prog \
		-p test_binary.exe checker_parameter
```

If any issue occurs during **pmreorder** analysis,
inconsistent stores are logged to pmreorder_out.log.


# SEE ALSO #

**<http://pmem.io>**
