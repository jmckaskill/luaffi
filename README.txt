About
-----
This is a library for calling C function and manipulating C types from lua. It
is designed to be interface compatible with the FFI library in luajit (see
http://luajit.org/ext_ffi.html). It can parse C function declarations and
struct definitions that have been directly copied out of C header files and
into lua source as a string.

License
-------
MIT same as Lua 5.1

Platforms
---------
Currently only windows x86 dll with vc. Adding posix x86 is trivial, other
architectures is more work.

Runs with both Lua 5.1 and Lua 5.2 beta.

Build
-----
To build run msvcbuild.bat in a visual studio cmd prompt (or 'msvcbuild.bat
debug' for a debug dll)

Edit msvcbuild.bat if your lua exe, lib, or lua include path differ from Lua
5.1 for windows.

Known Issues
------------
- Has not been bullet proof tested
- Casting is different from luajit. For the moment this follows C++
  - ffi.cast is equivalent to a C cast in C++ (T t = (T) f)
  - ffi.new and ctype() is equivalent to an implicit cast in C++ (T t = f)
     - since this follows C++ semantics void* does not cast to T* (an explicit
       cast using ffi.cast is required)
- Comparing a ctype pointer to nil doesn't work the same as luajit. This is
  unfixable with the current metamethod semantics. Instead use ffi.C.NULL
- Constant expressions can't handle non integer intermediate values (eg
  offsetof won't work because it manipulates pointers)
- Not all metamethods work with lua 5.1 (eg void* + number). This is due to
  the way metamethods are looked up with mixed types in Lua 5.1. If you need
this upgrade to Lua 5.2 or use boxed numbers (uint64_t and uintptr_t).

Todo
----
- Bitfields
- Fastcall
- Variable sized arrays and structs using [?]
- Lua function callbacks
- static const int
- C++ classes (excluding inline c++ functions)
- All the various gcc and msvc attributes

How it works
------------
Types are represented by a ctype_t structure and an associated user value table.
The table is shared between all related types for structs, unions, and
functions. It's members have the types of struct members, function argument
types, etc. The ctype_t structure then contains the modifications from the
base type (eg number of pointers, array size, etc).

Types are pushed into lua as a userdata containing the ctype_t with a user
value (or fenv in 5.1) set to the shared type table.

Boxed cdata types are pushed into lua as a userdata containing the cdata_t
structure (which contains the ctype_t of the data as its header) followed by
the boxed data.

The functions in ffi.c provide the cdata and ctype metatables and ffi.*
functions which manipulate these two types.

C functions (and function pointers) are pushed into lua as a lua c function
with the function pointer cdata as the first upvalue. The actual code is JITed
using dynasm (see call_x86.dasc). The JITed code does the following in order:
1. Calls the needed unpack functions in ffi.c placing each argument on the HW stack
2. Updates errno
3. Performs the c call 
4. Retries errno
5. Pushes the result back into lua from the HW register or stack



