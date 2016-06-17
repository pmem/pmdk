C++ Bindings For libpmemobj	{#mainpage}
===========================

This is the experimental C++ API for libpmemobj.

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

Please keep in mind that these C++ bindings are still in the experimental stage
and *SHOULD NOT* be used in production quality code. If you find any issues or
have suggestion about these bindings please file an issue in
https://github.com/pmem/issues. There are also blog articles in
http://pmem.io/blog/ which you might find helpful.

Have fun!
The NVML team

### Important classes/functions ###

 * Transactional allocations - make_persistent.hpp
 * Transactional array allocations - make_persistent_array.hpp
 * Atomic allocations - make_persistent_atomic.hpp
 * Atomic array allocations - make_persistent_array_atomic.hpp
 * Resides on persistent memory property - [p](@ref nvml::obj::p)
 * Persistent smart pointer - [persistent_ptr](@ref nvml::obj::persistent_ptr)
 * Persistent memory transactions - [transaction](@ref nvml::obj::transaction)
 * Persistent memory resident mutex - [mutex](@ref nvml::obj::mutex)
 * Persistent memory pool - [pool](@ref nvml::obj::pool)
