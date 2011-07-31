/* vim: ts=4 sw=4 sts=4 et tw=78
 * Copyright (c) 2011 James R. McKaskill. See license in ffi.h
 */
#include "ffi.h"

typedef struct jit_header_t jit_header_t;

static function_t compile(Dst_DECL, lua_State* L, function_t func, int ref);

static void* reserve_code(jit_t* jit, lua_State* L, size_t sz);
static void commit_code(jit_t* jit, void* p, size_t sz);

static void push_int(lua_State* L, int val)
{ lua_pushnumber(L, val); }

static void push_uint(lua_State* L, unsigned int val)
{ lua_pushnumber(L, val); }

static void push_float(lua_State* L, float val)
{ lua_pushnumber(L, val); }

static void print(lua_State* L, int** p, size_t sz)
{
    size_t i;
    lua_getglobal(L, "print");
    lua_pushfstring(L, "%p", p);
    for (i = 0; i < sz; i++) {
        lua_pushfstring(L, " %p", p[i]);
    }
    lua_concat(L, (int) sz + 1);
    lua_call(L, 1, 0);
}

#ifndef _WIN32
static int GetLastError(void)
{ return errno; }
static void SetLastError(int err)
{ errno = err; }
#endif

#ifdef NDEBUG
#define shred(a,b,c)
#else
#define shred(p,s,e) memset((uint8_t*)(p)+(s),0xCC,(e)-(s))
#endif


#ifdef _WIN64
#include "dynasm/dasm_x86.h"
#include "call_x64win.h"
#elif defined __amd64__
#include "dynasm/dasm_x86.h"
#include "call_x64.h"
#elif defined __arm__ || defined __arm || defined __ARM__ || defined __ARM || defined ARM || defined _ARM_ || defined ARMV4I || defined _M_ARM
#include "dynasm/dasm_arm.h"
#include "call_arm.h"
#else
#include "dynasm/dasm_x86.h"
#include "call_x86.h"
#endif

struct jit_header_t {
    size_t size;
    int ref;
    uint8_t jump[JUMP_SIZE];
};

#define LINKTABLE_MAX_SIZE (sizeof(extnames) / sizeof(extnames[0]) * (JUMP_SIZE))

static function_t compile(jit_t* jit, lua_State* L, function_t func, int ref)
{
    jit_header_t* code;
    size_t codesz;
    int err;

    dasm_checkstep(jit, -1);
    if ((err = dasm_link(jit, &codesz)) != 0) {
        char buf[32];
        sprintf(buf, "%x", err);
        luaL_error(L, "dasm_link error %s", buf);
    }

    codesz += sizeof(jit_header_t);
    code = (jit_header_t*) reserve_code(jit, L, codesz);
    code->ref = ref;
    code->size = codesz;
    compile_extern_jump(jit, L, func, code->jump);

    if ((err = dasm_encode(jit, code+1)) != 0) {
        char buf[32];
        sprintf(buf, "%x", err);
        commit_code(jit, code, 0);
        luaL_error(L, "dasm_encode error %s", buf);
    }

    commit_code(jit, code, codesz);
    return (function_t) (code+1);
}

typedef uint8_t jump_t[JUMP_SIZE];

int get_extern(jit_t* jit, uint8_t* addr, int idx, int type)
{
    page_t* page = jit->pages[jit->pagenum-1];
    jump_t* jumps = (jump_t*) (page+1);
    jit_header_t* h = (jit_header_t*) ((uint8_t*) page + page->off);
    uint8_t* jmp;
    ptrdiff_t off;

    if (idx == jit->function_extern) {
       jmp = h->jump;
    } else {
       jmp = jumps[idx];
    }

    /* compensate for room taken up for the offset so that we can work rip
     * relative */
    addr += BRANCH_OFF;

    /* see if we can fit the offset in the branch displacement, if not use the
     * jump instruction */
    off = *(uint8_t**) jmp - addr;

    if (MIN_BRANCH <= off && off <= MAX_BRANCH) {
        return (int32_t) off;
    } else {
        return (int32_t)(jmp + sizeof(uint8_t*) - addr);
    }
}

static void* reserve_code(jit_t* jit, lua_State* L, size_t sz)
{
    page_t* page;
    size_t off = (jit->pagenum > 0) ? jit->pages[jit->pagenum-1]->off : 0;
    size_t size = (jit->pagenum > 0) ? jit->pages[jit->pagenum-1]->size : 0;

    if (off + sz >= size) {
        int i;
        uint8_t* pdata;
        function_t func;

        /* need to create a new page */
        jit->pages = (page_t**) realloc(jit->pages, (++jit->pagenum) * sizeof(jit->pages[0]));

        size = ALIGN_UP(sz + LINKTABLE_MAX_SIZE + sizeof(page_t), jit->align_page_size);

        page = (page_t*) AllocPage(size);
        jit->pages[jit->pagenum-1] = page;
        pdata = (uint8_t*) page;
        page->size = size;
        page->off = sizeof(page_t);

        lua_newtable(L);

#define ADDFUNC(DLL, NAME) \
        lua_pushliteral(L, #NAME); \
        func = DLL ? (function_t) GetProcAddressA(DLL, #NAME) : NULL; \
        func = func ? func : (function_t) &NAME; \
        lua_pushcfunction(L, (lua_CFunction) func); \
        lua_rawset(L, -3)

        ADDFUNC(NULL, print);
        ADDFUNC(NULL, to_double);
        ADDFUNC(NULL, to_float);
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
        ADDFUNC(NULL, push_int);
        ADDFUNC(NULL, push_uint);
        ADDFUNC(NULL, push_float);
        ADDFUNC(jit->kernel32_dll, SetLastError);
        ADDFUNC(jit->kernel32_dll, GetLastError);
        ADDFUNC(jit->lua_dll, luaL_error);
        ADDFUNC(jit->lua_dll, lua_pushnumber);
        ADDFUNC(jit->lua_dll, lua_pushboolean);
        ADDFUNC(jit->lua_dll, lua_gettop);
        ADDFUNC(jit->lua_dll, lua_rawgeti);
        ADDFUNC(jit->lua_dll, lua_pushnil);
        ADDFUNC(jit->lua_dll, lua_callk);
        ADDFUNC(jit->lua_dll, lua_settop);
        ADDFUNC(jit->lua_dll, lua_remove);
#undef ADDFUNC

        for (i = 0; extnames[i] != NULL; i++) {

            if (strcmp(extnames[i], "FUNCTION") == 0) {
                shred(pdata + page->off, 0, JUMP_SIZE);
                jit->function_extern = i;

            } else {
                lua_getfield(L, -1, extnames[i]);
                func = (function_t) lua_tocfunction(L, -1);

                if (func == NULL) {
                    luaL_error(L, "internal error: missing link for %s", extnames[i]);
                }

                compile_extern_jump(jit, L, func, pdata + page->off);
                lua_pop(L, 1);
            }

            page->off += JUMP_SIZE;
        }

        page->freed = page->off;
        lua_pop(L, 1);

    } else {
        page = jit->pages[jit->pagenum-1];
        EnableWrite(page, page->size);
    }

    return (uint8_t*) page + page->off;
}

static void commit_code(jit_t* jit, void* code, size_t sz)
{
    page_t* page = jit->pages[jit->pagenum-1];
    page->off += sz;
    EnableExecute(page, page->size);
    {
#if 0
        FILE* out = fopen("\\Hard Disk\\out.bin", "wb");
        fwrite(page, page->off, 1, out);
        fclose(out);
#endif
    }
}

void free_code(jit_t* jit, lua_State* L, function_t func)
{
    size_t i;
    jit_header_t* h = ((jit_header_t*) func) - 1;
    for (i = 0; i < jit->pagenum; i++) {
        page_t* p = jit->pages[i];

        if ((uint8_t*) h < (uint8_t*) p || (uint8_t*) p + p->size <= (uint8_t*) h) {
            continue;
        }

        luaL_unref(L, LUA_REGISTRYINDEX, h->ref);

        EnableWrite(p, p->size);
        p->freed += h->size;

        shred(h, 0, h->size);

        if (p->freed < p->off) {
            EnableExecute(p, p->size);
            return;
        }

        FreePage(p, p->size);
        memmove(&jit->pages[i], &jit->pages[i+1], (jit->pagenum - (i+1)) * sizeof(jit->pages[0]));
        return;
    }

    assert(!"couldn't find func in the jit pages");
}


