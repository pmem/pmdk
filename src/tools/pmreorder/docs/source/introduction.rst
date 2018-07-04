Synopsis
========
.. parsed-literal::

   **pmreorder.py** LOGFILE LIB FUNC

Description
===========
The documentation is under construction.

The pmreorder tool is a collection of python scripts designed to parse and
replay operations logged by pmemcheck - a persistent memory checking tool.

Pmreorder performs store reordering between persistent memory barriers -
a sequence of flush-fence operations. It uses the consistency
checking routine provided in the command line options to check whether the files
are in a consistent state. By default all possible combinations of stores are
tested. This may take a significant amount of time, so the type of reordering
may be switched at runtime.

Seeing as the combination of Valgrind and the multiple store reordering will
take a lot of time to process, it would be beneficial to do as little stores
as possible to verify whether the tested program is powerfail safe.

The consistency checking function has to have a specific signature:

.. parsed-literal::

  **int func_name(const char \*file_name);**


* LOGFILE - the log output provided by pmemcheck
* LIB - the shared library with the consistency checking function
* FUNC - the name of the consistency checking function

