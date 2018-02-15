C++ Bindings For libpmemobj	{#mainpage}
===========================

This is the C++ API for libpmemobj.

During the development of libpmemobj, many difficulties were encountered and
compromises were made to make the C API as much user-friendly as possible. This
is mostly due to the semantics of the C language. Since C++ is a more expressive
language, it was natural to try and bridge the gap using native C++ features.

There are three main features of the C++ bindings:
 - the `persistent_ptr<>` smart pointer,
 - the `transaction`, which comes in two flavours - scoped and closure,
 - the `p<>` property.

The main issue with the C API is the generic PMEMoid and its typed counterpart,
the TOID. For them to be conveniently used in transactions, a large set of
macros has been defined. This made using the generic pointer easier, yet still
unintuitive. In C++, the `persistent_ptr<>` template makes it a lot easier
providing well known smart pointer semantics and built-in transactions support.

The other drawback of the C API is the transaction semantics. Manual usage of
`setjmp` and `jmpbuf` is error prone, so they were once again wrapped in
macros. They themselves have issues with undefined values of automatic
variables (see the libpmemobj manpage for more details). The transactions
defined in the C++ bindings try to fix the inadequacies of their C counterparts.

The `p<>`, which is called the _persistent property_, was designed with
seamless persistent memory integration in mind. It is designed to be used with
basic types within classes, to signify that these members in fact reside in
persistent memory and need to be handled appropriately.

Please remember to take extra care when using _static class members_. They are
not stored in persistent memory, therefore their value will _not_ always be
consistent across subsequent executions or compilations of user applications.

The C++ bindings implement an experimental, standard compliant memory allocator
which can be used in the C++ standard library's containers. This is an
experimental feature that should work with a custom libc++ implementation found
here https://github.com/pmem/libcxx. Please refer to the official LLVM
documentation on how to compile and install libc++. Also please note that the
allocator along with the changes in the implementation of libc++ are considered
experimental and are subject to change without prior notice.

If you find any issues or have suggestion about these bindings please file an
issue in https://github.com/pmem/issues. There are also blog articles in
http://pmem.io/blog/ which you might find helpful.

Have fun!
The PMDK team

### Compiler notice ###
The C++ bindings require a C++11 compliant compiler, therefore the minimal
versions of GCC and Clang are 4.8.1 and 3.3 respectively. However the
pmem::obj::transaction::automatic class requires C++17, so
you need a more recent version for this to be available(GCC 6.1/Clang 3.7).
It is recommended to use these or newer versions of GCC or Clang.

### Standard notice ###
Please note that the C++11 standard, section 3.8, states that a valid
non-trivially default constructible object (in other words, not plain old data)
must be properly constructed in the lifetime of the application.
Libpmemobj, or any shared memory solution for that matter, does not
strictly adhere to that constraint.

We believe that in the future, languages that wish to support persistent memory
will need to alter their semantics to establish a defined behavior for objects
whose lifetimes exceed that of the application. In the meantime, the programs
that wish to use persistent memory will need to rely on compiler-defined
behavior.

Our library, and by extension these bindings, have been extensively tested in
g++, clang++ and MSVC++ to make sure that our solution is safe to use and
practically speaking implementation defined. The only exception to this rule is
the use of polymorphic types, which are notably forbidden when using C++
bindings.

### Important classes/functions ###

 * Transactional allocations - make_persistent.hpp
 * Transactional array allocations - make_persistent_array.hpp
 * Atomic allocations - make_persistent_atomic.hpp
 * Atomic array allocations - make_persistent_array_atomic.hpp
 * Resides on persistent memory property - [p](@ref pmem::obj::p)
 * Persistent smart pointer - [persistent_ptr](@ref pmem::obj::persistent_ptr)
 * Persistent memory transactions - [transaction](@ref pmem::obj::transaction)
 * Persistent memory resident mutex - [mutex](@ref pmem::obj::mutex)
 * Persistent memory pool - [pool](@ref pmem::obj::pool)
 * Persistent memory allocator - [allocator](@ref pmem::obj::allocator)
