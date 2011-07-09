#include "ffi.h"

static int to_define_key;

static void update_on_definition(lua_State* L, int ct_usr, int ct_idx)
{
    ct_usr = lua_absindex(L, ct_usr);
    ct_idx = lua_absindex(L, ct_idx);

    lua_pushlightuserdata(L, &to_define_key);
    lua_rawget(L, ct_usr);

    if (lua_isnil(L, -1)) {
        /* usr[TO_UPDATE_KEY] = setmetatable({}, {__mode='k'}) */
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, WEAK_KEY_MT_UPVAL);
        lua_setmetatable(L, -2);
        lua_pushlightuserdata(L, &to_define_key);
        lua_pushvalue(L, -2);
        lua_rawset(L, ct_usr);
    }

    /* to_update[ctype or cdata] = true */
    lua_pushvalue(L, ct_idx);
    lua_pushboolean(L, 1);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

void set_defined(lua_State* L, int ct_usr, ctype_t* ct)
{
    ct_usr = lua_absindex(L, ct_usr);

    ct->is_defined = 1;

    /* update ctypes and cdatas that were created before the definition came in */
    lua_pushlightuserdata(L, &to_define_key);
    lua_rawget(L, ct_usr);

    if (!lua_isnil(L, -1)) {
        lua_pushnil(L);

        while (lua_next(L, -2)) {
            ctype_t* upd = (ctype_t*) lua_touserdata(L, -2);
            upd->size = ct->size;
            upd->align_mask = ct->align_mask;
            upd->is_defined = 1;
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
        /* usr[TO_UPDATE_KEY] = nil */
        lua_pushlightuserdata(L, &to_define_key);
        lua_pushnil(L);
        lua_rawset(L, ct_usr);
    } else {
        lua_pop(L, 1);
    }
}

void push_ctype(lua_State* L, int ct_usr, const ctype_t* ct)
{
    ctype_t* ret;
    ct_usr = lua_absindex(L, ct_usr);

    ret = (ctype_t*) lua_newuserdata(L, sizeof(ctype_t));
    *ret = *ct;

    lua_pushvalue(L, CTYPE_MT_UPVAL);
    lua_setmetatable(L, -2);

    if (ct->type >= ENUM_TYPE) {
        lua_pushvalue(L, ct_usr);
        lua_setuservalue(L, -2);
    }

    if (!ct->is_defined) {
        update_on_definition(L, ct_usr, -1);
    }
}

void* push_cdata(lua_State* L, int ct_usr, const ctype_t* ct)
{
    cdata_t* cd;
    size_t sz;
    ct_usr = lua_absindex(L, ct_usr);

    if (ct->type == VOID_TYPE && ct->pointers - ct->is_array == 0) {
        luaL_error(L, "can't create cdata of type void");
    } else if (!ct->is_defined && ct->pointers - ct->is_array == 0) {
        luaL_error(L, "can't create cdata for an undefined type");
    }

    if (ct->is_reference) {
        sz = sizeof(void*);
    } else if (ct->pointers - ct->is_array) {
        sz = sizeof(void*) * ct->array_size;
    } else {
        sz = ct->size * ct->array_size;
    }

    cd = (cdata_t*) lua_newuserdata(L, sizeof(cdata_t) + sz);
    *(ctype_t*) &cd->type = *ct;
    memset(cd+1, 0, sz);

    if (ct->type >= ENUM_TYPE) {
        lua_pushvalue(L, ct_usr);
        lua_setuservalue(L, -2);
    }

    lua_pushvalue(L, CDATA_MT_UPVAL);
    lua_setmetatable(L, -2);

    if (!ct->is_defined) {
        update_on_definition(L, ct_usr, -1);
    }

    return cd+1;
}

/* returns the value as a ctype, pushes the user value onto the stack */
void check_ctype(lua_State* L, int idx, ctype_t* ct)
{
    if (lua_isstring(L, idx)) {
        parser_t P;
        P.line = 1;
        P.prev = P.next = lua_tostring(L, idx);
        P.align_mask = DEFAULT_ALIGN_MASK;
        parse_type(L, &P, ct);
        parse_argument(L, &P, -1, ct, NULL);
        lua_remove(L, -2); /* remove the user value from parse_type */
        return;

    } else if (lua_getmetatable(L, idx)) {
        if (!lua_rawequal(L, -1, CTYPE_MT_UPVAL) && !lua_rawequal(L, -1, CDATA_MT_UPVAL)) {
            goto err;
        }

        lua_pop(L, 1); /* pop the metatable */
        *ct = *(ctype_t*) lua_touserdata(L, idx);
        lua_getuservalue(L, idx);
        return;

    } else if (lua_iscfunction(L, idx)) {
        /* cdata functions have a cdata as the first upvalue */
        if (!lua_getupvalue(L, idx, 1) || !lua_getmetatable(L, -1) || !lua_rawequal(L, -1, CDATA_MT_UPVAL)) {
            goto err;
        }

        lua_pop(L, 1); /* pop the metatable */
        *ct = *(ctype_t*) lua_touserdata(L, -1);
        lua_getuservalue(L, -1);
        lua_remove(L, -2); /* pop the ctype */
        return;
    }

err:
    luaL_error(L, "expected cdata, ctype or string for arg #%d", idx);
}

/* if the idx is a cdata returns the cdata_t* and pushes the user value onto
 * the stack, otherwise returns NULL and pushes nothing
 * also dereferences references */
void* to_cdata(lua_State* L, int idx, ctype_t* ct)
{
    cdata_t* cd;

    if (lua_getmetatable(L, idx)) {
        if (!lua_rawequal(L, -1, CDATA_MT_UPVAL)) {
            lua_pop(L, 1);
            return 0;
        }

        lua_pop(L, 1);
        cd = (cdata_t*) lua_touserdata(L, idx);
        lua_getuservalue(L, idx);

    } else if (lua_iscfunction(L, idx) && lua_getupvalue(L, idx, 1)) {
        /* cdata functions have the cdata function pointer as the first upvalue */
        if (!lua_getmetatable(L, -1)) {
            lua_pop(L, 1);
            return 0;
        } else if (!lua_rawequal(L, -1, CDATA_MT_UPVAL)) {
            lua_pop(L, 2);
            return 0;
        }

        lua_pop(L, 1);
        cd = (cdata_t*) lua_touserdata(L, -1);
        lua_getuservalue(L, -1);
        lua_remove(L, -2); /* remove the cdata user data */

    } else {
        return 0;
    }

    *ct = cd->type;

    if (ct->is_reference) {
        ct->is_reference = 0;
        return *(void**) (cd+1);

    } else if (ct->pointers && !ct->is_array) {
        return *(void**) (cd+1);

    } else {
        return cd + 1;
    }
}

/* returns the cdata_t* and pushes the user value onto the stack
 * also dereferences references */
void* check_cdata(lua_State* L, int idx, ctype_t* ct)
{
    void* p = to_cdata(L, idx, ct);
    if (!p) {
        luaL_error(L, "expected cdata for arg #%d", idx);
    }
    return p;
}

