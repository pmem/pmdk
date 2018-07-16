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

[comment]: <> (pmreorder.1 -- man page for daxio)

[NAME](#name)<br />
[SYNOPSIS](#synopsis)<br />
[DESCRIPTION](#description)<br />
[OPTIONS](#options)<br />
[ENGINES](#engines)<br />
[INSTRUMENTALIZATION](#instrumentalization)<br />
[PMEMCHECK STORE LOG](#pmemcheck-store-log)<br />
[EXAMPLE](#example)<br />
[SEE ALSO](#see-also)<br />


# NAME #

**pmreorder** -- performs persistent consisteny check
		 using store reordering mechanism


# SYNOPSIS #

```
$ python pmreorder <options>
```


# DESCRIPTION #

The pmreorder tool is a collection of python scripts designed
to parse and replay operations logged by pmemcheck -
a persistent memory checking tool.

Pmreorder performs store reordering between prersistent
memory barriers - a sequence of flush-fence operations.
It uses the consistency checking routine provided in the
command line options to check whether the files are in a
consistent state.

Seeing as the combination of valgrind and the multiple store
reordering will take a lot of time to process, it would be
beneficial to do as little stores as possible to verify
whether the tested program is powerfail safe.


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
The name of the consistency checking function
within the library.

`-t <print|file>, --output_type <print|file>`
Set the logger type. Default value is `print`.

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

By default all possible permutations of stores are tested.

In such case to check consistency of stores between barriers
pmreorder uses `full` reorder engine. It means that for each
set of stores it generates all possible permutations to check.
This may takes a significant amount of time, so there is avaiable
a few more types of engines to limit number of tested sequences:

+ **noreorder** - pass set of stores unchanged, do check
on the original collection. It can be useful for less
significant parts of code.

```
Example:
        input: (a, b, c)
        output: (a, b, c)
```

+ **accumulative** - perform check on growing sequences

```
Example:
        input: (a, b, c)
        output:
               ()
               ('a')
               ('a', 'b')
               ('a', 'b', 'c')
```

+ **partial** - generates a sequence of all possible combinations
without replacement of stores (limited to 1000), and then select
from them random 3 sequences to run under consistency checker.

```
 Example:
         input: (a, b, c)
         output:
                ('b', 'c')
                ('b',)
                ('a', 'b', 'c')
```


# INSTRUMENTALIZATION #

Another way to switch types of reordering is to use Valgrind-pmemcheck
instrumentalization at runtime within the tested program.

Exposed macros:

+ **VALGRIND_LOG_STORES**

Start logging memory operations.

+ **VALGRIND_NO_LOG_STORES**

Stop logging memory operations.

+ **VALGRIND_FULL_REORDED**

Issue a *full* reorder log.
For more details see ENGINES section above.

+ **VALGRIND_PARTIAL_REORDER**

Issue a *partial* reorder log.
For more details see ENGINES section above.

+ **VALGRIND_ONLY_FAULT**

Issue a log to disable reordering.
For more details see *noreorder* engine in ENGINES section above.

+ **VALGRIND_STOP_REORDER_FAULT**

Issue a log to disable reordering as **VALGRIND_ONLY_FAULT**,
but additionally disables consistency checker.

+ **VALGRIND_DEFAULT_REORDED**

Issue a log to set the default reorder engine.
(Previously set with --default_engine or -r option,
see OPTIONS section above).


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

For further details of pmemcheck parameters see [pmemcheck documentaion](https://github.com/pmem/valgrind/blob/pmem-3.13/pmemcheck/docs/pmc-manual.xml)


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

If any issue occures during **pmreorder** analisys,
inconsistent stores are logged to pmreorder_out.log.


# SEE ALSO #

**<http://pmem.io>**
