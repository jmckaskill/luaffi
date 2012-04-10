/* vim: ts=4 sw=4 sts=4 et tw=78
 * Copyright (c) 2011 James R. McKaskill. See license in ffi.h
 */
#include "ffi.h"
#include <math.h>
#include <inttypes.h>

int jit_key;
int ctype_mt_key;
int cdata_mt_key;
int callback_mt_key;
int cmodule_mt_key;
int constants_key;
int types_key;
int gc_key;
int callbacks_key;
int functions_key;
int abi_key;
int next_unnamed_key;
int niluv_key;

void push_upval(lua_State* L, int* key)
{
    lua_pushlightuserdata(L, key);
    lua_rawget(L, LUA_REGISTRYINDEX);
}

void set_upval(lua_State* L, int* key)
{
    lua_pushlightuserdata(L, key);
    lua_insert(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

int equals_upval(lua_State* L, int idx, int* key)
{
    int ret;
    lua_pushvalue(L, idx);
    push_upval(L, key);
    ret = lua_rawequal(L, -2, -1);
    lua_pop(L, 2);
    return ret;
}

struct jit* get_jit(lua_State* L)
{
    struct jit* jit;
    push_upval(L, &jit_key);
    jit = (struct jit*) lua_touserdata(L, -1);
    jit->L = L;
    lua_pop(L, 1); /* still in registry */
    return jit;
}

static int type_error(lua_State* L, int idx, const char* to_type, int to_usr, const struct ctype* to_ct)
{
    luaL_Buffer B;
    struct ctype ft;

    assert(to_type || (to_usr && to_ct));
    if (to_usr) {
        to_usr = lua_absindex(L, to_usr);
    }

    idx = lua_absindex(L, idx);

    luaL_buffinit(L, &B);
    to_cdata(L, idx, &ft);

    if (ft.type != INVALID_TYPE) {
        push_type_name(L, -1, &ft);
        lua_pushfstring(L, "unable to convert argument %d from cdata<%s> to cdata<", idx, lua_tostring(L, -1));
        lua_remove(L, -2);
        luaL_addvalue(&B);
    } else {
        lua_pushfstring(L, "unable to convert argument %d from lua<%s> to cdata<", idx, luaL_typename(L, idx));
        luaL_addvalue(&B);
    }

    if (to_ct) {
        push_type_name(L, to_usr, to_ct);
        luaL_addvalue(&B);
    } else {
        luaL_addstring(&B, to_type);
    }

    luaL_addchar(&B, '>');

    luaL_pushresult(&B);
    return lua_error(L);
}

#define TO_NUMBER(TYPE, ALLOW_POINTERS)                                     \
    void* p;                                                                \
    struct ctype ct;                                                             \
                                                                            \
    switch (lua_type(L, idx)) {                                             \
    case LUA_TBOOLEAN:                                                      \
        return (TYPE) lua_toboolean(L, idx);                                \
                                                                            \
    case LUA_TNUMBER:                                                       \
        return (TYPE) lua_tonumber(L, idx);                                 \
                                                                            \
    case LUA_TSTRING:                                                       \
        if (ALLOW_POINTERS) {                                               \
            return (TYPE) (intptr_t) lua_tostring(L, idx);                  \
        } else {                                                            \
            goto err;                                                       \
        }                                                                   \
                                                                            \
    case LUA_TLIGHTUSERDATA:                                                \
        if (ALLOW_POINTERS) {                                               \
            return (TYPE) (intptr_t) lua_topointer(L, idx);                 \
        } else {                                                            \
            goto err;                                                       \
        }                                                                   \
                                                                            \
    case LUA_TUSERDATA:                                                     \
        p = to_cdata(L, idx, &ct);                                          \
                                                                            \
        if (ct.type == INVALID_TYPE) {                                      \
            if (ALLOW_POINTERS) {                                           \
                return (TYPE) (intptr_t) p;                                 \
            } else {                                                        \
                goto err;                                                   \
            }                                                               \
        }                                                                   \
                                                                            \
        lua_pop(L, 1);                                                      \
                                                                            \
        if (ct.pointers || ct.type == STRUCT_TYPE || ct.type == UNION_TYPE) {\
            if (ALLOW_POINTERS) {                                           \
                return (TYPE) (intptr_t) p;                                 \
            } else {                                                        \
                goto err;                                                   \
            }                                                               \
        } else if (ct.type == UINTPTR_TYPE) {                               \
            return (TYPE) *(intptr_t*) p;                                   \
        } else if (ct.type == INT64_TYPE) {                                 \
            return (TYPE) *(int64_t*) p;                                    \
        } else if (ct.type == UINT64_TYPE) {                                \
            return (TYPE) *(uint64_t*) p;                                   \
        } else {                                                            \
            goto err;                                                       \
        }                                                                   \
                                                                            \
    case LUA_TNIL:                                                          \
        return (TYPE) 0;                                                    \
                                                                            \
    default:                                                                \
        goto err;                                                           \
    }                                                                       \
                                                                            \
err:                                                                        \
    type_error(L, idx, #TYPE, 0, NULL);                                     \
    return 0

int32_t to_int32(lua_State* L, int idx)
{ TO_NUMBER(int32_t, 0); }

uint32_t to_uint32(lua_State* L, int idx)
{ TO_NUMBER(uint32_t, 0); }

int64_t to_int64(lua_State* L, int idx)
{ TO_NUMBER(int64_t, 0); }

uint64_t to_uint64(lua_State* L, int idx)
{ TO_NUMBER(uint64_t, 0); }

double to_double(lua_State* L, int idx)
{ TO_NUMBER(double, 0); }

float to_float(lua_State* L, int idx)
{ TO_NUMBER(float, 0); }

uintptr_t to_uintptr(lua_State* L, int idx)
{ TO_NUMBER(uintptr_t, 1); }

static size_t unpack_vararg(lua_State* L, int i, char* to)
{
    void* p;
    struct ctype ct;

    switch (lua_type(L, i)) {
    case LUA_TBOOLEAN:
        *(int*) to = lua_toboolean(L, i);
        return sizeof(int);

    case LUA_TNUMBER:
        *(double*) to = lua_tonumber(L, i);
        return sizeof(double);

    case LUA_TSTRING:
        *(const char**) to = lua_tostring(L, i);
        return sizeof(const char*);

    case LUA_TLIGHTUSERDATA:
        *(void**) to = lua_touserdata(L, i);
        return sizeof(void*);

    case LUA_TUSERDATA:
        p = to_cdata(L, i, &ct);

        if (ct.type == INVALID_TYPE) {
            *(void**) to = p;
            return sizeof(void*);
        }

        lua_pop(L, 1);

        if (ct.pointers || ct.type == UINTPTR_TYPE) {
            *(void**) to = p;
            return sizeof(void*);

        } else if (ct.type == INT32_TYPE || ct.type == UINT32_TYPE) {
            *(int32_t*) to = *(int32_t*) p;
            return sizeof(int32_t);

        } else if (ct.type == INT64_TYPE || ct.type == UINT64_TYPE) {
            *(int64_t*) to = *(int64_t*) p;
            return sizeof(int64_t);
        }
        goto err;

    case LUA_TNIL:
        *(void**) to = NULL;
        return sizeof(void*);

    default:
        goto err;
    }

err:
    return type_error(L, i, "vararg", 0, NULL);
}

void unpack_varargs_stack(lua_State* L, int first, int last, char* to)
{
    int i;

    for (i = first; i <= last; i++) {
        to += unpack_vararg(L, i, to);
    }
}

void unpack_varargs_stack_skip(lua_State* L, int first, int last, int ints_to_skip, int floats_to_skip, char* to)
{
    int i;

    for (i = first; i <= last; i++) {
        int type = lua_type(L, i);

        if (type == LUA_TNUMBER && --floats_to_skip >= 0) {
            continue;
        } else if (type != LUA_TNUMBER && --ints_to_skip >= 0) {
            continue;
        }

        to += unpack_vararg(L, i, to);
    }
}

void unpack_varargs_float(lua_State* L, int first, int last, int max, char* to)
{
    int i;

    for (i = first; i <= last && max > 0; i++) {
        if (lua_type(L, i) == LUA_TNUMBER) {
            unpack_vararg(L, i, to);
            to += sizeof(double);
            max--;
        }
    }
}

void unpack_varargs_int(lua_State* L, int first, int last, int max, char* to)
{
    int i;

    for (i = first; i <= last && max > 0; i++) {
        if (lua_type(L, i) != LUA_TNUMBER) {
            unpack_vararg(L, i, to);
            to += sizeof(void*);
            max--;
        }
    }
}

void unpack_varargs_reg(lua_State* L, int first, int last, char* to)
{
    int i;

    for (i = first; i <= last; i++) {
        unpack_vararg(L, i, to);
        to += sizeof(double);
    }
}

/* to_enum tries to convert a value at idx to the enum type indicated by to_ct
 * and uv to_usr. For strings this means it will do a string lookup for the
 * enum type. It leaves the stack unchanged. Will throw an error if the type
 * at idx can't be conerted.
 */
int32_t to_enum(lua_State* L, int idx, int to_usr, const struct ctype* to_ct)
{
    int32_t ret;

    switch (lua_type(L, idx)) {
    case LUA_TSTRING:
        /* lookup string in to_usr to find value */
        to_usr = lua_absindex(L, to_usr);
        lua_pushvalue(L, idx);
        lua_rawget(L, to_usr);

        if (lua_isnil(L, -1)) {
            goto err;
        }

        ret = (int32_t) lua_tointeger(L, -1);
        lua_pop(L, 1);
        return ret;

    case LUA_TUSERDATA:
        return to_int32(L, idx);

    case LUA_TNIL:
        return (int32_t) 0;

    default:
        goto err;
    }

err:
    return type_error(L, idx, NULL, to_usr, to_ct);
}

/* to_pointer tries converts a value at idx to a pointer. It fills out ct and
 * pushes the uv of the found type. It will throw a lua error if it can not
 * convert the value to a pointer. */
static void* to_pointer(lua_State* L, int idx, struct ctype* ct)
{
    void* p;
    memset(ct, 0, sizeof(*ct));
    ct->pointers = 1;
    idx = lua_absindex(L, idx);

    switch (lua_type(L, idx)) {
    case LUA_TNIL:
        ct->type = VOID_TYPE;
        ct->is_null = 1;
        lua_pushnil(L);
        return NULL;

    case LUA_TLIGHTUSERDATA:
        ct->type = VOID_TYPE;
        lua_pushnil(L);
        return lua_touserdata(L, idx);

    case LUA_TSTRING:
        ct->type = INT8_TYPE;
        ct->is_array = 1;
        ct->base_size = 1;
        ct->const_mask = 2;
        lua_pushnil(L);
        return (void*) lua_tolstring(L, idx, &ct->array_size);

    case LUA_TUSERDATA:
        p = to_cdata(L, idx, ct);

        if (ct->type == INVALID_TYPE) {
            /* some other type of user data */
            ct->type = VOID_TYPE;
            lua_pushnil(L);
            return lua_touserdata(L, idx);
        } else if (ct->pointers || ct->type == STRUCT_TYPE || ct->type == UNION_TYPE) {
            return p;
        } else if (ct->type == UINTPTR_TYPE) {
            return *(void**) p;
        }
        break;
    }

    type_error(L, idx, "pointer", 0, NULL);
    return NULL;
}

static int is_void_ptr(const struct ctype* ct)
{
    return ct->type == VOID_TYPE
        && ct->pointers == 1;
}

static int is_same_type(lua_State* L, int usr1, int usr2, const struct ctype* t1, const struct ctype* t2)
{
    return t1->type == t2->type
        && (!IS_COMPLEX(t1->type) || lua_rawequal(L, usr1, usr2));
}

static void set_struct(lua_State* L, int idx, void* to, int to_usr, const struct ctype* tt, int check_pointers);

/* to_typed_pointer converts a value at idx to a type tt with target uv to_usr
 * checking all types. May push a temporary value so that it can create
 * structs on the fly. */
void* to_typed_pointer(lua_State* L, int idx, int to_usr, const struct ctype* tt)
{
    struct ctype ft;
    void* p;

    to_usr = lua_absindex(L, to_usr);
    idx = lua_absindex(L, idx);

    if (tt->pointers == 1 && (tt->type == STRUCT_TYPE || tt->type == UNION_TYPE) && lua_type(L, idx) == LUA_TTABLE) {
        /* need to construct a struct of the target type */
        struct ctype ct = *tt;
        ct.pointers = ct.is_array = 0;
        p = push_cdata(L, to_usr, &ct);
        set_struct(L, idx, p, to_usr, &ct, 1);
        return p;
    }

    p = to_pointer(L, idx, &ft);

    if (tt->pointers == 1 && ft.pointers == 0 && (ft.type == STRUCT_TYPE || ft.type == UNION_TYPE)) {
        /* auto dereference structs */
        ft.pointers = 1;
        ft.const_mask <<= 1;
    }

    if (is_void_ptr(tt)) {
        /* any pointer can convert to void* */
        goto suc;

    } else if (ft.is_null) {
        /* NULL can convert to any pointer */
        goto suc;

    } else if (!is_same_type(L, to_usr, -1, tt, &ft)) {
        /* the base type is different */
        goto err;

    } else if (tt->pointers != ft.pointers) {
        goto err;

    } else if (ft.const_mask & ~tt->const_mask) {
        /* for every const in from it must be in to, there are further rules
         * for const casting (see the c++ spec), but they are hard to test
         * quickly */
        goto err;
    }

suc:
    return p;

err:
    type_error(L, idx, NULL, to_usr, tt);
    return NULL;
}

/* to_cfunction converts a value at idx with usr table at to_usr and type tt
 * into a function. Leaves the stack unchanged. */
static cfunction to_cfunction(lua_State* L, int idx, int to_usr, const struct ctype* tt, int check_pointers)
{
    void* p;
    struct ctype ft;
    cfunction f;
    int top = lua_gettop(L);

    idx = lua_absindex(L, idx);
    to_usr = lua_absindex(L, to_usr);

    switch (lua_type(L, idx)) {
    case LUA_TFUNCTION:
        /* Function cdatas are pinned and must be manually cleaned up by
         * calling func:free(). */
        push_upval(L, &callbacks_key);
        f = compile_callback(L, idx, to_usr, tt);
        lua_pushboolean(L, 1);
        lua_rawset(L, -3);
        lua_pop(L, 1); /* callbacks tbl */
        return f;

    case LUA_TNIL:
        return NULL;

    case LUA_TLIGHTUSERDATA:
        if (check_pointers) {
            goto err;
        } else {
            return (cfunction) lua_touserdata(L, idx);
        }

    case LUA_TUSERDATA:
        p = to_cdata(L, idx, &ft);
        assert(lua_gettop(L) == top + 1);

        if (ft.type == INVALID_TYPE) {
            if (check_pointers) {
                goto err;
            } else {
                lua_pop(L, 1);
                return (cfunction) lua_touserdata(L, idx);
            }

        } else if (ft.is_null) {
            lua_pop(L, 1);
            return NULL;

        } else if (!check_pointers && (ft.pointers || ft.type == UINTPTR_TYPE)) {
            lua_pop(L, 1);
            return (cfunction) *(void**) p;

        } else if (ft.type != FUNCTION_TYPE) {
            goto err;

        } else if (!check_pointers) {
            lua_pop(L, 1);
            return *(cfunction*) p;

        } else if (ft.calling_convention != tt->calling_convention) {
            goto err;

        } else if (!lua_rawequal(L, -1, to_usr)) {
            goto err;

        } else {
            lua_pop(L, 1);
            return *(cfunction*) p;
        }

    default:
        goto err;
    }

err:
    type_error(L, idx, NULL, to_usr, tt);
    return NULL;
}

/* to_type_cfunction converts a value at idx with uv at to_usr and type tt to
 * a cfunction. Leaves the stack unchanged. */
cfunction to_typed_cfunction(lua_State* L, int idx, int to_usr, const struct ctype* tt)
{ return to_cfunction(L, idx, to_usr, tt, 1); }

static void set_value(lua_State* L, int idx, void* to, int to_usr, const struct ctype* tt, int check_pointers);

static void set_array(lua_State* L, int idx, void* to, int to_usr, const struct ctype* tt, int check_pointers)
{
    size_t i, sz, esz;
    struct ctype et;

    assert(!tt->is_variable_array);

    idx = lua_absindex(L, idx);
    to_usr = lua_absindex(L, to_usr);

    switch (lua_type(L, idx)) {
    case LUA_TSTRING:
        if (tt->pointers == 1 && IS_CHAR(tt->type)) {
            const char* str = lua_tolstring(L, idx, &sz);

            if (sz >= tt->array_size) {
                memcpy(to, str, tt->array_size);
            } else {
                /* include nul terminator */
                memcpy(to, str, sz+1);
            }
        } else {
            goto err;
        }
        break;

    case LUA_TTABLE:
        et = *tt;
        et.pointers--;
        et.const_mask >>= 1;
        et.is_array = 0;
        esz = et.pointers ? sizeof(void*) : et.base_size;

        lua_rawgeti(L, idx, 2);

        if (lua_isnil(L, -1)) {
            /* there is no second element, so we set the whole array to the
             * first element (or nil - ie 0) if there is no first element) */
            lua_pop(L, 1);
            lua_rawgeti(L, idx, 1);
            if (lua_isnil(L, -1)) {
                memset(to, 0, ctype_size(L, tt));
            } else {
                for (i = 0; i < tt->array_size; i++) {
                    set_value(L, -1, (char*) to + esz * i, to_usr, &et, check_pointers);
                }
            }
            lua_pop(L, 1);

        } else {
            /* there is a second element, so we set each element using the
             * equiv index in the table initializer */
            lua_pop(L, 1);
            for (i = 0; i < tt->array_size; i++) {
                lua_rawgeti(L, idx, (int) (i+1));

                if (lua_isnil(L, -1)) {
                    /* we've hit the end of the values provided in the
                     * initializer, so memset the rest to zero */
                    lua_pop(L, 1);
                    memset((char*) to + esz * i, 0, (tt->array_size - i) * esz);
                    break;

                } else {
                    set_value(L, -1, (char*) to + esz * i, to_usr, &et, check_pointers);
                    lua_pop(L, 1);
                }
            }
        }
        break;

    default:
        goto err;
    }

    return;

err:
    type_error(L, idx, NULL, to_usr, tt);
}

/* pops the member key from the stack, leaves the member user value on the
 * stack. Returns the member offset. Returns -ve if the member can not be
 * found. */
static int get_member(lua_State* L, int usr, const struct ctype* ct, struct ctype* mt)
{
    int off;
    lua_rawget(L, usr);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return -1;
    }

    *mt = *(const struct ctype*) lua_touserdata(L, -1);
    lua_getuservalue(L, -1);
    lua_replace(L, -2);

    if (mt->is_variable_array) {
        /* eg char mbr[?] */
        size_t sz = (mt->pointers > 1) ? sizeof(void*) : mt->base_size;
        assert(ct->is_variable_struct && ct->variable_size_known && mt->is_array);
        mt->array_size = ct->variable_increment / sz;
        mt->is_variable_array = 0;

    } else if (mt->is_variable_struct) {
        /* eg struct {char a; char b[?]} mbr; */
        assert(ct->is_variable_struct && ct->variable_size_known);
        mt->variable_size_known = 1;
        mt->variable_increment = ct->variable_increment;
    }

    off = mt->offset;
    mt->offset = 0;
    return off;
}

static void set_struct(lua_State* L, int idx, void* to, int to_usr, const struct ctype* tt, int check_pointers)
{
    int have_first = 0;
    int have_other = 0;
    struct ctype mt;
    void* p;

    assert(!tt->is_variable_struct || tt->variable_size_known);

    to_usr = lua_absindex(L, to_usr);
    idx = lua_absindex(L, idx);

    switch (lua_type(L, idx)) {
    case LUA_TTABLE:
        /* match up to the members based off the table initializers key - this
         * will match both numbered and named members in the user table
         * we need a special case for when no entries in the initializer -
         * zero initialize the c struct, and only one entry in the initializer
         * - set all members to this value */
        memset(to, 0, ctype_size(L, tt));
        lua_pushnil(L);
        while (lua_next(L, idx)) {
            int off;

            if (!have_first && lua_tonumber(L, -2) == 1 && lua_tonumber(L, -1) != 0) {
                have_first = 1;
            } else if (!have_other && (lua_type(L, -2) != LUA_TNUMBER || lua_tonumber(L, -2) != 1)) {
                have_other = 1;
            }

            lua_pushvalue(L, -2);
            off = get_member(L, to_usr, tt, &mt);
            assert(off >= 0);
            set_value(L, -2, (char*) to + off, -1, &mt, check_pointers);

            /* initializer value, mt usr */
            lua_pop(L, 2);
        }

        /* if we only had a single non zero value then initialize all members to that value */
        if (!have_other && have_first && tt->type != UNION_TYPE) {
            size_t i, sz;
            int off;
            lua_rawgeti(L, idx, 1);
            sz = lua_rawlen(L, to_usr);

            for (i = 2; i < sz; i++) {
                lua_pushnumber(L, i);
                off = get_member(L, to_usr, tt, &mt);
                assert(off >= 0);
                set_value(L, -2, (char*) to + off, -1, &mt, check_pointers);
                lua_pop(L, 1); /* mt usr */
            }

            lua_pop(L, 1); /* initializer table */
        }
        break;

    case LUA_TUSERDATA:
        if (check_pointers) {
            p = to_typed_pointer(L, idx, to_usr, tt);
        } else {
            struct ctype ct;
            p = to_pointer(L, idx, &ct);
        }
        memcpy(to, p, tt->base_size);
        lua_pop(L, 1);
        break;

    default:
        goto err;
    }

    return;

err:
    type_error(L, idx, NULL, to_usr, tt);
}

static void set_value(lua_State* L, int idx, void* to, int to_usr, const struct ctype* tt, int check_pointers)
{
    int top = lua_gettop(L);

    if (tt->is_array) {
        set_array(L, idx, to, to_usr, tt, check_pointers);

    } else if (tt->pointers) {
        union {
            uint8_t c[sizeof(void*)];
            void* p;
        } u;

        if (lua_istable(L, idx)) {
            luaL_error(L, "Can't set a pointer member to a struct that's about to be freed");
        }

        if (check_pointers) {
            u.p = to_typed_pointer(L, idx, to_usr, tt);
        } else {
            struct ctype ct;
            u.p = to_pointer(L, idx, &ct);
        }

#ifndef ALLOW_MISALIGNED_ACCESS
        if ((uintptr_t) to & PTR_ALIGN_MASK) {
            memcpy(to, u.c, sizeof(void*));
        } else
#endif
        {
            *(void**) to = u.p;
        }

        lua_pop(L, 1);

    } else if (tt->is_bitfield) {

        uint64_t hi_mask = UINT64_C(0) - (UINT64_C(1) << (tt->bit_offset + tt->bit_size));
        uint64_t low_mask = (UINT64_C(1) << tt->bit_offset) - UINT64_C(1);
        uint64_t val = to_uint64(L, idx);
        val &= (UINT64_C(1) << tt->bit_size) - 1;
        val <<= tt->bit_offset;
        *(uint64_t*) to = val | (*(uint64_t*) to & (hi_mask | low_mask));

    } else if (tt->type == STRUCT_TYPE || tt->type == UNION_TYPE) {
        set_struct(L, idx, to, to_usr, tt, check_pointers);

    } else {

#ifndef ALLOW_MISALIGNED_ACCESS
        union {
            uint8_t c[8];
            _Bool b;
            uint64_t u64;
            float f;
            double d;
            cfunction func;
        } misalign;

        void* origto = to;

        if ((uintptr_t) origto & (tt->base_size - 1)) {
            to = misalign.c;
        }
#endif

        switch (tt->type) {
        case BOOL_TYPE:
            *(_Bool*) to = (to_int32(L, idx) != 0);
            break;
        case UINT8_TYPE:
            *(uint8_t*) to = (uint8_t) to_uint32(L, idx);
            break;
        case INT8_TYPE:
            *(int8_t*) to = (int8_t) to_int32(L, idx);
            break;
        case UINT16_TYPE:
            *(uint16_t*) to = (uint16_t) to_uint32(L, idx);
            break;
        case INT16_TYPE:
            *(int16_t*) to = (int16_t) to_int32(L, idx);
            break;
        case UINT32_TYPE:
            *(uint32_t*) to = to_uint32(L, idx);
            break;
        case INT32_TYPE:
            *(int32_t*) to = to_int32(L, idx);
            break;
        case UINT64_TYPE:
            *(uint64_t*) to = to_uint64(L, idx);
            break;
        case INT64_TYPE:
            *(int64_t*) to = to_int64(L, idx);
            break;
        case FLOAT_TYPE:
            *(float*) to = (float) to_double(L, idx);
            break;
        case DOUBLE_TYPE:
            *(double*) to = to_double(L, idx);
            break;
        case UINTPTR_TYPE:
            *(uintptr_t*) to = to_uintptr(L, idx);
            break;
        case ENUM_TYPE:
            *(int32_t*) to = to_enum(L, idx, to_usr, tt);
            break;
        case FUNCTION_TYPE:
            *(cfunction*) to = to_cfunction(L, idx, to_usr, tt, check_pointers);
            break;
        default:
            goto err;
        }

#ifndef ALLOW_MISALIGNED_ACCESS
        if ((uintptr_t) origto & (tt->base_size - 1)) {
            memcpy(origto, misalign.c, tt->base_size);
        }
#endif
    }

    assert(lua_gettop(L) == top);
    return;
err:
    type_error(L, idx, NULL, to_usr, tt);
}

static int ffi_typeof(lua_State* L)
{
    struct ctype ct;
    check_ctype(L, 1, &ct);
    push_ctype(L, -1, &ct);
    return 1;
}

static void setmintop(lua_State* L, int idx)
{
    if (lua_gettop(L) < idx) {
        lua_settop(L, idx);
    }
}

/* warning: in the case that it finds an array size, it removes that index */
static void get_variable_array_size(lua_State* L, int idx, struct ctype* ct)
{
    /* we only care about the variable buisness for the variable array
     * directly ie ffi.new('char[?]') or the struct that contains the variable
     * array ffi.new('struct {char v[?]}'). A pointer to the struct doesn't
     * care about the variable size (it treats it as a zero sized array). */

    if (ct->is_variable_array) {
        assert(ct->is_array);
        ct->array_size = (size_t) luaL_checknumber(L, idx);
        ct->is_variable_array = 0;
        lua_remove(L, idx);

    } else if (ct->is_variable_struct && !ct->variable_size_known) {
        assert(ct->type == STRUCT_TYPE && !ct->is_array);
        ct->variable_increment *= (size_t) luaL_checknumber(L, idx);
        ct->variable_size_known = 1;
        lua_remove(L, idx);
    }
}

static int do_new(lua_State* L, int is_cast)
{
    void* p;
    struct ctype ct;
    int check_ptrs = !is_cast;

    setmintop(L, 3);
    check_ctype(L, 1, &ct);

    /* don't push a callback when we have a c function, as cb:set needs a
     * compiled callback from a lua function to work */
    if (!ct.pointers && ct.type == FUNCTION_TYPE && (lua_isnil(L, 2) || lua_isfunction(L, 2))) {
        /* Function cdatas are pinned and must be manually cleaned up by
         * calling func:free(). */
        compile_callback(L, 2, -1, &ct);
        push_upval(L, &callbacks_key);
        lua_pushvalue(L, -2);
        lua_pushboolean(L, 1);
        lua_rawset(L, -3);
        lua_pop(L, 1); /* callbacks tbl */
        return 1;
    }

    if (!is_cast) {
        get_variable_array_size(L, 2, &ct);
    }

    p = push_cdata(L, -1, &ct);

    /* if the user mt has a __gc function then call ffi.gc on this value */
    if (push_user_mt(L, -2, &ct)) {
        push_upval(L, &gc_key);
        lua_pushvalue(L, -3);

        /* user_mt.__gc */
        lua_pushliteral(L, "__gc");
        lua_rawget(L, -4);

        lua_rawset(L, -3); /* gc_upval[cdata] = user_mt.__gc */
        lua_pop(L, 2); /* user_mt and gc_upval */
    }

    if (!lua_isnil(L, 2)) {
        set_value(L, 2, p, -2, &ct, check_ptrs);
    }

    return 1;
}

static int ffi_new(lua_State* L)
{ return do_new(L, 0); }

static int ffi_cast(lua_State* L)
{ return do_new(L, 1); }

static int ffi_sizeof(lua_State* L)
{
    struct ctype ct;
    check_ctype(L, 1, &ct);
    get_variable_array_size(L, 2, &ct);
    lua_pushnumber(L, ctype_size(L, &ct));
    return 1;
}

static int ffi_alignof(lua_State* L)
{
    struct ctype ct;
    check_ctype(L, 1, &ct);
    lua_pushnumber(L, ct.align_mask + 1);
    return 1;
}

static int ffi_offsetof(lua_State* L)
{
    int off;
    struct ctype ct, mt;
    lua_settop(L, 2);
    check_ctype(L, 1, &ct);

    lua_pushvalue(L, 2);
    off = get_member(L, -2, &ct, &mt); /* this replaces the member key at -1 with the mbr usr value */
    if (off < 0) {
        push_type_name(L, 3, &ct);
        return luaL_error(L, "type %s has no member %s", lua_tostring(L, -1), lua_tostring(L, 2));
    }

    lua_pushnumber(L, off);

    if (!mt.is_bitfield) {
        return 1;
    }

    lua_pushnumber(L, mt.bit_offset);
    lua_pushnumber(L, mt.bit_size);
    return 3;
}

static int ffi_istype(lua_State* L)
{
    struct ctype tt, ft;
    check_ctype(L, 1, &tt);
    to_cdata(L, 2, &ft);

    if (ft.type == INVALID_TYPE) {
        goto fail;
    }

    if (!is_same_type(L, 3, 4, &tt, &ft)) {
        goto fail;
    }

    if (tt.pointers != ft.pointers) {
        goto fail;
    }

    lua_pushboolean(L, 1);
    return 1;

fail:
    lua_pushboolean(L, 0);
    return 0;
}

static int cdata_gc(lua_State* L)
{
    struct ctype ct;
    check_cdata(L, 1, &ct);
    lua_settop(L, 1);

    /* call the gc func if there is any registered */
    lua_pushvalue(L, 1);
    lua_rawget(L, lua_upvalueindex(2));
    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, 1);
        lua_pcall(L, 1, 0, 0);
    }

    /* unset the closure */
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    lua_rawset(L, lua_upvalueindex(1));

    return 0;
}

static int callback_free(lua_State* L)
{
    cfunction* p = (cfunction*) lua_touserdata(L, 1);
    free_code(get_jit(L), L, *p);
    return 0;
}

static int cdata_free(lua_State* L)
{
    struct ctype ct;
    cfunction* p = (cfunction*) check_cdata(L, 1, &ct);
    lua_settop(L, 1);

    /* unset the closure */
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    lua_rawset(L, lua_upvalueindex(1));

    if (ct.is_jitted) {
        free_code(get_jit(L), L, *p);
        *p = NULL;
    }

    return 0;
}

static int cdata_set(lua_State* L)
{
    struct ctype ct;
    cfunction* p = (cfunction*) check_cdata(L, 1, &ct);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (!ct.is_jitted) {
        luaL_error(L, "can't set the function for a non-lua callback");
    }

    if (*p == NULL) {
        luaL_error(L, "can't set the function for a free'd callback");
    }

    push_func_ref(L, *p);
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, CALLBACK_FUNC_USR_IDX);

    /* remove the closure for this callback as it embeds the function pointer
     * value */
    lua_pushvalue(L, 1);
    lua_pushboolean(L, 1);
    lua_rawset(L, lua_upvalueindex(1));

    return 0;
}

static int cdata_call(lua_State* L)
{
    struct ctype ct;
    int top = lua_gettop(L);
    cfunction* p = (cfunction*) check_cdata(L, 1, &ct);

    if (push_user_mt(L, -1, &ct)) {
        lua_pushliteral(L, "__call");
        lua_rawget(L, -2);

        if (!lua_isnil(L, -1)) {
            lua_insert(L, 1);
            lua_pop(L, 2); /* ct_usr, user_mt */
            lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
            return lua_gettop(L);
        }
    }
    if (ct.pointers || ct.type != FUNCTION_TYPE) {
        return luaL_error(L, "only function callbacks are callable");
    }

    lua_pushvalue(L, 1);
    lua_rawget(L, lua_upvalueindex(1));

    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        compile_function(L, *p, -1, &ct);

        assert(lua_gettop(L) == top + 2); /* uv, closure */

        /* closures[func] = closure */
        lua_pushvalue(L, 1);
        lua_pushvalue(L, -2);
        lua_rawset(L, lua_upvalueindex(1));

        lua_replace(L, 1);
    } else {
        lua_replace(L, 1);
    }

    lua_pop(L, 1); /* uv */
    assert(lua_gettop(L) == top);

    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

static int user_mt_key;

static int ffi_metatype(lua_State* L)
{
    struct ctype ct;
    lua_settop(L, 2);

    check_ctype(L, 1, &ct);
    if (lua_type(L, 2) != LUA_TTABLE && lua_type(L, 2) != LUA_TNIL) {
        return luaL_argerror(L, 2, "metatable must be a table or nil");
    }

    lua_pushlightuserdata(L, &user_mt_key);
    lua_pushvalue(L, 2);
    lua_rawset(L, 3); /* user[user_mt_key] = mt */

    /* return the passed in ctype */
    push_ctype(L, 3, &ct);
    return 1;
}

/* push_user_mt returns 1 if the type has a user metatable and pushes it onto
 * the stack, otherwise it returns 0 and pushes nothing */
int push_user_mt(lua_State* L, int ct_usr, const struct ctype* ct)
{
    if (ct->pointers || (ct->type != STRUCT_TYPE && ct->type != UNION_TYPE)) {
        return 0;
    }

    ct_usr = lua_absindex(L, ct_usr);
    lua_pushlightuserdata(L, &user_mt_key);
    lua_rawget(L, ct_usr);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }

    return 1;
}

static int ffi_gc(lua_State* L)
{
    struct ctype ct;
    lua_settop(L, 2);
    check_cdata(L, 1, &ct);

    push_upval(L, &gc_key);
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_rawset(L, -3);

    /* return the cdata back */
    lua_settop(L, 1);
    return 1;
}

/* lookup_cdata_index returns the offset of the found type and user value on
 * the stack if valid. Otherwise returns -ve and doesn't touch the stack.
 */
static int lookup_cdata_index(lua_State* L, int idx, int ct_usr, struct ctype* ct)
{
    struct ctype mt;
    int off;

    ct_usr = lua_absindex(L, ct_usr);

    switch (lua_type(L, idx)) {
    case LUA_TNUMBER:
        /* possibilities are array, pointer */

        if (!ct->pointers || is_void_ptr(ct)) {
            return -1;
        }

        ct->is_array = 0;
        ct->pointers--;
        ct->const_mask >>= 1;

        lua_pushvalue(L, ct_usr);

        return (ct->pointers ? sizeof(void*) : ct->base_size) * (size_t) lua_tonumber(L, 2);

    case LUA_TSTRING:
        /* possibilities are struct/union, pointer to struct/union */

        if ((ct->type != STRUCT_TYPE && ct->type != UNION_TYPE) || ct->is_array || ct->pointers > 1) {
            return -1;
        }

        lua_pushvalue(L, idx);
        off = get_member(L, ct_usr, ct, &mt);
        if (off < 0) {
            return -1;
        }

        *ct = mt;
        return off;

    default:
        return -1;
    }
}

static int cdata_newindex(lua_State* L)
{
    struct ctype tt;
    char* to;
    int off;

    lua_settop(L, 3);

    to = (char*) check_cdata(L, 1, &tt);
    off = lookup_cdata_index(L, 2, -1, &tt);

    if (off < 0) {
        if (!push_user_mt(L, -1, &tt)) {
            goto err;
        }

        lua_pushliteral(L, "__newindex");
        lua_rawget(L, -2);

        if (lua_isnil(L, -1)) {
            goto err;
        }

        lua_insert(L, 1);
        lua_settop(L, 4);
        lua_call(L, 3, LUA_MULTRET);
        return lua_gettop(L);
    }

    if (tt.const_mask & 1) {
        return luaL_error(L, "can't set const data");
    }

    set_value(L, 3, to + off, -1, &tt, 1);
    return 0;

err:
    push_type_name(L, 4, &tt);
    return luaL_error(L, "type %s has no member %s", lua_tostring(L, -1), lua_tostring(L, 2));
}

static int cdata_index(lua_State* L)
{
    void* to;
    struct ctype ct;
    char* data;
    int off;

    lua_settop(L, 2);
    data = (char*) check_cdata(L, 1, &ct);
    assert(lua_gettop(L) == 3);

    /* Callbacks use the same metatable as standard cdata values, but have set
     * and free members. So instead of mt.__index = mt, we do the equiv here. */
    if (!ct.pointers && ct.type == FUNCTION_TYPE) {
        lua_getmetatable(L, 1);
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
        return 1;
    }

    off = lookup_cdata_index(L, 2, -1, &ct);

    if (off < 0) {
        assert(lua_gettop(L) == 3);
        if (!push_user_mt(L, -1, &ct)) {
            goto err;
        }

        lua_pushliteral(L, "__index");
        lua_rawget(L, -2);

        if (lua_isnil(L, -1)) {
            goto err;
        }

        if (lua_istable(L, -1)) {
            lua_pushvalue(L, 2);
            lua_gettable(L, -2);
            return 1;
        }

        lua_insert(L, 1);
        lua_settop(L, 3);
        lua_call(L, 2, LUA_MULTRET);
        return lua_gettop(L);

err:
        push_type_name(L, 3, &ct);
        return luaL_error(L, "type %s has no member %s", lua_tostring(L, -1), lua_tostring(L, 2));
    }

    assert(lua_gettop(L) == 4); /* ct, key, ct_usr, mbr_usr */
    data += off;

    if (ct.is_array) {
        /* push a reference to the array */
        assert(!ct.is_variable_array);
        ct.is_reference = 1;
        to = push_cdata(L, -1, &ct);
        *(void**) to = data;
        return 1;

    } else if (ct.is_bitfield) {

        if (ct.type == UINT64_TYPE || ct.type == INT64_TYPE) {
            struct ctype rt;
            uint64_t val = *(uint64_t*) data;
            val <<= ct.bit_offset;
            val &= (UINT64_C(1) << ct.bit_size) - 1;

            memset(&rt, 0, sizeof(rt));
            rt.base_size = 8;
            rt.type = UINT64_TYPE;
            rt.is_defined = 1;

            to = push_cdata(L, -1, &rt);
            *(uint64_t*) to = val;

            return 1;

        } else if (ct.type == BOOL_TYPE) {
            uint64_t val = *(uint64_t*) data;
            lua_pushboolean(L, (int) (val & (UINT64_C(1) << ct.bit_offset)));
            return 1;

        } else {
            uint64_t val = *(uint64_t*) data;
            val <<= ct.bit_offset;
            val &= (UINT64_C(1) << ct.bit_size) - 1;
            lua_pushnumber(L, val);
            return 1;
        }

    } else if (ct.pointers) {
#ifndef ALLOW_MISALIGNED_ACCESS
        union {
            uint8_t c[8];
            void* p;
        } misalignbuf;

        if ((uintptr_t) data & PTR_ALIGN_MASK) {
            memcpy(misalignbuf.c, data, sizeof(void*));
            data = misalignbuf.c;
        }
#endif
        to = push_cdata(L, -1, &ct);
        *(void**) to = *(void**) data;
        return 1;

    } else if (ct.type == STRUCT_TYPE || ct.type == UNION_TYPE) {
        /* push a reference to the member */
        ct.is_reference = 1;
        to = push_cdata(L, -1, &ct);
        *(void**) to = data;
        return 1;

    } else if (ct.type == FUNCTION_TYPE) {
        cfunction* pf = (cfunction*) push_cdata(L, -1, &ct);
        *pf = *(cfunction*) data;
        return 1;

    } else {
#ifndef ALLOW_MISALIGNED_ACCESS
        union {
            uint8_t c[8];
            double d;
            float f;
            uint64_t u64;
        } misalignbuf;

        assert(ct.base_size <= 8);

        if ((uintptr_t) data & (ct.base_size - 1)) {
            memcpy(misalignbuf.c, data, ct.base_size);
            data = misalignbuf.c;
        }
#endif

        switch (ct.type) {
        case BOOL_TYPE:
            lua_pushboolean(L, *(_Bool*) data);
            break;
        case UINT8_TYPE:
            lua_pushnumber(L, *(uint8_t*) data);
            break;
        case INT8_TYPE:
            lua_pushnumber(L, *(int8_t*) data);
            break;
        case UINT16_TYPE:
            lua_pushnumber(L, *(uint16_t*) data);
            break;
        case INT16_TYPE:
            lua_pushnumber(L, *(int16_t*) data);
            break;
        case UINT32_TYPE:
            lua_pushnumber(L, *(uint32_t*) data);
            break;
        case ENUM_TYPE:
        case INT32_TYPE:
            lua_pushnumber(L, *(int32_t*) data);
            break;
        case UINT64_TYPE:
        case INT64_TYPE:
            to = push_cdata(L, -1, &ct);
            *(int64_t*) to = *(int64_t*) data;
            break;
        case UINTPTR_TYPE:
            to = push_cdata(L, -1, &ct);
            *(uintptr_t*) to = *(uintptr_t*) data;
            break;
        case FLOAT_TYPE:
            lua_pushnumber(L, *(float*) data);
            break;
        case DOUBLE_TYPE:
            lua_pushnumber(L, *(double*) data);
            break;
        default:
            luaL_error(L, "internal error: invalid member type");
        }

        return 1;
    }
}

static int64_t to_intptr(lua_State* L, int idx, struct ctype* ct)
{
    void* p;
    int64_t ret;

    switch (lua_type(L, idx)) {
    case LUA_TNUMBER:
        memset(ct, 0, sizeof(*ct));
        ct->base_size = 8;
        ct->type = INT64_TYPE;
        ret = lua_tonumber(L, idx);
        lua_pushnil(L);
        return ret;

    case LUA_TUSERDATA:
        p = check_cdata(L, idx, ct);

        if (ct->pointers) {
            return (intptr_t) p;
        } else if (ct->type == UINTPTR_TYPE) {
            return *(intptr_t*) p;
        } else if (ct->type == INT64_TYPE) {
            return *(int64_t*) p;
        } else if (ct->type == UINT64_TYPE) {
            return *(int64_t*) p;
        } else if (ct->type == FUNCTION_TYPE) {
            return (intptr_t) *(cfunction*) p;
        } else {
            lua_pop(L, 1);
            goto err;
        }

    default:
        goto err;
    }

err:
    type_error(L, idx, "intptr_t", 0, NULL);
    return 0;
}

static int rank(int type)
{
    switch (type) {
    case UINTPTR_TYPE:
        return sizeof(uintptr_t) >= 8 ? 4 : 1;
    case INT64_TYPE:
        return 2;
    case UINT64_TYPE:
        return 3;
    default:
        return 0;
    }
}

static void push_number(lua_State* L, int usr, const struct ctype* ct, int64_t val)
{
    void* to = push_cdata(L, usr, ct);

    if (ct->type == UINTPTR_TYPE) {
        *(intptr_t*) to = val;
    } else {
        *(int64_t*) to = val;
    }
}

static int cdata_unm(lua_State* L)
{
    struct ctype ct;
    void* to;
    int64_t val;

    lua_settop(L, 1);
    to_cdata(L, 1, &ct);

    if (push_user_mt(L, -1, &ct)) {
        lua_pushliteral(L, "__unm");
        lua_rawget(L, -2);

        if (!lua_isnil(L, -1)) {
            int top = lua_gettop(L);
            lua_pushvalue(L, 1);
            lua_call(L, 1, LUA_MULTRET);
            return lua_gettop(L) - top + 1;
        }
    }

    val = to_intptr(L, 1, &ct);

    if (ct.pointers) {
        luaL_error(L, "can't negate a pointer value");
    } else {
        lua_pushnil(L);
        ct.type = INT64_TYPE;
        ct.base_size = 8;
        to = push_cdata(L, -1, &ct);
        *(int64_t*) to = -val;
    }

    return 1;
}

/* returns -ve if no binop was called otherwise returns the number of return
 * arguments */
static int call_user_binop(lua_State* L, const char* opfield, int lidx, int ridx)
{
    struct ctype lt, rt;

    lidx = lua_absindex(L, lidx);
    ridx = lua_absindex(L, ridx);

    to_cdata(L, lidx, &lt);
    to_cdata(L, ridx, &rt);

    push_type_name(L, -2, &lt);
    lua_pop(L, 1);

    if (push_user_mt(L, -2, &lt)) {
        lua_pushstring(L, opfield);
        lua_rawget(L, -2);

        if (!lua_isnil(L, -1)) {
            int top = lua_gettop(L);
            lua_pushvalue(L, lidx);
            lua_pushvalue(L, ridx);
            lua_call(L, 2, LUA_MULTRET);
            return lua_gettop(L) - top + 1;
        }

        lua_pop(L, 2); /* user_mt and user_mt.op */
    }

    if (push_user_mt(L, -1, &rt)) {
        lua_pushstring(L, opfield);
        lua_rawget(L, -2);

        if (!lua_isnil(L, -1)) {
            int top = lua_gettop(L);
            lua_pushvalue(L, lidx);
            lua_pushvalue(L, ridx);
            lua_call(L, 2, LUA_MULTRET);
            return lua_gettop(L) - top + 1;
        }

        lua_pop(L, 2); /* user_mt and user_mt.op */
    }

    lua_pop(L, 2); /* user values */
    return -1;
}

static int cdata_concat(lua_State* L)
{
    int ret;

    lua_settop(L, 2);

    ret = call_user_binop(L, "__concat", 1, 2);
    if (ret >= 0) {
        return ret;
    }

    return luaL_error(L, "NYI");
}

static int cdata_len(lua_State* L)
{
    struct ctype ct;
    int ret;

    lua_settop(L, 2);

    ret = call_user_binop(L, "__len", 1, 2);
    if (ret >= 0) {
        return ret;
    }

    check_cdata(L, 1, &ct);
    push_type_name(L, -1, &ct);
    return luaL_error(L, "type %s does not implement the __len metamethod", lua_tostring(L, -1));
}

static int cdata_add(lua_State* L)
{
    struct ctype lt, rt;
    int64_t left, right, res;
    void* to;
    int ret;

    lua_settop(L, 2);

    ret = call_user_binop(L, "__add", 1, 2);
    if (ret >= 0) {
        return ret;
    }

    left = to_intptr(L, 1, &lt);
    right = to_intptr(L, 2, &rt);
    assert(lua_gettop(L) == 4);

    /* note due to 2s complement it doesn't matter if we do the addition as int or uint,
     * but the result needs to be uint64_t if either of the sources are */

    if (lt.pointers && rt.pointers) {
        luaL_error(L, "can't add two pointers");

    } else if (lt.pointers) {
        lt.is_array = 0;
        res = left + (lt.pointers > 1 ? sizeof(void*) : lt.base_size) * right;
        to = push_cdata(L, 3, &lt);
        *(intptr_t*) to = res;

    } else if (rt.pointers) {
        rt.is_array = 0;
        res = right + (rt.pointers > 1 ? sizeof(void*) : rt.base_size) * left;
        to = push_cdata(L, 4, &rt);
        *(intptr_t*) to = res;

    } else if (rank(lt.type) > rank(rt.type)) {
        res = left + right;
        push_number(L, 3, &lt, res);

    } else {
        res = left + right;
        push_number(L, 4, &rt, res);
    }

    return 1;
}

static int cdata_sub(lua_State* L)
{
    struct ctype lt, rt;
    int64_t left, right, res;
    void* to;
    int ret;

    lua_settop(L, 2);

    ret = call_user_binop(L, "__sub", 1, 2);
    if (ret >= 0) {
        return ret;
    }

    left = to_intptr(L, 1, &lt);
    right = to_intptr(L, 2, &rt);

    if (rt.pointers) {
        luaL_error(L, "can't subtract a pointer value");

    } else if (lt.pointers) {
        lt.is_array = 0;
        res = left - (lt.pointers > 1 ? sizeof(void*) : lt.base_size) * right;
        to = push_cdata(L, 3, &lt);
        *(intptr_t*) to = res;

    } else if (rank(lt.type) > rank(rt.type)) {
        res = left - right;
        push_number(L, 3, &lt, res);

    } else {
        res = left - right;
        push_number(L, 4, &rt, res);
    }

    return 1;
}

/* TODO fix for unsigned */
#define NUMBER_ONLY_BINOP(OP, OPSTR)                                        \
    struct ctype lt, rt;                                                    \
    int64_t left, right, res;                                               \
    int ret;                                                                \
                                                                            \
    lua_settop(L, 2);                                                       \
                                                                            \
    ret = call_user_binop(L, OPSTR, 1, 2);                                  \
    if (ret >= 0) {                                                         \
        return ret;                                                         \
    }                                                                       \
                                                                            \
    left = to_intptr(L, 1, &lt);                                            \
    right = to_intptr(L, 2, &rt);                                           \
    res = OP(left, right);                                                  \
                                                                            \
    if (lt.pointers || rt.pointers) {                                       \
        luaL_error(L, "can't operate on a pointer value");                  \
    } else if (rank(lt.type) > rank(rt.type)) {                             \
        push_number(L, 3, &lt, res);                                        \
    } else {                                                                \
        push_number(L, 4, &rt, res);                                        \
    }                                                                       \
                                                                            \
    return 1

#define MUL(l,r) l * r
#define DIV(l,r) l / r
#define MOD(l,r) l % r
#define POW(l,r) pow(l, r)

static int cdata_mul(lua_State* L)
{ NUMBER_ONLY_BINOP(MUL, "__mul"); }

static int cdata_div(lua_State* L)
{ NUMBER_ONLY_BINOP(DIV, "__div"); }

static int cdata_mod(lua_State* L)
{ NUMBER_ONLY_BINOP(MOD, "__mod"); }

static int cdata_pow(lua_State* L)
{ NUMBER_ONLY_BINOP(POW, "__pow"); }

#define COMPARE_BINOP(OP, OPSTR)                                            \
    struct ctype lt, rt;                                                    \
    int64_t left, right;                                                    \
    int res;                                                                \
                                                                            \
    lua_settop(L, 2);                                                       \
                                                                            \
    res = call_user_binop(L, OPSTR, 1, 2);                                  \
    if (res >= 0) {                                                         \
        return res;                                                         \
    }                                                                       \
                                                                            \
    left = to_intptr(L, 1, &lt);                                            \
    right = to_intptr(L, 2, &rt);                                           \
                                                                            \
    if (lt.pointers && rt.pointers) {                                       \
        if (is_void_ptr(&lt) || is_void_ptr(&rt) || is_same_type(L, 3, 4, &lt, &rt)) { \
            res = OP((uint64_t) left, (uint64_t) right);                    \
        } else {                                                            \
            goto err;                                                       \
        }                                                                   \
                                                                            \
    } else if (lt.is_null && rt.type == FUNCTION_TYPE) {                    \
        res = OP((uint64_t) left, (uint64_t) right);                        \
                                                                            \
    } else if (rt.is_null && lt.type == FUNCTION_TYPE) {                    \
        res = OP((uint64_t) left, (uint64_t) right);                        \
                                                                            \
    } else if (lt.pointers && rt.type == UINTPTR_TYPE) {                    \
        res = OP((uint64_t) left, (uint64_t) right);                        \
                                                                            \
    } else if (rt.pointers && lt.type == UINTPTR_TYPE) {                    \
        res = OP((uint64_t) left, (uint64_t) right);                        \
                                                                            \
    } else if (rt.pointers || lt.pointers) {                                \
        goto err;                                                           \
                                                                            \
    } else if (lt.type != INT64_TYPE && rt.type != INT64_TYPE) {            \
        res = OP((uint64_t) left, (uint64_t) right);                        \
                                                                            \
    } else if (lt.type != INT64_TYPE) {                                     \
        res = OP((int64_t) (uint64_t) left, right);                         \
                                                                            \
    } else if (rt.type != INT64_TYPE) {                                     \
        res = OP(left, (int64_t) (uint64_t) right);                         \
                                                                            \
    } else {                                                                \
        res = OP(left, right);                                              \
    }                                                                       \
                                                                            \
    lua_pushboolean(L, res);                                                \
    return 1

#define EQ(l, r) (l) == (r)
#define LT(l, r) (l) < (r)
#define LE(l, r) (l) <= (r)

static int cdata_eq(lua_State* L)
{
    COMPARE_BINOP(EQ, "__eq");
err:
    lua_pushboolean(L, 0);
    return 1;
}

static int cdata_lt(lua_State* L)
{
    COMPARE_BINOP(LT, "__lt");
err:
    lua_getuservalue(L, 1);
    lua_getuservalue(L, 2);
    push_type_name(L, -2, &lt);
    push_type_name(L, -2, &lt);
    return luaL_error(L, "trying to compare incompatible types %s and %s", lua_tostring(L, -2), lua_tostring(L, -1));
}

static int cdata_le(lua_State* L)
{
    COMPARE_BINOP(LE, "__le");
err:
    lua_getuservalue(L, 1);
    lua_getuservalue(L, 2);
    push_type_name(L, -2, &lt);
    push_type_name(L, -2, &lt);
    return luaL_error(L, "trying to compare incompatible types %s and %s", lua_tostring(L, -2), lua_tostring(L, -1));
}

static int ctype_tostring(lua_State* L)
{
    struct ctype ct;
    assert(lua_type(L, 1) == LUA_TUSERDATA);
    lua_settop(L, 1);
    check_ctype(L, 1, &ct);
    assert(lua_gettop(L) == 2);
    push_type_name(L, -1, &ct);
    lua_pushfstring(L, "ctype<%s>", lua_tostring(L, -1));

    return 1;
}

static int cdata_tostring(lua_State* L)
{
    struct ctype ct;
    char buf[64];
    void* p = check_cdata(L, 1, &ct);

    if (ct.pointers == 0 && ct.type == UINT64_TYPE) {
        sprintf(buf, "%"PRIu64, *(uint64_t*) p);
        lua_pushstring(L, buf);

    } else if (ct.pointers == 0 && ct.type == INT64_TYPE) {
        sprintf(buf, "%"PRId64, *(uint64_t*) p);
        lua_pushstring(L, buf);

    } else if (ct.pointers == 0 && ct.type == UINTPTR_TYPE) {
        lua_pushfstring(L, "%p", *(uintptr_t*) p);

    } else {
        push_type_name(L, -1, &ct);
        lua_pushfstring(L, "cdata<%s>: %p", lua_tostring(L, -1), p);
    }

    return 1;
}

static int ffi_errno(lua_State* L)
{
    struct jit* jit = get_jit(L);

    if (!lua_isnoneornil(L, 1)) {
        lua_pushnumber(L, jit->last_errno);
        jit->last_errno = luaL_checknumber(L, 1);
    } else {
        lua_pushnumber(L, jit->last_errno);
    }

    return 1;
}

static int ffi_number(lua_State* L)
{
    struct ctype ct;
    void* data = to_cdata(L, 1, &ct);

    if (ct.type != INVALID_TYPE) {
        if (ct.pointers) {
            return 0;

        } else if (ct.type == UINTPTR_TYPE) {
            lua_pushnumber(L, *(uintptr_t*) data);
            return 1;

        } else if (ct.type == UINT64_TYPE) {
            lua_pushnumber(L, *(uint64_t*) data);
            return 1;

        } else if (ct.type == INT64_TYPE) {
            lua_pushnumber(L, *(int64_t*) data);
            return 1;

        } else {
            return 0;
        }

    } else {
        /* call the old _G.tonumber, we use an upvalue as _G.tonumber is set
         * to this function */
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushvalue(L, 1);
        lua_call(L, 1, LUA_MULTRET);
        return 1;
    }
}

static int ffi_string(lua_State* L)
{
    struct ctype ct;
    char* data;
    lua_settop(L, 2);

    data = (char*) check_cdata(L, 1, &ct);

    if (is_void_ptr(&ct)) {
        lua_pushlstring(L, data, (size_t) luaL_checknumber(L, 2));
        return 1;

    } else if (IS_CHAR(ct.type) && ct.pointers == 1) {
        size_t sz;

        if (!lua_isnil(L, 2)) {
            sz = (size_t) luaL_checknumber(L, 2);

        } else if (ct.is_array) {
            char* nul = memchr(data, '\0', ct.array_size);
            sz = nul ? nul - data : ct.array_size;

        } else {
            sz = strlen(data);
        }

        lua_pushlstring(L, data, sz);
        return 1;
    }

    return luaL_error(L, "unable to convert cdata to string");
}

static int ffi_copy(lua_State* L)
{
    struct ctype ft, tt;
    char* to = (char*) to_pointer(L, 1, &tt);
    char* from = (char*) to_pointer(L, 2, &ft);

    if (!lua_isnoneornil(L, 3)) {
        memcpy(to, from, (size_t) luaL_checknumber(L, 3));

    } else if (IS_CHAR(ft.type) && ft.pointers == 1) {
        size_t sz = ft.is_array ? ft.array_size : strlen(from);
        memcpy(to, from, sz);
        to[sz] = '\0';
    }

    return 0;
}

static int ffi_fill(lua_State* L)
{
    struct ctype ct;
    void* to = to_pointer(L, 1, &ct);
    size_t sz = (size_t) luaL_checknumber(L, 2);
    int val = 0;

    if (!lua_isnoneornil(L, 3)) {
        val = (int) luaL_checkinteger(L, 3);
    }

    memset(to, val, sz);
    return 0;
}

static int ffi_abi(lua_State* L)
{
    luaL_checkstring(L, 1);
    push_upval(L, &abi_key);
    lua_pushvalue(L, 1);
    lua_rawget(L, -2);
    lua_pushboolean(L, lua_toboolean(L, -1));
    return 1;
}

static int ffi_load(lua_State* L)
{
    const char* libname = luaL_checkstring(L, 1);
    void** lib = (void**) lua_newuserdata(L, sizeof(void*));

    *lib = LoadLibraryA(libname);

#ifdef LIB_FORMAT_1
    if (!*lib) {
        libname = lua_pushfstring(L, LIB_FORMAT_1, lua_tostring(L, 1));
        *lib = LoadLibraryA(libname);
        lua_pop(L, 1);
    }
#endif

#ifdef LIB_FORMAT_2
    if (!*lib) {
        libname = lua_pushfstring(L, LIB_FORMAT_2, lua_tostring(L, 1));
        *lib = LoadLibraryA(libname);
        lua_pop(L, 1);
    }
#endif

    if (!*lib) {
        return luaL_error(L, "could not load library %s", lua_tostring(L, 1));
    }

    lua_newtable(L);
    lua_setuservalue(L, -2);

    push_upval(L, &cmodule_mt_key);
    lua_setmetatable(L, -2);
    return 1;
}

static int find_function(lua_State* L, int module, int name, int usr, const struct ctype* ct)
{
    size_t i;
    void** libs = (void**) lua_touserdata(L, module);
    size_t num = lua_rawlen(L, module) / sizeof(void*);
    const char* funcname = lua_tostring(L, name);

    module = lua_absindex(L, module);
    name = lua_absindex(L, name);
    usr = lua_absindex(L, usr);

    for (i = 0; i < num; i++) {
        if (libs[i]) {
            cfunction func = (cfunction) GetProcAddressA(libs[i], funcname);

            if (func) {
                compile_function(L, func, usr, ct);

                /* cache the function in this module's user value for next time */
                lua_getuservalue(L, module);
                lua_pushvalue(L, name);
                lua_pushvalue(L, -3);
                lua_rawset(L, 3);
                lua_pop(L, 1);
                return 1;
            }
        }
    }

    return 0;
}

static int cmodule_index(lua_State* L)
{
    const char* funcname;
    struct ctype ct;

    lua_getuservalue(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    /* see if we have already loaded the function */
    if (!lua_isnil(L, -1)) {
        return 1;
    }
    lua_pop(L, 1);

    funcname = luaL_checkstring(L, 2);

    /* find the function type */
    push_upval(L, &functions_key);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    lua_remove(L, -2); /* function tbl */
    if (lua_isnil(L, -1)) {
        luaL_error(L, "missing declaration for function %s", funcname);
    }
    ct = *(const struct ctype*) lua_touserdata(L, -1);
    lua_getuservalue(L, -1);

    if (find_function(L, 1, 2, -1, &ct)) {
        return 1;
    }

#if defined _WIN32 && !defined _WIN64 && (defined __i386__ || defined _M_IX86)
    ct.calling_convention = STD_CALL;
    lua_pushfstring(L, "_%s@%d", funcname, x86_stack_required(L, -1));
    if (find_function(L, 1, -1, -2, &ct)) {
        return 1;
    }
    lua_pop(L, 1);

    ct.calling_convention = FAST_CALL;
    lua_pushfstring(L, "@%s@%d", funcname, x86_stack_required(L, -1));
    if (find_function(L, 1, -1, -2, &ct)) {
        return 1;
    }
    lua_pop(L, 1);
#endif

    return luaL_error(L, "failed to find function %s", funcname);
}

static int jit_gc(lua_State* L)
{
    size_t i;
    struct jit* jit = get_jit(L);
    dasm_free(jit);
    for (i = 0; i < jit->pagenum; i++) {
        FreePage(jit->pages[i], jit->pages[i]->size);
    }
    free(jit->globals);
    return 0;
}

static int ffi_debug(lua_State* L)
{
    lua_newtable(L);

    lua_newtable(L);
    push_upval(L, &ctype_mt_key);
    lua_setfield(L, -2, "ctype_mt");
    push_upval(L, &cdata_mt_key);
    lua_setfield(L, -2, "cdata_mt");
    push_upval(L, &cmodule_mt_key);
    lua_setfield(L, -2, "cmodule_mt");
    push_upval(L, &constants_key);
    lua_setfield(L, -2, "constants");
    lua_rawseti(L, -2, 1);

    lua_newtable(L);
    push_upval(L, &types_key);
    lua_setfield(L, -2, "types");
    lua_rawseti(L, -2, 2);

    /*push_upval(L, &jit_key);
    lua_setfield(L, -2, "jit");*/
    push_upval(L, &gc_key);
    lua_setfield(L, -2, "gc");
    push_upval(L, &callbacks_key);
    lua_setfield(L, -2, "callbacks");
    push_upval(L, &functions_key);
    lua_setfield(L, -2, "functions");
    push_upval(L, &abi_key);
    lua_setfield(L, -2, "abi");
    push_upval(L, &next_unnamed_key);
    lua_setfield(L, -2, "next_unnamed");
    return 1;
}

static const luaL_Reg cdata_mt[] = {
    {"__gc", &cdata_gc},
    {"__call", &cdata_call},
    {"free", &cdata_free},
    {"set", &cdata_set},
    {"__index", &cdata_index},
    {"__newindex", &cdata_newindex},
    {"__add", &cdata_add},
    {"__sub", &cdata_sub},
    {"__mul", &cdata_mul},
    {"__div", &cdata_div},
    {"__mod", &cdata_mod},
    {"__pow", &cdata_pow},
    {"__unm", &cdata_unm},
    {"__eq", &cdata_eq},
    {"__lt", &cdata_lt},
    {"__le", &cdata_le},
    {"__tostring", &cdata_tostring},
    {"__concat", &cdata_concat},
    {"__len", &cdata_len},
    {NULL, NULL}
};

static const luaL_Reg callback_mt[] = {
    {"__gc", &callback_free},
    {NULL, NULL}
};

static const luaL_Reg ctype_mt[] = {
    {"__call", &ffi_new},
    {"__tostring", &ctype_tostring},
    {NULL, NULL}
};

static const luaL_Reg cmodule_mt[] = {
    {"__index", &cmodule_index},
    {NULL, NULL}
};

static const luaL_Reg jit_mt[] = {
    {"__gc", &jit_gc},
    {NULL, NULL}
};

static const luaL_Reg ffi_reg[] = {
    {"cdef", &ffi_cdef},
    {"load", &ffi_load},
    {"new", &ffi_new},
    {"typeof", &ffi_typeof},
    {"cast", &ffi_cast},
    {"metatype", &ffi_metatype},
    {"gc", &ffi_gc},
    {"sizeof", &ffi_sizeof},
    {"alignof", &ffi_alignof},
    {"offsetof", &ffi_offsetof},
    {"istype", &ffi_istype},
    {"errno", &ffi_errno},
    {"string", &ffi_string},
    {"copy", &ffi_copy},
    {"fill", &ffi_fill},
    {"abi", &ffi_abi},
    {"debug", &ffi_debug},
    {NULL, NULL}
};

/* leaves the usr table on the stack */
static void push_builtin(lua_State* L, struct ctype* ct, const char* name, int type, int size, int align)
{
    memset(ct, 0, sizeof(*ct));
    ct->type = type;
    ct->base_size = size;
    ct->align_mask = align;
    ct->is_defined = 1;

    push_upval(L, &types_key);
    lua_pushnil(L);
    push_ctype(L, -1, ct);

    /* stack is at +3: types, usr (nil), ctype */

    lua_setfield(L, -3, name);
    lua_pop(L, 2); /* types, nil usr */
}

static void add_typedef(lua_State* L, const char* from, const char* to)
{
    struct ctype ct;
    struct parser P;
    P.line = 1;
    P.align_mask = DEFAULT_ALIGN_MASK;
    P.next = P.prev = from;

    push_upval(L, &types_key);
    parse_type(L, &P, &ct);
    parse_argument(L, &P, -1, &ct, NULL);
    push_ctype(L, -1, &ct);

    /* stack is at +4: types, type usr, arg usr, ctype */

    lua_setfield(L, -4, to);
    lua_pop(L, 3); /* types, type usr, arg usr */
}

static int setup_upvals(lua_State* L)
{
    struct jit* jit = get_jit(L);

    /* jit setup */
    {
        dasm_init(jit, 64);
#ifdef _WIN32
        {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            jit->align_page_size = si.dwAllocationGranularity - 1;
        }
#else
        jit->align_page_size = sysconf(_SC_PAGE_SIZE) - 1;
#endif
        jit->globals = (void**) malloc(64 * sizeof(void*));
        dasm_setupglobal(jit, jit->globals, 64);
        compile_globals(jit, L);
    }

    /* ffi.C */
    {
#ifdef _WIN32
        size_t sz = sizeof(HMODULE) * 6;
        HMODULE* libs = lua_newuserdata(L, sz);
        memset(libs, 0, sz);

        /* exe */
        GetModuleHandle(NULL);
        /* lua dll */
#ifdef LUA_DLL_NAME
#define STR2(tok) #tok
#define STR(tok) STR2(tok)
        libs[1] = LoadLibraryA(STR(LUA_DLL_NAME));
#undef STR
#undef STR2
#endif

        /* crt */
#ifdef UNDER_CE
        libs[2] = LoadLibraryA("coredll.dll");
#else
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (char*) &_fmode, &libs[2]);
        libs[3] = LoadLibraryA("kernel32.dll");
        libs[4] = LoadLibraryA("user32.dll");
        libs[5] = LoadLibraryA("gdi32.dll");
#endif

        jit->lua_dll = libs[1];
        jit->kernel32_dll = libs[3];

#else /* !_WIN32 */
        size_t sz = sizeof(void*) * 5;
        void** libs = lua_newuserdata(L, sz);
        memset(libs, 0, sz);

        libs[0] = LoadLibraryA(NULL); /* exe */
        libs[1] = LoadLibraryA("libc.so");
#ifdef __GNUC__
        libs[2] = LoadLibraryA("libgcc.so");
#endif
        libs[3] = LoadLibraryA("libm.so");
        libs[4] = LoadLibraryA("libdl.so");
#endif

        /* use the constants table as the base module's uv such that
         * ffi.C.CONTSTANT looks up the constant correctly
         */
        push_upval(L, &constants_key);
        lua_setuservalue(L, -2);

        push_upval(L, &cmodule_mt_key);
        lua_setmetatable(L, -2);

        lua_setfield(L, 1, "C");
    }

    /* setup builtin types */
    {
        struct {char ch; uint16_t v;} a16;
        struct {char ch; uint32_t v;} a32;
        struct {char ch; uint64_t v;} a64;
        struct {char ch; float v;} af;
        struct {char ch; double v;} ad;
        struct {char ch; uintptr_t v;} aptr;
        struct ctype ct;
        /* add void type and NULL constant */
        push_builtin(L, &ct, "void", VOID_TYPE, 0, 0);

        ct.pointers = 1;
        ct.is_null = 1;

        lua_pushnil(L);
        push_upval(L, &constants_key);
        push_cdata(L, -2, &ct);
        lua_setfield(L, -2, "NULL");
        lua_pop(L, 2); /* nil uv, constants */

#define ALIGNOF(S) ((int) ((char*) &S.v - (char*) &S - 1))

        /* add the rest of the builtin types */
        push_builtin(L, &ct, "bool", BOOL_TYPE, sizeof(_Bool), sizeof(_Bool) -1);
        push_builtin(L, &ct, "uint8_t", UINT8_TYPE, 1, 0);
        push_builtin(L, &ct, "int8_t", INT8_TYPE, 1, 0);
        push_builtin(L, &ct, "uint16_t", UINT16_TYPE, 2, ALIGNOF(a16));
        push_builtin(L, &ct, "int16_t", INT16_TYPE, 2, ALIGNOF(a16));
        push_builtin(L, &ct, "uint32_t", UINT32_TYPE, 4, ALIGNOF(a32));
        push_builtin(L, &ct, "int32_t", INT32_TYPE, 4, ALIGNOF(a32));
        push_builtin(L, &ct, "uint64_t", UINT64_TYPE, 8, ALIGNOF(a64));
        push_builtin(L, &ct, "int64_t", INT64_TYPE, 8, ALIGNOF(a64));
        push_builtin(L, &ct, "float", FLOAT_TYPE, 4, ALIGNOF(af));
        push_builtin(L, &ct, "double", DOUBLE_TYPE, 8, ALIGNOF(ad));
        push_builtin(L, &ct, "uintptr_t", UINTPTR_TYPE, sizeof(uintptr_t), ALIGNOF(aptr));
    }

    /* setup builtin typedefs */
    {
        add_typedef(L, "bool", "_Bool");

        if (sizeof(uint32_t) == sizeof(size_t)) {
            add_typedef(L, "uint32_t", "size_t");
            add_typedef(L, "int32_t", "ssize_t");
        } else if (sizeof(uint64_t) == sizeof(size_t)) {
            add_typedef(L, "uint64_t", "size_t");
            add_typedef(L, "int64_t", "ssize_t");
        }

        if (sizeof(int32_t) == sizeof(intptr_t)) {
            add_typedef(L, "int32_t", "intptr_t");
            add_typedef(L, "int32_t", "ptrdiff_t");
        } else if (sizeof(int64_t) == sizeof(intptr_t)) {
            add_typedef(L, "int64_t", "intptr_t");
            add_typedef(L, "int64_t", "ptrdiff_t");
        }

        if (sizeof(uint8_t) == sizeof(wchar_t)) {
            add_typedef(L, "uint8_t", "wchar_t");
        } else if (sizeof(uint16_t) == sizeof(wchar_t)) {
            add_typedef(L, "uint16_t", "wchar_t");
        } else if (sizeof(uint32_t) == sizeof(wchar_t)) {
            add_typedef(L, "uint32_t", "wchar_t");
        }

        if (sizeof(va_list) == sizeof(char*)) {
            add_typedef(L, "char*", "va_list");
        }
    }

    /* setup ABI params table */
    push_upval(L, &abi_key);

#if defined ARCH_X86 || defined ARCH_ARM
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "32bit");
#elif defined ARCH_X64
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "64bit");
#else
#error
#endif

#if defined ARCH_X86 || defined ARCH_X64 || defined ARCH_ARM
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "le");
#else
#error
#endif

#if defined ARCH_X86 || defined ARCH_X64
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "fpu");
#elif defined ARCH_ARM
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "softfp");
#else
#error
#endif
    lua_pop(L, 1); /* abi tbl */


    /* GC table - shouldn't pin cdata values */
    push_upval(L, &gc_key);
    lua_newtable(L);
    lua_pushliteral(L, "k");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);


    /* ffi.os */
#if defined OS_CE
    lua_pushliteral(L, "WindowsCE");
#elif defined OS_WIN
    lua_pushliteral(L, "Windows");
#elif defined OS_OSX
    lua_pushliteral(L, "OSX");
#elif defined OS_LINUX
    lua_pushliteral(L, "Linux");
#elif defined OS_BSD
    lua_pushliteral(L, "BSD");
#elif defined OS_POSIX
    lua_pushliteral(L, "POSIX");
#else
    lua_pushliteral(L, "Other");
#endif
    lua_setfield(L, 1, "os");


    /* ffi.arch */
#if defined ARCH_X86
    lua_pushliteral(L, "x86");
#elif defined ARCH_X64
    lua_pushliteral(L, "x64");
#elif defined ARCH_ARM
    lua_pushliteral(L, "arm");
#else
# error
#endif
    lua_setfield(L, 1, "arch");



    return 0;
}

static void setup_mt(lua_State* L, const luaL_Reg* mt, int upvals)
{
    lua_pushboolean(L, 1);
    lua_setfield(L, -upvals-2, "__metatable");
    luaL_setfuncs(L, mt, upvals);
}

int luaopen_ffi(lua_State* L)
{
    lua_settop(L, 0);

    lua_newtable(L);
    set_upval(L, &niluv_key);

    lua_newtable(L);
    setup_mt(L, ctype_mt, 0);
    set_upval(L, &ctype_mt_key);

    lua_newtable(L);
    set_upval(L, &callbacks_key);

    lua_newtable(L);
    set_upval(L, &gc_key);

    lua_newtable(L);
    push_upval(L, &callbacks_key);
    push_upval(L, &gc_key);
    setup_mt(L, cdata_mt, 2);
    set_upval(L, &cdata_mt_key);

    lua_newtable(L);
    setup_mt(L, callback_mt, 0);
    set_upval(L, &callback_mt_key);

    lua_newtable(L);
    setup_mt(L, cmodule_mt, 0);
    set_upval(L, &cmodule_mt_key);

    memset(lua_newuserdata(L, sizeof(struct jit)), 0, sizeof(struct jit));
    lua_newtable(L);
    setup_mt(L, jit_mt, 0);
    lua_setmetatable(L, -2);
    set_upval(L, &jit_key);

    lua_newtable(L);
    set_upval(L, &constants_key);

    lua_newtable(L);
    set_upval(L, &types_key);

    lua_newtable(L);
    set_upval(L, &functions_key);

    lua_newtable(L);
    set_upval(L, &abi_key);

    lua_pushinteger(L, 1);
    set_upval(L, &next_unnamed_key);

    assert(lua_gettop(L) == 0);

    /* ffi table */
    lua_newtable(L);
    luaL_setfuncs(L, ffi_reg, 0);

    /* setup_upvals(ffi tbl) */
    lua_pushcfunction(L, &setup_upvals);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 0);

    assert(lua_gettop(L) == 1);

    lua_getglobal(L, "tonumber");
    lua_pushcclosure(L, &ffi_number, 1);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "tonumber");
    lua_setfield(L, -2, "number"); /* ffi.number */

    return 1;
}
