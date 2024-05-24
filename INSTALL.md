# How to build the pmdk libraries from source

## Dependencies

Required packages for each supported OS are listed below. It is important to note that some tests and example applications require additional packages, but they do not interrupt building if they are missing. An appropriate message is displayed instead. For details please read the DEPENDENCIES section in the appropriate README file
(in [tests/](src/test/README) or [examples/](src/examples/README) sub-directories).

See our **[Dockerfiles](utils/docker/images)** (used e.g. on our CI
systems) to get an idea what packages are required to build
the entire PMDK, with all the tests and examples.

You will need to install the following required packages on the build system:

* **autoconf**
* **pkg-config**
* **libndctl-devel** (v63 or later)
* **libdaxctl-devel** (v63 or later)
* **pandoc** (for documentation, required during install)

The following packages are required only by selected PMDK components
or features:

PMDK depends on libndctl and libdaxctl to support RAS features. It is possible
to disable this support by passing `NDCTL_ENABLE=n` to `make`, but we strongly
discourage users from doing that. Disabling NDCTL strips PMDK from ability to
detect hardware failures, which may lead to silent data corruption.
For information how to disable RAS at runtime for kernels prior to 5.0.4 please
see https://github.com/pmem/pmdk/issues/4207.

## Building PMDK

To build from source, clone this tree:

```sh
git clone https://github.com/pmem/pmdk
cd pmdk
```

For a stable version, checkout a [release tag](https://github.com/pmem/pmdk/releases) as follows. Otherwise skip this step to build the latest development release.

```sh
git checkout tags/2.1.0
```

Once all required [dependencies](#dependencies) are installed, PMDK is built using the

```sh
make
```

By default, all code is built with the `-Werror` flag, which fails
the whole build when the compiler emits any warning. This is very useful during
development, but can be annoying in deployment. If you want to disable `-Werror`,
use the `EXTRA_CFLAGS` variable:

```sh
make EXTRA_CFLAGS="-Wno-error"
```

>or

```sh
make EXTRA_CFLAGS="-Wno-error=$(type-of-warning)"
```

## Installing PMDK

After compiling the libraries, you can install them:

```sh
sudo make install
```

## Testing PMDK

You will need to install the following package to run unit tests:
* **ndctl**

Before running the tests, you may need to prepare a test configuration file (`src/test/testconfig.sh`). Please see the available configuration settings in the example file [src/test/testconfig.sh.example](src/test/testconfig.sh.example).

To build and run the **unit tests**:

```sh
make check
```

To run a specific **subset of tests**, run for example:

```sh
make check TEST_TYPE=short TEST_BUILD=debug TEST_FS=pmem
```

To **modify the timeout** which is available for **check** type tests, run:

```sh
make check TEST_TIME=1m
```

This will set the timeout to 1 minute.

Please refer to the [src/test/README](src/test/README) for more details on how to
run different types of tests.

## Additional options

Both building and installation scripts are very flexible. To see additional options please see the [Makefile](Makefile) file.

### Memory Management Tools

The PMDK libraries support standard Valgrind DRD, Helgrind and Memcheck, as well as a PM-aware version of [Valgrind](https://github.com/pmem/valgrind).
By default, support for all tools is enabled. If you wish to disable it, supply the compiler with `VALGRIND` flag set to 0:

```sh
make VALGRIND=0
```

The `SANITIZE` flag allows the libraries to be tested with various
sanitizers. For example, to test the libraries with AddressSanitizer
and UndefinedBehaviorSanitizer, run:

```sh
make SANITIZE=address,undefined clobber check
```

## Debugging

To enable logging of debug information, use debug version of a library and set
desired log level using (library-specific) variable, e.g. `PMEM_LOG_LEVEL=<level>`.

For more details see appropriate manpage (debbuging section), e.g.
[libpmem(7)](https://github.com/pmem/pmdk/blob/master/doc/libpmem/libpmem.7.md#error-handling-1).

## Experimental Packages

Some components in the source tree are treated as experimental. By default,
those components are built but not installed (and thus not included in
packages).

If you want to build/install experimental packages run:

```sh
make EXPERIMENTAL=y [install,rpm,dpkg]
```
