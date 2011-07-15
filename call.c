#include "ffi.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

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
    fprintf(stderr, "%.*s", (int) codesz, (char*) code);
    return code;
}

#ifndef _WIN32
static int GetLastError(void)
{ return errno; }
static void SetLastError(int err)
{ errno = err; }
#endif


#include "dynasm/dasm_x86.h"

#ifdef _WIN64
#include "call_x64win.h"
#elif defined __amd64__
#include "call_x64.h"
#else
#include "call_x86.h"
#endif

