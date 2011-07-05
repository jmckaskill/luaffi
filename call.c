#include "ffi.h"
#include <windows.h>

static void push_uint(lua_State* L, unsigned int val)
{ lua_pushnumber(L, val); }

static void* compile(Dst_DECL, lua_State* L)
{
    void* code;
    size_t codesz;
    int err;

    dasm_checkstep(Dst, -1);
    if ((err = dasm_link(Dst, &codesz)) != 0) {
        luaL_error(L, "dasm_link error %d", err);
    }

    code = reserve_code(Dst, codesz);
    if ((err = dasm_encode(Dst, code)) != 0) {
        commit_code(Dst, code, 0);
        luaL_error(L, "dasm_encode error %d", err);
    }

    commit_code(Dst, code, codesz);
    return code;
}

#ifndef _WIN32
static int GetLastError(void)
{ return errno; }
#endif


#include "dynasm/dasm_x86.h"
#include "call_x86.h"
