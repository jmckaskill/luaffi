#include "ffi.h"

static void* reserve_code(jit_t* jit, lua_State* L, size_t sz);
static void commit_code(jit_t* jit, void* p, size_t sz);
static void* compile(Dst_DECL, lua_State* L, function_t func);

static void push_uint(lua_State* L, unsigned int val)
{ lua_pushnumber(L, val); }


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

#define LINKTABLE_MAX_SIZE (sizeof(extnames) / sizeof(extnames[0]) * (JUMP_SIZE))

static void* compile(jit_t* jit, lua_State* L, function_t func)
{
    uint8_t* code;
    size_t codesz;
    int err;

    dasm_checkstep(jit, -1);
    if ((err = dasm_link(jit, &codesz)) != 0) {
        luaL_error(L, "dasm_link error %d", err);
    }

    code = (uint8_t*) reserve_code(jit, L, codesz + JUMP_SIZE);
    compile_extern_jump(jit, L, func, code);
    if ((err = dasm_encode(jit, code + JUMP_SIZE)) != 0) {
        commit_code(jit, code, 0);
        luaL_error(L, "dasm_encode error %d", err);
    }

    commit_code(jit, code, codesz + JUMP_SIZE);
    return code + JUMP_SIZE;
}

int get_extern(jit_t* jit, uint8_t* addr, int idx, int type)
{
    page_t* page = &jit->pages[jit->pagenum-1];
    addr += 4; /* compensate for room taken up for the offset so that we can work rip relative */
    if (idx == jit->function_extern) {
        return (int32_t)(page->data + page->off - addr);
    } else {
        return (int32_t)(page->data + (idx * JUMP_SIZE) - addr);
    }
}

static void* reserve_code(jit_t* jit, lua_State* L, size_t sz)
{
    page_t* page;
    size_t off = (jit->pagenum > 0) ? jit->pages[jit->pagenum-1].off : 0;
    size_t size = (jit->pagenum > 0) ? jit->pages[jit->pagenum-1].size : 0;

    if (off + sz >= size) {
        int i;

        /* need to create a new page */
        jit->pages = (page_t*) realloc(jit->pages, (++jit->pagenum) * sizeof(page_t));
        page = &jit->pages[jit->pagenum-1];

        page->size = ALIGN(sz + LINKTABLE_MAX_SIZE, jit->align_page_size);
        page->off = 0;
        page->data = (uint8_t*) AllocPage(page->size);

        lua_newtable(L);

#define ADDFUNC(name) \
        lua_pushliteral(L, #name); \
        lua_pushcfunction(L, (lua_CFunction) &name); \
        lua_rawset(L, -3)

        ADDFUNC(to_double);
        ADDFUNC(to_uint64);
        ADDFUNC(to_int64);
        ADDFUNC(to_int32);
        ADDFUNC(to_uint32);
        ADDFUNC(to_uintptr);
        ADDFUNC(to_enum);
        ADDFUNC(to_typed_pointer);
        ADDFUNC(to_typed_function);
        ADDFUNC(unpack_varargs_stack);
        ADDFUNC(unpack_varargs_stack_skip);
        ADDFUNC(unpack_varargs_reg);
        ADDFUNC(unpack_varargs_float);
        ADDFUNC(unpack_varargs_int);
        ADDFUNC(push_cdata);
        ADDFUNC(push_uint);
        ADDFUNC(SetLastError);
        ADDFUNC(GetLastError);
        ADDFUNC(luaL_error);
        ADDFUNC(lua_pushnumber);
        ADDFUNC(lua_pushboolean);
        ADDFUNC(lua_gettop);
#undef ADDFUNC

        for (i = 0; extnames[i] != NULL; i++) {
            if (strcmp(extnames[i], "FUNCTION") == 0) {
                jit->function_extern = i;
                memset(page->data + page->off, 0xCC, JUMP_SIZE);

            } else if (strcmp(extnames[i], "ERRNO") == 0) {
                compile_errno_load(jit, L, page->data + page->off);

            } else {
                function_t func;
                lua_getfield(L, -1, extnames[i]);
                func = (function_t) lua_tocfunction(L, -1);
                if (func == NULL) {
                    luaL_error(L, "internal error: missing link for %s", extnames[i]);
                }

                compile_extern_jump(jit, L, func, page->data + page->off);
                lua_pop(L, 1);
            }

            page->off += JUMP_SIZE;
        }

        lua_pop(L, 1);

    } else {
        page = &jit->pages[jit->pagenum-1];
        EnableExecute(page->data, page->size, 0);
    }

    return page->data + page->off;
}

static void commit_code(jit_t* jit, void* p, size_t sz)
{
    page_t* page = &jit->pages[jit->pagenum-1];
    page->off += sz;
    EnableExecute(page->data, page->size, 1);
}


