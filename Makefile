.PHONY: all clean test

LUA_CFLAGS=`pkg-config --cflags lua5.1 2>/dev/null || pkg-config --cflags lua`
LUA=lua
SOCFLAGS=-fPIC
SOCC=$(CC) -shared $(SOCFLAGS)
CFLAGS=-fPIC -O2 -Wall -Werror $(LUA_CFLAGS) -fvisibility=hidden -Wno-unused-function

MODNAME=ffi
MODSO=$(MODNAME).so

all: $(MODSO) test_cdecl.so

macosx:
	$(MAKE) all "SOCC=MACOSX_DEPLOYMENT_TARGET=10.3 $(CC) -dynamiclib -single_module -undefined dynamic_lookup $(SOCFLAGS)"

clean:
	rm -f *.o *.so call_*.h

call_x86.h: call_x86.dasc dynasm/*.lua
	$(LUA) dynasm/dynasm.lua -LN -o $@ $<

call_x64.h: call_x86.dasc dynasm/*.lua
	$(LUA) dynasm/dynasm.lua -D X64 -LN -o $@ $<

call_x64win.h: call_x86.dasc dynasm/*.lua
	$(LUA) dynasm/dynasm.lua -D X64 -D X64WIN -LN -o $@ $<

%.o: %.c *.h dynasm/*.h call_x86.h call_x64.h call_x64win.h
	$(CC) $(CFLAGS) -o $@ -c $<

$(MODSO): ffi.o ctype.o parser.o call.o
	$(SOCC) $^ -o $@

test_cdecl.so: test.o
	$(SOCC) $^ -o $@

test: test_cdecl.so $(MODSO)
	$(LUA) test.lua



