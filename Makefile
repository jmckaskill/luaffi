.PHONY: all clean test

LUA_CFLAGS=`pkg-config --cflags lua5.1`
LUA=../lua5.1/src/lua
CFLAGS=-fPIC -O2 -Wall $(LUA_CFLAGS) -fvisibility=hidden -Wno-unused-function -m32

all: ffi.so

clean: 
	rm -f *.o *.so

call_x86.h: call_x86.dasc
	$(LUA) dynasm/dynasm.lua -LN -o $@ $<

%.o: %.c *.h dynasm/*.h call_x86.h
	$(CC) $(CFLAGS) -o $@ -c $<

ffi.so: ffi.o ctype.o parser.o call.o
	$(CC) $(CFLAGS) -shared $^ -o $@

test_cdecl.so: test.o
	$(CC) $(CFLAGS) -shared $^ -o $@

test: test_cdecl.so ffi.so
	$(LUA) test.lua



