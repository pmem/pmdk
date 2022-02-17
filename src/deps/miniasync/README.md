# **miniasync: Mini Asynchronous Library**

[![GHA build status](https://github.com/pmem/miniasync/workflows/On_Pull_Request/badge.svg?branch=master)](https://github.com/pmem/miniasync/actions)
[![Coverage Status](https://codecov.io/github/pmem/miniasync/coverage.svg?branch=master)](https://codecov.io/gh/pmem/miniasync/branch/master)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/pmem/miniasync.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/pmem/miniasync/context:cpp)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/24161/badge.svg)](https://scan.coverity.com/projects/pmem-miniasync)

The **Mini Library for Asynchronous Programming in C** is a C low-level concurrency library for asynchronous functions.
For more information, see [pmem.io](https://pmem.io).

## Building

Requirements:
- C compiler
- cmake >= 3.3
- pkg_config

First, you have to create a `build` directory.
From there you have to prepare the compilation using CMake.
The final build step is just a `make` command.

```shell
$ mkdir build && cd build
$ cmake ..
$ make -j
```

### CMake standard options

List of options provided by CMake:

| Name | Description | Values | Default |
| - | - | - | - |
| BUILD_EXAMPLES | Build the examples | ON/OFF | ON |
| BUILD_TESTS | Build the tests | ON/OFF | ON |
| COVERAGE | Run coverage test | ON/OFF | OFF |
| DEVELOPER_MODE | Enable developer checks | ON/OFF | OFF |
| CHECK_CSTYLE | Check code style of C sources | ON/OFF | OFF |
| TRACE_TESTS | More verbose test outputs | ON/OFF | OFF |
| USE_ASAN | Enable AddressSanitizer | ON/OFF | OFF |
| USE_UBSAN | Enable UndefinedBehaviorSanitizer | ON/OFF | OFF |
| TESTS_USE_VALGRIND | Enable tests with valgrind | ON/OFF | ON |
| TEST_DIR | Working directory for tests | *dir path* | ./build/tests |
| CMAKE_BUILD_TYPE | Choose the type of build | None/Debug/Release/RelWithDebInfo | Debug |
| COMPILE_DML | Compile DML implementation of miniasync | ON/OFF | OFF |

## Running

The *miniasync* library can be found in the `build/src` directory.
You can also run the examples from the `build/examples` directory.

## Testing

After completing the building step, you can run tests by invoking:
`make test -j` or `ctest`.

To get more verbose output on failure, you can invoke:
`ctest --output-on-failure`.

Tests using Valgrind are switched on by default. To switch them off use:
`cmake -DTESTS_USE_VALGRIND=OFF ..`.

The option necessary to run tests `BUILD_TESTS` is set to `ON` by default.

### Building packages

In order to build 'rpm' or 'deb' packages you should issue the following commands:

```shell
$ mkdir build && cd build
$ cmake .. -DCPACK_GENERATOR="$GEN" -DCMAKE_INSTALL_PREFIX=/usr
$ make package
```

where $GEN is a type of package generator: RPM or DEB.

CMAKE_INSTALL_PREFIX must be set to a destination where packages will be installed.

## Contact Us

For more information on this library, contact
Piotr Balcer (piotr.balcer@intel.com),
[Google group](https://groups.google.com/group/pmem).
