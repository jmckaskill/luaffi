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
    if (idx == jit->errno_extern) {
        return (int32_t)(page->data + (idx * JUMP_SIZE) - addr);
    } else {
        uint8_t* jmp = (idx == jit->function_extern)
                     ? (page->data + page->off)
                     : (page->data + idx*JUMP_SIZE);

        /* see if we can fit the offset in a 32bit displacement, if not use the jump instruction */
        ptrdiff_t off = *(uint8_t**) jmp - addr;

        if (INT32_MIN <= off && off <= INT32_MAX) {
            return (int32_t) off;
        } else {
            return (int32_t)(jmp + sizeof(uint8_t*) - addr);
        }
    }
}

static void* reserve_code(jit_t* jit, lua_State* L, size_t sz)
{
    page_t* page;
    size_t off = (jit->pagenum > 0) ? jit->pages[jit->pagenum-1].off : 0;
    size_t size = (jit->pagenum > 0) ? jit->pages[jit->pagenum-1].size : 0;

    if (off + sz >= size) {
        int i;
        function_t func;

        /* need to create a new page */
        jit->pages = (page_t*) realloc(jit->pages, (++jit->pagenum) * sizeof(page_t));
        page = &jit->pages[jit->pagenum-1];

        page->size = ALIGN(sz + LINKTABLE_MAX_SIZE, jit->align_page_size);
        page->off = 0;
        page->data = (uint8_t*) AllocPage(page->size);

        lua_newtable(L);

#define ADDFUNC(DLL, NAME) \
        lua_pushliteral(L, #NAME); \
        func = DLL ? (function_t) GetProcAddress(DLL, #NAME) : NULL; \
        func = func ? func : (function_t) &NAME; \
        lua_pushcfunction(L, (lua_CFunction) func); \
        lua_rawset(L, -3)

        ADDFUNC(NULL, to_double);
        ADDFUNC(NULL, to_uint64);
        ADDFUNC(NULL, to_int64);
        ADDFUNC(NULL, to_int32);
        ADDFUNC(NULL, to_uint32);
        ADDFUNC(NULL, to_uintptr);
        ADDFUNC(NULL, to_enum);
        ADDFUNC(NULL, to_typed_pointer);
        ADDFUNC(NULL, to_typed_function);
        ADDFUNC(NULL, unpack_varargs_stack);
        ADDFUNC(NULL, unpack_varargs_stack_skip);
        ADDFUNC(NULL, unpack_varargs_reg);
        ADDFUNC(NULL, unpack_varargs_float);
        ADDFUNC(NULL, unpack_varargs_int);
        ADDFUNC(NULL, push_cdata);
        ADDFUNC(NULL, push_uint);
        ADDFUNC(jit->kernel32_dll, SetLastError);
        ADDFUNC(jit->kernel32_dll, GetLastError);
        ADDFUNC(jit->lua_dll, luaL_error);
        ADDFUNC(jit->lua_dll, lua_pushnumber);
        ADDFUNC(jit->lua_dll, lua_pushboolean);
        ADDFUNC(jit->lua_dll, lua_gettop);
#undef ADDFUNC

        for (i = 0; extnames[i] != NULL; i++) {
            if (strcmp(extnames[i], "FUNCTION") == 0) {
                jit->function_extern = i;
                shred(page->data + page->off, 0, JUMP_SIZE);

            } else if (strcmp(extnames[i], "ERRNO") == 0) {
                compile_errno_load(jit, L, page->data + page->off);
                jit->errno_extern = i;

            } else {
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
#if 0
    {
        FILE* out = fopen("out.bin", "wb");
        fwrite(page->data, page->off, 1, out);
        fclose(out);
    }
#endif
}


