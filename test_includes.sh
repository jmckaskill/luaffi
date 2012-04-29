#!/bin/sh
make test || exit
mkdir -p test_includes
for f in /usr/include/*.h
do
  echo "${f}";
  (cc -E -c "${f}" || continue) | lua -e '
    local str = io.read("*a")
    -- remove preprocessor commands eg line directives
    str = str:gsub("#[^\n]*", "")
    -- remove inline function definitions and declarations
    str = str:gsub("static [^;(]* __attribute__%b()", "static ")
    str = str:gsub("static [^;(]*%b()%s*%b{}", "")
    str = str:gsub("static [^;(]*%b()%s*;", "")
    io.write(str)' > "test_includes/`basename $f`"
  lua -e 'ffi = require("ffi"); ffi.cdef(io.read("*a"))' < "test_includes/`basename $f`"
done
