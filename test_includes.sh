#!/bin/sh
make test || exit
mkdir -p test_includes
for f in /usr/include/*.h
do
  echo "${f}";
  cc -E -c "${f}" | sed -e 's/^#.*//g' > "test_includes/`basename $f`"
  lua -e 'ffi = require("ffi"); ffi.cdef(io.read("*a"))' < "test_includes/`basename $f`"
done
