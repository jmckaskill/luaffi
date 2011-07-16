#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
# include <lua.h>
# include <lauxlib.h>
}
# define EXTERN_C extern "C"
#else
# include <lua.h>
# include <lauxlib.h>
# define EXTERN_C extern
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/mman.h>
#endif

#ifndef NDEBUG
#define DASM_CHECKS
#endif

typedef struct jit_t jit_t;
#define Dst_DECL	jit_t* Dst
#define Dst_REF		(Dst->ctx)
#define DASM_EXTERN(a,b,c,d) get_extern(a,b,c,d)

#include "dynasm/dasm_proto.h"

#if defined _WIN32
# define EXPORT __declspec(dllexport)
#elif defined __GNUC__
# define EXPORT __attribute__((visibility("default")))
#else
# define EXPORT
#endif

EXTERN_C EXPORT int luaopen_ffi(lua_State* L);

#if LUA_VERSION_NUM == 501
static int lua_absindex(lua_State* L, int idx) {
    return (LUA_REGISTRYINDEX <= idx && idx < 0)
         ? lua_gettop(L) + idx + 1
         : idx;
}
/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l && l->name; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}
#define lua_setuservalue lua_setfenv
#define lua_getuservalue lua_getfenv
#define lua_rawlen lua_objlen
#endif


#ifdef _WIN32
#define LIB_FORMAT_1 "%s.dll"
#define AllocPage(size) VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE)
#define FreePage(data, size) VirtualFree(data, 0, MEM_RELEASE)
static void EnableExecute(void* data, size_t size, int enabled)
{
    DWORD oldprotect;
    VirtualProtect(data, size, enabled ? PAGE_EXECUTE : PAGE_READWRITE, &oldprotect);
}
#else
#define LIB_FORMAT_1 "%s.so"
#define LIB_FORMAT_2 "lib%s.so"
#define LoadLibraryA(name) dlopen(name, RTLD_NOW | RTLD_GLOBAL)
#define GetProcAddress(lib, name) dlsym(lib, name)
#define AllocPage(size) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
#define FreePage(data, size) munmap(data, size)
#define EnableExecute(data, size, enabled) mprotect(data, size, enabled ? PROT_READ|PROT_EXEC : PROT_READ|PROT_WRITE)
#endif


typedef struct {
    int line;
    const char* next;
    const char* prev;
    size_t align_mask;
} parser_t;

typedef struct {
    uint8_t* data;
    size_t size;
    size_t off;
} page_t;

struct jit_t {
    lua_State* L;
    int32_t last_errno;
    dasm_State* ctx;
    size_t pagenum;
    page_t* pages;
    size_t align_page_size;
    void** globals;
    int function_extern;
    int errno_extern;
    void* lua_dll;
    void* kernel32_dll;
};

#define ALIGN(PTR, MASK) \
  (( ((uintptr_t) (PTR)) + ((uintptr_t) (MASK)) ) & (~ ((uintptr_t) (MASK)) ))


/* cdata_t/ctype_t */

struct ptr_align {char ch; void* v;};
#define PTR_ALIGN_MASK (((uintptr_t) &((struct ptr_align*) NULL)->v) - 1)
#define FUNCTION_ALIGN_MASK (sizeof(void (*)()) - 1)
#define DEFAULT_ALIGN_MASK 7

/* this needs to match the order of upvalues in luaopen_ffi
 * warning: CDATA_MT is the only upvalue copied across to the function pointer
 * closures (see call_*.dasc) so that to_cdata works
 */
enum {
    CTYPE_MT_IDX = 2,
    CDATA_MT_IDX,
    CMODULE_MT_IDX,
    CONSTANTS_IDX,
    WEAK_KEY_MT_IDX,
    TYPE_IDX,
    FUNCTION_IDX,
    ABI_PARAM_IDX,
    JIT_IDX,
    GC_IDX,
    TO_NUMBER_IDX,
    LAST_IDX,
};

#define UPVAL_NUM (LAST_IDX - 2)

#define CTYPE_MT_UPVAL lua_upvalueindex(CTYPE_MT_IDX - 1)
#define CDATA_MT_UPVAL lua_upvalueindex(CDATA_MT_IDX - 1)
#define CMODULE_MT_UPVAL lua_upvalueindex(CMODULE_MT_IDX - 1)
#define CONSTANTS_UPVAL lua_upvalueindex(CONSTANTS_IDX - 1)
#define WEAK_KEY_MT_UPVAL lua_upvalueindex(WEAK_KEY_MT_IDX - 1)
#define TYPE_UPVAL lua_upvalueindex(TYPE_IDX - 1)
#define FUNCTION_UPVAL lua_upvalueindex(FUNCTION_IDX - 1)
#define ABI_PARAM_UPVAL lua_upvalueindex(ABI_PARAM_IDX - 1)
#define JIT_UPVAL lua_upvalueindex(JIT_IDX - 1)
#define GC_UPVAL lua_upvalueindex(GC_IDX - 1)
#define TO_NUMBER_UPVAL lua_upvalueindex(TO_NUMBER_IDX - 1)

/* both ctype and cdata are stored as userdatas
 *
 * usr value is a table shared between the related subtypes which has:
 * name -> member ctype (for structs and unions)
 * +ves -> member ctype - in memory order (for structs)
 * +ves -> argument ctype (for function prototypes)
 * 0 -> return ctype (for function prototypes)
 * light userdata -> misc
 */

#define POINTER_BITS 4
#define POINTER_MAX ((2 << POINTER_BITS) - 2)

enum {
    C_CALL,
    STD_CALL,
    FAST_CALL,
};

enum {
    VOID_TYPE,
    DOUBLE_TYPE,
    FLOAT_TYPE,
    BOOL_TYPE,
    INT8_TYPE,
    INT16_TYPE,
    INT32_TYPE,
    INT64_TYPE,
    UINT8_TYPE,
    UINT16_TYPE,
    UINT32_TYPE,
    UINT64_TYPE,
    UINTPTR_TYPE,
    ENUM_TYPE,
    /* fancy types from here */
    UNION_TYPE,
    STRUCT_TYPE,
    FUNCTION_TYPE,
};

#define IS_UNSIGNED(type) (type >= UINT8_TYPE)
#define IS_CHAR(type) ((type) == INT8_TYPE || (type) == UINT8_TYPE)

/* note: if adding a new member that is associated with a struct/union
 * definition then it needs to be copied over in ctype.c for when we create
 * types based off of the declaration alone */
typedef struct ctype_t {
    size_t base_size; /* size of the base type */
    union {
        /* Valid if is_array and not is_variable_struct and not is_variable_array */
        size_t array_size;
        /* Valid for is_variable_struct or is_variable_array. If
         * variable_size_known (only used for is_variable_struct) then this is
         * the total increment otherwise this is the per element increment
         */
        size_t variable_increment;
    };
    size_t offset;
    unsigned int align_mask : 4; /* as align bytes - 1 eg 7 gives 8 byte alignment */
    unsigned int pointers : 4;
    unsigned int type : 5;
    unsigned int is_reference : 1;
    unsigned int is_array : 1;
    unsigned int calling_convention : 2;
    unsigned int is_defined : 1;
    unsigned int has_var_arg : 1;
    unsigned int is_variable_struct : 1;
    unsigned int is_variable_array : 1;
    unsigned int variable_size_known : 1;
} ctype_t;

typedef union cdata_t cdata_t;

#ifdef _MSC_VER
__declspec(align(16))
#endif
union cdata_t {
    const ctype_t type
#ifdef __GNUC__
      __attribute__ ((aligned(16)))
#endif
      ;

};

typedef void (*function_t)(void);

void set_defined(lua_State* L, int ct_usr, ctype_t* ct);
void push_ctype(lua_State* L, int ct_usr, const ctype_t* ct);
void* push_cdata(lua_State* L, int ct_usr, const ctype_t* ct);
void check_ctype(lua_State* L, int idx, ctype_t* ct);
void* to_cdata(lua_State* L, int idx, ctype_t* ct);
void* check_cdata(lua_State* L, int idx, ctype_t* ct);
size_t ctype_size(lua_State* L, const ctype_t* ct);

void parse_type(lua_State* L, parser_t* P, ctype_t* type);
const char* parse_argument(lua_State* L, parser_t* P, int ct_usr, ctype_t* type, size_t* namesz);
void push_type_name(lua_State* L, int usr, const ctype_t* ct);

int ffi_cdef(lua_State* L);

int x86_stack_required(lua_State* L, int usr);
void push_function(jit_t* jit, lua_State* L, function_t f, int ct_usr, const ctype_t* ct);
void compile_globals(jit_t* jit, lua_State* L);
int get_extern(jit_t* jit, uint8_t* addr, int idx, int type);

/* WARNING: assembly needs to be updated for prototype changes of these functions */
double to_double(lua_State* L, int idx);
uint64_t to_uint64(lua_State* L, int idx);
int64_t to_int64(lua_State* L, int idx);
int32_t to_int32(lua_State* L, int idx);
uint32_t to_uint32(lua_State* L, int idx);
uintptr_t to_uintptr(lua_State* L, int idx);
int32_t to_enum(lua_State* L, int idx, int to_usr, const ctype_t* tt);
/* this will always push a value for temp storage for structs created on the fly */
void* to_typed_pointer(lua_State* L, int idx, int to_usr, const ctype_t* tt);
function_t to_typed_function(lua_State* L, int idx, int to_usr, const ctype_t* tt);

void unpack_varargs_stack(lua_State* L, int first, int last, char* to);
void unpack_varargs_reg(lua_State* L, int first, int last, char* to);

void unpack_varargs_stack_skip(lua_State* L, int first, int last, int ints_to_skip, int floats_to_skip, char* to);
void unpack_varargs_float(lua_State* L, int first, int last, int max, char* to);
void unpack_varargs_int(lua_State* L, int first, int last, int max, char* to);



