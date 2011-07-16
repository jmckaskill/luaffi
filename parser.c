#include "ffi.h"

typedef enum {
        TOK_NIL,
        TOK_NUMBER,
        TOK_STRING,
        TOK_TOKEN,

        /* the order of these values must match the token strings in lex.c */

        TOK_3_BEGIN,
        TOK_VA_ARG,

        TOK_2_BEGIN,
        TOK_LEFT_SHIFT, TOK_RIGHT_SHIFT, TOK_LOGICAL_AND, TOK_LOGICAL_OR, TOK_LESS_EQUAL,
        TOK_GREATER_EQUAL, TOK_EQUAL, TOK_NOT_EQUAL,

        TOK_1_BEGIN,
        TOK_OPEN_CURLY, TOK_CLOSE_CURLY, TOK_SEMICOLON, TOK_COMMA, TOK_COLON,
        TOK_ASSIGN, TOK_OPEN_PAREN, TOK_CLOSE_PAREN, TOK_OPEN_SQUARE, TOK_CLOSE_SQUARE,
        TOK_DOT, TOK_AMPERSAND, TOK_LOGICAL_NOT, TOK_BITWISE_NOT, TOK_MINUS,
        TOK_PLUS, TOK_STAR, TOK_DIVIDE, TOK_MODULUS, TOK_LESS,
        TOK_GREATER, TOK_BITWISE_XOR, TOK_BITWISE_OR, TOK_QUESTION, TOK_POUND,

        TOK_REFERENCE = TOK_AMPERSAND,
        TOK_MULTIPLY = TOK_STAR,
        TOK_BITWISE_AND = TOK_AMPERSAND,
} etoken_t;

typedef struct {
    etoken_t type;
    int64_t integer;
    const char* str;
    size_t size;
} token_t;

#define IS_LITERAL(TOK, STR) \
  (((TOK).size == sizeof(STR) - 1) && 0 == memcmp((TOK).str, STR, sizeof(STR) - 1))

/* the order of tokens _must_ match the order of the etoken_t enum */

static char tok3[][4] = {
    "...", /* unused ">>=", "<<=", */
};

static char tok2[][3] = {
    ">>", "<<", "&&", "||", "<=",
    ">=", "==", "!=",
    /* unused "+=", "-=", "*=", "/=", "%=", "&=", "^=", "|=", "++", "--", "->", "::", */
};

static char tok1[] = {
    '{', '}', ';', ',', ':',
    '=', '(', ')', '[', ']',
    '.', '&', '!', '~', '-',
    '+', '*', '/', '%', '<',
    '>', '^', '|', '?', '#'
};

static int next_token(lua_State* L, parser_t* P, token_t* tok)
{
    size_t i;
    const char* s = P->next;

    /* UTF8 BOM */
    if (s[0] == '\xEF' && s[1] == '\xBB' && s[2] == '\xBF') {
        s += 3;
    }

    /* consume whitespace and comments */
    for (;;) {
        /* consume whitespace */
        while(*s == '\t' || *s == '\n' || *s == ' ' || *s == '\v' || *s == '\r') {
            if (*s == '\n') {
                P->line++;
            }
            s++;
        }

        /* consume comments */
        if (*s == '/' && *(s+1) == '/') {

            s = strchr(s, '\n');
            if (!s) {
                luaL_error(L, "non-terminated comment");
            }

        } else if (*s == '/' && *(s+1) == '*') {
            s += 2;

            for (;;) {
                if (s[0] == '\0') {
                    luaL_error(L, "non-terminated comment");
                } else if (s[0] == '*' && s[1] == '/') {
                    s += 2;
                    break;
                } else if (s[0] == '\n') {
                    P->line++;
                }
                s++;
            }

        } else if (*s == '\0') {
            tok->type = TOK_NIL;
            return 0;

        } else {
            break;
        }
    }

    P->prev = s;

    for (i = 0; i < sizeof(tok3) / sizeof(tok3[0]); i++) {
        if (s[0] == tok3[i][0] && s[1] == tok3[i][1] && s[2] == tok3[i][2]) {
            tok->type = (etoken_t) (TOK_3_BEGIN + 1 + i);
            P->next = s + 3;
            return 1;
        }
    }

    for (i = 0; i < sizeof(tok2) / sizeof(tok2[0]); i++) {
        if (s[0] == tok2[i][0] && s[1] == tok2[i][1]) {
            tok->type = (etoken_t) (TOK_2_BEGIN + 1 + i);
            P->next = s + 2;
            return 1;
        }
    }

    for (i = 0; i < sizeof(tok1) / sizeof(tok1[0]); i++) {
        if (s[0] == tok1[i]) {
            tok->type = (etoken_t) (TOK_1_BEGIN + 1 + i);
            P->next = s + 1;
            return 1;
        }
    }

    if (*s == '.' || *s == '-' || ('0' <= *s && *s <= '9')) {
        /* number */
        tok->type = TOK_NUMBER;

        /* split out the negative case so we get the full range of bits for
         * unsigned (eg to support 0xFFFFFFFF where sizeof(long) == 4)
         */
        if (*s == '-') {
            tok->integer = strtol(s, (char**) &s, 0);
        } else {
            tok->integer = strtoul(s, (char**) &s, 0);
        }

        while (*s == 'u' || *s == 'U' || *s == 'l' || *s == 'L') {
            s++;
        }

        P->next = s;
        return 1;

    } else if (*s == '\'' || *s == '\"') {
        /* "..." or '...' */
        s++; /* jump over " */

        tok->type = TOK_STRING;
        tok->str = s;

        while (*s != tok->str[0]) {
            
            if (*s == '\0' || (*s == '\\' && *(s+1) == '\0')) {
                return luaL_error(L, "string not finished");
            }

            if (*s == '\\') {
                s++;
            }

            s++;
        }

        tok->size = s - tok->str;
        s++; /* jump over " */
        P->next = s;
        return 1;

    } else if (('a' <= *s && *s <= 'z') || ('A' <= *s && *s <= 'Z') || *s == '_') {
        /* tokens */
        tok->type = TOK_TOKEN;
        tok->str = s;

        while (('a' <= *s && *s <= 'z') || ('A' <= *s && *s <= 'Z') || *s == '_' || ('0' <= *s && *s <= '9')) {
            s++;
        }

        tok->size = s - tok->str;
        P->next = s;
        return 1;

    } else {
        return luaL_error(L, "invalid character %d", P->line);
    }
}

static void require_token(lua_State* L, parser_t* P, token_t* tok)
{
    if (!next_token(L, P, tok)) {
        luaL_error(L, "unexpected end");
    }
}

static void check_token(lua_State* L, parser_t* P, int type, const char* str, const char* err, ...)
{
    token_t tok;
    if (!next_token(L, P, &tok) || tok.type != type || (tok.type == TOK_TOKEN && (tok.size != strlen(str) || memcmp(tok.str, str, tok.size) != 0))) {
        va_list ap;
        va_start(ap, err);
        lua_pushvfstring(L, err, ap);
        lua_error(L);
    }
}

static void put_back(parser_t* P)
{ P->next = P->prev; }


int64_t calculate_constant(lua_State* L, parser_t* P);

static int g_name_key;
static int g_next_unnamed_key;


/* this parses a struct or union starting with the optional
 * name before the opening brace
 * leaves the type usr value on the stack
 */
static void parse_record(lua_State* L, parser_t* P, ctype_t* type)
{
    token_t tok;
    int top = lua_gettop(L);
    int ct_usr = top + 1;

    require_token(L, P, &tok);

    /* name is optional */
    if (tok.type == TOK_TOKEN) {
        /* declaration */
        lua_pushlstring(L, tok.str, tok.size);
        lua_pushvalue(L, -1);
        lua_rawget(L, TYPE_UPVAL);

        if (lua_isnil(L, -1)) {
            lua_pop(L, 1); /* pop the nil */
            lua_newtable(L);

            lua_pushlightuserdata(L, &g_name_key);
            lua_pushvalue(L, -3);
            lua_rawset(L, -3); /* usr[name_key] = name */

            lua_pushvalue(L, -2);
            push_ctype(L, -2, type);
            lua_rawset(L, TYPE_UPVAL); /* type[name] = new_ctype */

        } else {
            /* get the exsting declared type */
            const ctype_t* prevt = (const ctype_t*) lua_touserdata(L, -1);

            if (prevt->type != type->type) {
                luaL_error(L, "type %.*s previously declared as a different type", (int) tok.size, tok.str);
            }

            *type = *prevt;

            /* replace the ctype at idx -1 with its usr value */
            lua_getuservalue(L, -1);
            lua_replace(L, -2);
        }

        /* remove the extra name */
        lua_remove(L, -2);

        assert(lua_gettop(L) == top + 1 && lua_istable(L, -1));

        /* if a name is given then we may be at the end of the string 
         * eg for ffi.new('struct foo')
         */
        if (!next_token(L, P, &tok)) {
            return;
        }

    } else {
        /* create a new unnamed record */
        lua_newtable(L);

        lua_pushlightuserdata(L, &g_next_unnamed_key);
        lua_rawget(L, TYPE_UPVAL);

        lua_pushlightuserdata(L, &g_name_key);
        lua_pushfstring(L, "%d", (int) lua_tonumber(L, -2) + 1);
        lua_rawset(L, -4); /* usr[name_key] = tostring(next + 1) */

        lua_pushlightuserdata(L, &g_next_unnamed_key);
        lua_pushnumber(L, lua_tonumber(L, -2) + 1);
        lua_rawset(L, TYPE_UPVAL); /* type[next] = type[next] + 1 */

        lua_pop(L, 1); /* old next_unnamed */
    }

    /* this may just be a declaration or use of the type as a argument or
     * member */
    if (tok.type != TOK_OPEN_CURLY) {
        put_back(P);
        return;
    }

    if (type->is_defined) {
        luaL_error(L, "redefinition in line %d", P->line);
    }

    assert(lua_gettop(L) == top + 1 && lua_istable(L, -1));

    if (type->type == ENUM_TYPE) {
        int value = -1;

        for (;;) {
            require_token(L, P, &tok);

            assert(lua_gettop(L) == top + 1);

            if (tok.type == TOK_CLOSE_CURLY) {
                break;
            } else if (tok.type != TOK_TOKEN) {
                luaL_error(L, "unexpected token in enum at line %d", P->line);
            }

            lua_pushlstring(L, tok.str, tok.size);

            require_token(L, P, &tok);

            if (tok.type == TOK_COMMA || tok.type == TOK_CLOSE_CURLY) {
                /* we have an auto calculated enum value */
                value++;
            } else if (tok.type == TOK_ASSIGN) {
                /* we have an explicit enum value */
                value = (int) calculate_constant(L, P);
                require_token(L, P, &tok);
            } else {
                luaL_error(L, "unexpected token in enum at line %d", P->line);
            }

            assert(lua_gettop(L) == top + 2);

            /* add the enum value to the constants table */
            lua_pushvalue(L, -1);
            lua_pushnumber(L, value);
            lua_rawset(L, CONSTANTS_UPVAL);

            assert(lua_gettop(L) == top + 2);

            /* add the enum value to the enum usr value table */
            lua_pushnumber(L, value);
            lua_rawset(L, ct_usr);
            
            if (tok.type == TOK_CLOSE_CURLY) {
                break;
            } else if (tok.type != TOK_COMMA) {
                luaL_error(L, "unexpected token in enum at line %d", P->line);
            }
        }

        assert(lua_gettop(L) == top + 1);

    } else {
        int midx = 1;

        /* parse members */
        for (;;) {
            ctype_t mbase;

            assert(lua_gettop(L) == top + 1);

            /* see if we're at the end of the struct */
            require_token(L, P, &tok);
            if (tok.type == TOK_CLOSE_CURLY) {
                break;
            } else if (type->is_variable_struct) {
                luaL_error(L, "can't have members after a variable sized member on line %d", P->line);
            } else {
                put_back(P);
            }

            /* members are of the form
             * <base type> <arg>, <arg>, <arg>;
             * eg struct foo bar, *bar2[2];
             * mbase is 'struct foo'
             * mtype is '' then '*[2]'
             * mname is 'bar' then 'bar2'
             */

            parse_type(L, P, &mbase);

            for (;;) {
                size_t malign, msize, mnamesz;
                ctype_t mtype = mbase;
                const char* mname;

                assert(lua_gettop(L) == top + 2);
                mname = parse_argument(L, P, -1, &mtype, &mnamesz);
                assert(lua_gettop(L) == top + 3);

                if (!mtype.is_defined && (mtype.pointers - mtype.is_array) == 0) {
                    luaL_error(L, "member type is undefined on line %d", P->line);
                }

                if (mtype.type == VOID_TYPE && (mtype.pointers - mtype.is_array) == 0) {
                    luaL_error(L, "member type can not be void on line %d", P->line);
                }

                malign = (mtype.pointers - mtype.is_array) ? PTR_ALIGN_MASK : mtype.align_mask;

                if ((mtype.is_variable_struct || mtype.is_variable_array) && type->type != STRUCT_TYPE) {
                    luaL_error(L, "NYI: variable sized members in unions");
                }


                if (mtype.is_variable_array) {
                    msize = 0;
                    type->is_variable_struct = 1;
                    type->variable_increment = mtype.pointers > 1 ? sizeof(void*) : mtype.base_size;

                } else if (mtype.is_variable_struct) {
                    assert(!mtype.variable_size_known);
                    msize = mtype.base_size;
                    type->is_variable_struct = 1;
                    type->variable_increment = mtype.variable_increment;

                } else if (mtype.is_array) {
                    msize = mtype.array_size * (mtype.pointers ? sizeof(void*) : mtype.base_size);

                } else {
                    msize = mtype.pointers ? sizeof(void*) : mtype.base_size;
                }



                /* P->align_mask specifies the max alignment due to the current pack setting */
                if (malign > P->align_mask) {
                    malign = P->align_mask;
                }

                /* increase the outer struct/union alignment if needed */
                if (type->align_mask < malign) {
                    type->align_mask = (unsigned int) malign;
                }

                /* increase the union size if needed */
                if (type->type == UNION_TYPE && type->base_size < msize) {
                    type->base_size = msize;
                }

                /* set the mbr offset and increase the struct size */
                if (type->type == STRUCT_TYPE) {
                    mtype.offset = ALIGN(type->base_size, malign);
                    type->base_size = mtype.offset + msize;
                }


                if (mname) {
                    lua_pushlstring(L, mname, mnamesz);
                    push_ctype(L, -2, &mtype);

                    if (type->type == STRUCT_TYPE) {
                        /* usrvalue[mbr index] = pushed mtype */
                        lua_pushvalue(L, -1);
                        lua_rawseti(L, ct_usr, midx++);
                    }

                    assert(lua_gettop(L) == top + 5);

                    /* set usrvalue[mname] = pushed mtype */
                    lua_rawset(L, ct_usr);

                } else if (mtype.type == STRUCT_TYPE || mtype.type == UNION_TYPE) {
                    /* With an unnamed member we copy all of the submembers
                     * into our usr value adjusting the offset as necessary */

                    /* the numeric indexes are used for passing structs as
                     * arguments, this is only possible when none of the
                     * records are unions 
                     */
                    if (type->type == STRUCT_TYPE && mtype.type == STRUCT_TYPE) {
                        size_t i, sublen = lua_rawlen(L, -1);
                        for (i = 0; i < sublen; i++) {
                            lua_rawgeti(L, -1, (int) i);
                            lua_rawseti(L, ct_usr, midx++);
                        }
                    }

                    /* copy over the values from the inner user value with a
                     * string key including updating its offset note we need
                     * to create a new ctype when updating the ctype as ctypes
                     * are immutable */
                    lua_pushnil(L);
                    while (lua_next(L, -2)) {
                        if (lua_type(L, -2) == LUA_TSTRING) {
                            ctype_t ct = *(const ctype_t*) lua_touserdata(L, -1);
                            ct.offset += mtype.offset;
                            lua_getuservalue(L, -1);

                            /* uservalue[sub_mname] = new_sub_mtype */
                            lua_pushvalue(L, -3);
                            push_ctype(L, -2, &ct);
                            lua_rawset(L, ct_usr);

                            lua_pop(L, 1); /* remove submember user value */
                        }
                        lua_pop(L, 1);
                    }
                }
                /* we ignore unnamed members that aren't structs or unions -
                 * these are there to change the padding */

                /* pop the usr value from push_argument */
                lua_pop(L, 1);
                assert(lua_gettop(L) == top + 2);

                require_token(L, P, &tok);
                if (tok.type == TOK_SEMICOLON) {
                    break;
                } else if (tok.type != TOK_COMMA) {
                    luaL_error(L, "unexpected token in struct definition on line %d", P->line);
                } else if (type->is_variable_struct) {
                    luaL_error(L, "can't have members after a variable sized member on line %d", P->line);
                }
            }

            /* pop the usr value from push_type */
            lua_pop(L, 1);
        }

        /* only void is allowed 0 size */
        if (type->base_size == 0) {
            type->base_size = 1;
        }

        type->base_size = ALIGN(type->base_size, type->align_mask);
    }

    assert(lua_gettop(L) == top + 1);
    set_defined(L, -1, type);
    assert(lua_gettop(L) == top + 1);
}

/* parses out the base type of a type expression in a function declaration,
 * struct definition, typedef etc
 * 
 * leaves the usr value of the type on the stack
 */
void parse_type(lua_State* L, parser_t* P, ctype_t* type)
{
    token_t tok;
    int top = lua_gettop(L);

    memset(type, 0, sizeof(*type));

    require_token(L, P, &tok);

    /* get const/volatile before the base type */
    for (;;) {
        if (tok.type != TOK_TOKEN) {
            luaL_error(L, "unexpected value before type name on line %d", P->line);

        } else if (IS_LITERAL(tok, "const") || IS_LITERAL(tok, "volatile")) {
            /* ignored for now */
            require_token(L, P, &tok);

        } else {
            break;
        }
    }

    /* get base type */
    if (tok.type != TOK_TOKEN) {
        luaL_error(L, "unexpected value before type name on line %d", P->line);

    } else if (IS_LITERAL(tok, "struct")) {
        type->type = STRUCT_TYPE;
        parse_record(L, P, type);

    } else if (IS_LITERAL(tok, "union")) {
        type->type = UNION_TYPE;
        parse_record(L, P, type);

    } else if (IS_LITERAL(tok, "enum")) {
        type->type = ENUM_TYPE;
        parse_record(L, P, type);

    } else {
        int flags = 0;

        enum {
            UNSIGNED = 0x01,
            SIGNED = 0x02,
            LONG = 0x04,
            SHORT = 0x08,
            INT = 0x10,
            CHAR = 0x20,
            LONG_LONG = 0x40,
            INT32 = 0x80,
            DOUBLE = 0x100,
            FLOAT = 0x200,
            COMPLEX = 0x400,
        };

        /* we have to manually decode the builtin types since they can take up
         * more then one token
         */
        for (;;) {
            if (tok.type != TOK_TOKEN) {
                break;
            } else if (IS_LITERAL(tok, "unsigned")) {
                flags |= UNSIGNED;
            } else if (IS_LITERAL(tok, "signed")) {
                flags |= SIGNED;
            } else if (IS_LITERAL(tok, "short")) {
                flags |= SHORT;
            } else if (IS_LITERAL(tok, "char")) {
                flags |= CHAR;
            } else if (IS_LITERAL(tok, "long")) {
                flags |= (flags & LONG) ? LONG_LONG : LONG;
            } else if (IS_LITERAL(tok, "int")) {
                flags |= INT;
            } else if (IS_LITERAL(tok, "__int32")) {
                flags |= INT32;
            } else if (IS_LITERAL(tok, "__int64")) {
                flags |= LONG_LONG;
            } else if (IS_LITERAL(tok, "double")) {
                flags |= DOUBLE;
            } else if (IS_LITERAL(tok, "float")) {
                flags |= FLOAT;
            } else if (IS_LITERAL(tok, "complex")) {
                flags |= COMPLEX;
            } else {
                break;
            }

            if (!next_token(L, P, &tok)) {
                break;
            }
        }

        if (flags) {
            put_back(P);
        }

        if (flags & CHAR) {
            if (flags & SIGNED) {
                lua_pushliteral(L, "int8_t");
            } else if (flags & UNSIGNED) {
                lua_pushliteral(L, "uint8_t");
            } else {
                lua_pushstring(L, (((char) -1) > 0) ? "uint8_t" : "int8_t");
            }
        
        } else if (flags & INT32) {
            if (flags & UNSIGNED) {
                lua_pushliteral(L, "uint32_t");
            } else {
                lua_pushliteral(L, "int32_t");
            }

        } else if (flags & COMPLEX) {
            luaL_error(L, "NYI complex");

        } else if (flags & DOUBLE) {
            if (flags & LONG) {
                luaL_error(L, "NYI: long double");
            } else {
                lua_pushliteral(L, "double");
            }

        } else if (flags & FLOAT) {
            lua_pushliteral(L, "float");
        
        } else if (flags & LONG_LONG) {
            if (flags & UNSIGNED) {
                lua_pushliteral(L, "uint64_t");
            } else {
                lua_pushliteral(L, "int64_t");
            }

        } else if (flags & SHORT) {
#define SHORT_TYPE(u) (sizeof(short) == sizeof(int64_t) ? u "int64_t" : sizeof(short) == sizeof(int32_t) ? u "int32_t" : u "int16_t")
            if (flags & UNSIGNED) {
                lua_pushstring(L, SHORT_TYPE("u"));
            } else {
                lua_pushstring(L, SHORT_TYPE(""));
            }
#undef SHORT_TYPE

        } else if (flags & LONG) {
#define LONG_TYPE(u) (sizeof(long) == sizeof(int64_t) ? u "int64_t" : u "int32_t")
            if (flags & UNSIGNED) {
                lua_pushstring(L, LONG_TYPE("u"));
            } else {
                lua_pushstring(L, LONG_TYPE(""));
            }
#undef LONG_TYPE

        } else if (flags) {
#define INT_TYPE(u) (sizeof(int) == sizeof(int64_t) ? u "int64_t" : sizeof(int) == sizeof(int32_t) ? u "int32_t" : u "int16_t")
            if (flags & UNSIGNED) {
                lua_pushstring(L, INT_TYPE("u"));
            } else {
                lua_pushstring(L, INT_TYPE(""));
            }
#undef INT_TYPE

        } else {
            lua_pushlstring(L, tok.str, tok.size);
        }

        /* lookup type */
        lua_rawget(L, TYPE_UPVAL);
        if (lua_isnil(L, -1)) {
            luaL_error(L, "unknown type %.*s on line %d", (int) tok.size, tok.str, P->line);
        }

        *type = *(const ctype_t*) lua_touserdata(L, -1);
        lua_getuservalue(L, -1);
        lua_replace(L, -2);
    }

    while (next_token(L, P, &tok)) {
        if (tok.type != TOK_TOKEN) {
            put_back(P);
            break;

        } else if (IS_LITERAL(tok, "const") || IS_LITERAL(tok, "volatile")) {
            /* ignore for now */

        } else {
            put_back(P);
            break;
        }
    }

    assert(lua_gettop(L) == top + 1 && (lua_istable(L, -1) || lua_isnil(L, -1)));
}

static void append_type_name(luaL_Buffer* B, int usr, const ctype_t* ct)
{
    size_t i;
    lua_State* L = B->L;

    switch (ct->type) {
    case ENUM_TYPE:
        luaL_addstring(B, "enum ");
        goto get_name;

    case STRUCT_TYPE:
        luaL_addstring(B, "struct ");
        goto get_name;

    case UNION_TYPE:
        luaL_addstring(B, "union ");
        goto get_name;

    case FUNCTION_TYPE:
    get_name:
        lua_pushlightuserdata(L, &g_name_key);
        lua_rawget(L, usr);
        luaL_addvalue(B);
        break;

    case VOID_TYPE:
        luaL_addstring(B, "void");
        break;
    case DOUBLE_TYPE:
        luaL_addstring(B, "double");
        break;
    case FLOAT_TYPE:
        luaL_addstring(B, "float");
        break;
    case INT8_TYPE:
        luaL_addstring(B, "char");
        break;
    case UINT8_TYPE:
        luaL_addstring(B, "unsigned char");
        break;
    case INT16_TYPE:
        luaL_addstring(B, "short");
        break;
    case UINT16_TYPE:
        luaL_addstring(B, "unsigned short");
        break;
    case INT32_TYPE:
        luaL_addstring(B, "int");
        break;
    case UINT32_TYPE:
        luaL_addstring(B, "unsigned int");
        break;
    case INT64_TYPE:
        luaL_addstring(B, "int64_t");
        break;
    case UINT64_TYPE:
        luaL_addstring(B, "uint64_t");
        break;

    case UINTPTR_TYPE:
        if (sizeof(uintptr_t) == sizeof(uint32_t)) {
            luaL_addstring(B, "unsigned int");
        } else if (sizeof(uintptr_t) == sizeof(uint64_t)) {
            luaL_addstring(B, "uint64_t");
        } else {
            luaL_error(L, "internal error - bad type");
        }
        break;

    default:
        luaL_error(L, "internal error - bad type");
    }

    for (i = 0; i < ct->pointers - ct->is_array; i++) {
        luaL_addchar(B, '*');
    }

    if (ct->is_array) {
        lua_pushfstring(L, "[%d]", (int) ct->array_size);
        luaL_addvalue(B);
    }
}

void push_type_name(lua_State* L, int usr, const ctype_t* ct)
{
    luaL_Buffer B;
    usr = lua_absindex(L, usr);
    luaL_buffinit(L, &B);
    append_type_name(&B, usr, ct);
    luaL_pushresult(&B);
}

static void push_function_type_string(lua_State* L, int usr, const ctype_t* ct)
{
    size_t i, args;
    luaL_Buffer B;
    int top = lua_gettop(L);
    usr = lua_absindex(L, usr);

    /* return type */
    lua_rawgeti(L, usr, 0);
    lua_getuservalue(L, -1);

    /* note push the arg and user value below the indexes used by the buffer
     * and use indexes relative to top to avoid problems due to the buffer
     * system pushing a variable number of arguments onto the stack */
    luaL_buffinit(L, &B);
    append_type_name(&B, top+2, (const ctype_t*) lua_touserdata(L, top+1));

    switch (ct->calling_convention) {
    case STD_CALL:
        luaL_addstring(&B, " (__stdcall *)(");
        break;
    case FAST_CALL:
        luaL_addstring(&B, " (__fastcall *)(");
        break;
    case C_CALL:
        luaL_addstring(&B, " (*)(");
        break;
    default:
        luaL_error(L, "internal error - unknown calling convention");
    }

    /* arguments */
    args = lua_rawlen(L, usr);
    for (i = 1; i <= args; i++) {
        if (i > 1) {
            luaL_addstring(&B, ", ");
        }

        lua_rawgeti(L, usr, (int) i);
        lua_replace(L, top+1);
        lua_getuservalue(L, top+1);
        lua_replace(L, top+2);
        append_type_name(&B, top+2, (const ctype_t*) lua_touserdata(L, top+1));
    }

    luaL_addstring(&B, ")");
    luaL_pushresult(&B);

    lua_remove(L, -2);
    lua_remove(L, -2);
}

/* parses from after the opening paranthesis to after the closing parenthesis
 * leaves the ctype usrvalue on the top of the stack
 */
static void parse_function_arguments(lua_State* L, parser_t* P, ctype_t* ftype, int ret_usr, ctype_t* ret_type)
{
    token_t tok;
    int arg_idx = 1;
    int top = lua_gettop(L);
    int func_usr;

    ret_usr = lua_absindex(L, ret_usr);
   
    /* user table for the function type, at the end we look up the type and if
     * we find another usr table that matches exactly, we dump this one and
     * use that instead
     */
    lua_newtable(L);
    func_usr = lua_gettop(L);

    assert(lua_gettop(L) == top + 1);

    push_ctype(L, ret_usr, ret_type);
    lua_rawseti(L, func_usr, 0);

    for (;;) {
        ctype_t arg_type;

        assert(lua_gettop(L) == top + 1);

        require_token(L, P, &tok);

        if (tok.type == TOK_CLOSE_PAREN) {
            break;
        }
        
        if (arg_idx > 1) {
            if (tok.type == TOK_COMMA) {
                require_token(L, P, &tok);
            } else {
                luaL_error(L, "unexpected token in function argument %d on line %d", arg_idx, P->line);
            }
        }

        if (tok.type == TOK_VA_ARG) {
            ftype->has_var_arg = true;
            check_token(L, P, TOK_CLOSE_PAREN, "", "unexpected token in function argument %d on line %d", arg_idx, P->line);
            break;

        } else if (tok.type == TOK_TOKEN) {
            put_back(P);
            parse_type(L, P, &arg_type);
            parse_argument(L, P, -1, &arg_type, NULL);

            /* array arguments are just treated as their base pointer type */
            arg_type.is_array = 0;

            /* check for the c style int func(void) and error on other uses of arguments of type void */
            if (arg_type.type == VOID_TYPE && arg_type.pointers == 0) {
                if (arg_idx > 1) {
                    luaL_error(L, "can't have argument of type void on line %d", P->line);
                }

                check_token(L, P, TOK_CLOSE_PAREN, "", "unexpected token in function argument %d on line %d", arg_idx, P->line);
                lua_pop(L, 2);
                break;
            }

            assert(lua_gettop(L) == top + 3);

            push_ctype(L, -1, &arg_type);
            lua_rawseti(L, func_usr, arg_idx++);

            /* pop the type and argument usr values */
            lua_pop(L, 2);

        } else {
            luaL_error(L, "unexpected token in function argument %d on line %d", arg_idx, P->line);
        }
    }

    assert(lua_gettop(L) == top + 1 && lua_istable(L, -1));

    push_function_type_string(L, -1, ftype);
    assert(lua_gettop(L) == top + 2);

    /* top+1 is the usr table
     * top+2 is the type string
     */

    lua_pushvalue(L, top+2);
    lua_rawget(L, TYPE_UPVAL);

    if (lua_isnil(L, -1)) {
        lua_pushvalue(L, top+2);
        lua_pushvalue(L, top+1);
        lua_rawset(L, TYPE_UPVAL); /* type[name] = usr */

        lua_pushlightuserdata(L, &g_name_key);
        lua_pushvalue(L, top+2);
        lua_rawset(L, top+1); /* usr[name_key] = name */
    } else {
        /* use the usr value from the type table */
        lua_replace(L, top+1);
    }

    lua_settop(L, top+1);
    assert(lua_istable(L, -1));
}

/* parses after the main base type of a typedef, function argument or
 * struct/union member
 * eg for const void* bar[3] the base type is void with the subtype so far of
 * const, this parses the "* bar[3]" and updates the type argument
 *
 * ct_usr and type must be as filled out by parse_type
 * 
 * pushes the updated user value on the top of the stack
 */
const char* parse_argument(lua_State* L, parser_t* P, int ct_usr, ctype_t* type, size_t* namesz)
{
    token_t tok;
    const char* name = NULL;
    int top = lua_gettop(L);

    ct_usr = lua_absindex(L, ct_usr);

    for (;;) {
        if (!next_token(L, P, &tok)) {
            /* we've reached the end of the string */
            break;

        } else if (tok.type == TOK_STAR) {
            type->pointers++;

        } else if (tok.type == TOK_REFERENCE) {
            luaL_error(L, "NYI: c++ reference types");

        } else if (tok.type == TOK_OPEN_PAREN) {
            /* we have a function pointer or a function */

            ctype_t ret_type = *type;

            memset(type, 0, sizeof(*type));
            type->base_size = sizeof(void (*)());
            type->align_mask = FUNCTION_ALIGN_MASK;
            type->type = FUNCTION_TYPE;
            type->calling_convention = ret_type.calling_convention;
            type->is_defined = 1;

            ret_type.calling_convention = C_CALL;

            /* if we already have a name then this is a function declaration,
             * if not then this is a function pointer or the function name is
             * wrapped in parentheses */
            if (!name) {
                require_token(L, P, &tok);

                for (;;) {
                    if (tok.type == TOK_CLOSE_PAREN) {
                        break;

                    } else if (tok.type == TOK_STAR) {
                        type->pointers++;

                    } else if (tok.type != TOK_TOKEN) {
                        luaL_error(L, "unexpected token in function on line %d", P->line);

                    } else if (IS_LITERAL(tok, "__cdecl")) {
                        type->calling_convention = C_CALL;

                    } else if (IS_LITERAL(tok, "__stdcall")) {
                        type->calling_convention = STD_CALL;

                    } else if (IS_LITERAL(tok, "__fastcall")) {
                        type->calling_convention = FAST_CALL;

                    } else if (IS_LITERAL(tok, "const") || IS_LITERAL(tok, "volatile")) {
                        /* ignored for now */

                    } else {
                        name = tok.str;

                        if (namesz) {
                            *namesz = tok.size;
                        }

                        /* check that next is a close paran to ensure we've
                         * got the right name and not some unknown attribute
                         */
                        check_token(L, P, TOK_CLOSE_PAREN, "", "unexpected token after name on line %d", P->line);
                        break;
                    }

                    require_token(L, P, &tok);
                }

                /* function declarations and single function pointers are just
                 * FUNCTION_TYPE
                 */
                if (type->pointers > 0) {
                    type->pointers--;
                }

                check_token(L, P, TOK_OPEN_PAREN, "", "unexpected token in function on line %d", P->line);
            }

            parse_function_arguments(L, P, type, ct_usr, &ret_type);
            return name;

        } else if (tok.type == TOK_OPEN_SQUARE) {
            /* array */
            type->is_array = 1;
            type->pointers++;
            require_token(L, P, &tok);

            if (type->pointers == 1 && !type->is_defined) {
                luaL_error(L, "array of undefined type on line %d", P->line);
            }

            if (type->is_variable_struct || type->is_variable_array) {
                luaL_error(L, "can't have an array of a variably sized type on line %d", P->line);
            }

            if (tok.type == TOK_QUESTION) {
                type->is_variable_array = 1;
                type->variable_increment = (type->pointers > 1) ? sizeof(void*) : type->base_size;

            } else {
                int64_t asize;
                put_back(P);
                asize = calculate_constant(L, P);
                if (asize < 0) {
                    luaL_error(L, "array size can not be negative on line %d", P->line);
                }
                type->array_size = (size_t) asize;
            }

            check_token(L, P, TOK_CLOSE_SQUARE, "", "invalid character in array on line %d", P->line);
            break;
           
        } else if (tok.type == TOK_COLON) {
            luaL_error(L, "NYI bitfields");
           
        } else if (tok.type != TOK_TOKEN) {
            /* we've reached the end of the declaration */
            put_back(P);
            break;

        } else if (IS_LITERAL(tok, "__cdecl")) {
            type->calling_convention = C_CALL;

        } else if (IS_LITERAL(tok, "__stdcall")) {
            type->calling_convention = STD_CALL;

        } else if (IS_LITERAL(tok, "__fastcall")) {
            type->calling_convention = FAST_CALL;

        } else if (IS_LITERAL(tok, "const") || IS_LITERAL(tok, "volatile")) {
            /* ignored for now */

        } else {
            name = tok.str;
            if (namesz) {
                *namesz = tok.size;
            }

            /* check that next is not a token to ensure we've got the right
             * name and not some unknown attribute
             */
            if (next_token(L, P, &tok)) {
                if (tok.type == TOK_TOKEN) {
                    luaL_error(L, "unexpected token after name on line %d", P->line);
                }
                put_back(P);
            }
        }
    }

    type->calling_convention = C_CALL;
    lua_pushvalue(L, ct_usr);
    assert(lua_gettop(L) == top + 1 && (lua_istable(L, -1) || lua_isnil(L, -1)));
    return name;
}

static void parse_typedef(lua_State* L, parser_t* P)
{
    token_t tok;
    ctype_t base_type;
    int top = lua_gettop(L);

    parse_type(L, P, &base_type);

    for (;;) {
        ctype_t arg_type = base_type;
        const char* name;
        size_t namesz;

        assert(lua_gettop(L) == top + 1);
        name = parse_argument(L, P, -1, &arg_type, &namesz);
        assert(lua_gettop(L) == top + 2);

        if (!name) {
            luaL_error(L, "Can't have a typedef without a name on line %d", P->line);
        } else if (arg_type.is_variable_array) {
            luaL_error(L, "Can't typedef a variable length array on line %d", P->line);
        }

        lua_pushlstring(L, name, namesz);
        push_ctype(L, -2, &arg_type);
        lua_rawset(L, TYPE_UPVAL);
        lua_pop(L, 1);

        require_token(L, P, &tok);

        if (tok.type == TOK_SEMICOLON) {
            break;
        } else if (tok.type != TOK_COMMA) {
            luaL_error(L, "Unexpected character in typedef on line %d", P->line);
        }
    }

    lua_pop(L, 1);
    assert(lua_gettop(L) == top);
}

#define END 0
#define PRAGMA_POP 1

static int parse_root(lua_State* L, parser_t* P)
{
    int top = lua_gettop(L);
    token_t tok;

    while (next_token(L, P, &tok)) {
        /* we can have:
         * struct definition
         * enum definition
         * union definition
         * struct/enum/union declaration
         * typedef
         * function declaration
         * pragma pack
         */

        assert(lua_gettop(L) == top);

        if (tok.type == TOK_SEMICOLON) {
            /* empty semicolon in root continue on */

        } else if (tok.type == TOK_POUND) {

            check_token(L, P, TOK_TOKEN, "pragma", "unexpected pre processor directive on line %d", P->line);
            check_token(L, P, TOK_TOKEN, "pack", "unexpected pre processor directive on line %d", P->line);
            check_token(L, P, TOK_OPEN_PAREN, "", "invalid pack directive on line %d", P->line);

            require_token(L, P, &tok);

            if (tok.type == TOK_NUMBER) {
                if (tok.integer != 1 && tok.integer != 2 && tok.integer != 4 && tok.integer != 8 && tok.integer != 16) {
                    luaL_error(L, "pack directive with invalid pack size on line %d", P->line);
                }

                P->align_mask = (size_t) (tok.integer - 1);
                check_token(L, P, TOK_CLOSE_PAREN, "", "invalid pack directive on line %d", P->line);

            } else if (tok.type == TOK_TOKEN && IS_LITERAL(tok, "push")) {
                int line = P->line;
                size_t previous_alignment = P->align_mask;

                check_token(L, P, TOK_CLOSE_PAREN, "", "invalid pack directive on line %d", P->line);

                if (parse_root(L, P) != PRAGMA_POP) {
                    luaL_error(L, "reached end of string without a pragma pop to match the push on line %d", line);
                }

                P->align_mask = previous_alignment;

            } else if (tok.type == TOK_TOKEN && IS_LITERAL(tok, "pop")) {
                check_token(L, P, TOK_CLOSE_PAREN, "", "invalid pack directive on line %d", P->line);
                return PRAGMA_POP;

            } else {
                luaL_error(L, "invalid pack directive on line %d", P->line);
            }


        } else if (tok.type != TOK_TOKEN) {
            return luaL_error(L, "unexpected character on line %d", P->line);

        } else if (IS_LITERAL(tok, "typedef")) {
            parse_typedef(L, P);

        } else {
            /* type declaration, type definition, or function declaration */
            ctype_t type;
            const char* name;
            size_t namesz;

            put_back(P);
            parse_type(L, P, &type);
            name = parse_argument(L, P, -1, &type, &namesz);

            if (type.type < ENUM_TYPE || type.pointers) {
                return luaL_error(L, "unexpected type in root on line %d", P->line);
            }

            require_token(L, P, &tok);

            if (tok.type != TOK_SEMICOLON) {
                return luaL_error(L, "missing semicolon on line %d", P->line);
            }

            /* this was either a function or type declaration/definition - if
             * the latter then the type has already been processed */
            if (type.type == FUNCTION_TYPE && name) {
                lua_pushlstring(L, name, namesz);
                push_ctype(L, -2, &type);
                lua_rawset(L, FUNCTION_UPVAL);
            }

            lua_pop(L, 2);
        }
    }

    return END;
}

int ffi_cdef(lua_State* L)
{
    parser_t P;

    P.line = 1;
    P.prev = P.next = luaL_checkstring(L, 1);
    P.align_mask = DEFAULT_ALIGN_MASK;

    if (parse_root(L, &P) == PRAGMA_POP) {
        luaL_error(L, "pragma pop without an associated push on line %d", P.line);
    }

    return 0;
}

/* calculate_constant handles operator precedence by having a number of
 * recursive commands each of which computes the result at that level of
 * precedence and above. calculate_constant1 is the highest precedence
 */

/* () */
static int64_t calculate_constant1(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t ret;

    if (tok->type == TOK_NUMBER) {
        ret = tok->integer;
        next_token(L, P, tok);
        return ret;

    } else if (tok->type == TOK_TOKEN) {
        /* look up name in constants table */
        lua_pushlstring(L, tok->str, tok->size);
        lua_rawget(L, CONSTANTS_UPVAL);

        if (!lua_isnumber(L, -1)) {
            luaL_error(L, "use of undefined constant %.*s on line %d", (int) tok->size, tok->str, P->line);
        }

        ret = (int64_t) lua_tonumber(L, -1);
        lua_pop(L, 1);
        next_token(L, P, tok);
        return ret;

    } else if (tok->type == TOK_OPEN_PAREN) {
        ret = calculate_constant(L, P);

        require_token(L, P, tok);
        if (tok->type != TOK_CLOSE_PAREN) {
            luaL_error(L, "error whilst parsing constant at line %d", P->line);
        }

        next_token(L, P, tok);
        return ret;

    } else {
        return luaL_error(L, "unexpected token whilst parsing constant at line %d", P->line);
    }
}

/* ! and ~, unary + and -, and sizeof */
static int64_t calculate_constant2(lua_State* L, parser_t* P, token_t* tok)
{
    if (tok->type == TOK_LOGICAL_NOT) {
        require_token(L, P, tok);
        return !calculate_constant2(L, P, tok);

    } else if (tok->type == TOK_BITWISE_NOT) {
        require_token(L, P, tok);
        return ~calculate_constant2(L, P, tok);

    } else if (tok->type == TOK_PLUS) {
        require_token(L, P, tok);
        return calculate_constant2(L, P, tok);

    } else if (tok->type == TOK_MINUS) {
        require_token(L, P, tok);
        return -calculate_constant2(L, P, tok);

    } else if (tok->type == TOK_TOKEN && IS_LITERAL(*tok, "sizeof")) {
        ctype_t type;

        require_token(L, P, tok);
        if (tok->type != TOK_OPEN_PAREN) {
            luaL_error(L, "invalid sizeof at line %d", P->line);
        }

        parse_type(L, P, &type);
        parse_argument(L, P, -1, &type, NULL);
        lua_pop(L, 2);

        require_token(L, P, tok);
        if (tok->type != TOK_CLOSE_PAREN) {
            luaL_error(L, "invalid sizeof at line %d", P->line);
        }

        next_token(L, P, tok);

        return ctype_size(L, &type);

    } else {
        return calculate_constant1(L, P, tok);
    }
}

/* binary * / and % (left associative) */
static int64_t calculate_constant3(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant2(L, P, tok);

    for (;;) {
        if (tok->type == TOK_MULTIPLY) {
            require_token(L, P, tok);
            left *= calculate_constant2(L, P, tok);

        } else if (tok->type == TOK_DIVIDE) {
            require_token(L, P, tok);
            left /= calculate_constant2(L, P, tok);

        } else if (tok->type == TOK_MODULUS) {
            require_token(L, P, tok);
            left %= calculate_constant2(L, P, tok);

        } else {
            return left;
        }
    }
}

/* binary + and - (left associative) */
static int64_t calculate_constant4(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant3(L, P, tok);

    for (;;) {
        if (tok->type == TOK_PLUS) {
            require_token(L, P, tok);
            left += calculate_constant3(L, P, tok);

        } else if (tok->type == TOK_MINUS) {
            require_token(L, P, tok);
            left -= calculate_constant3(L, P, tok);

        } else {
            return left;
        }
    }
}

/* binary << and >> (left associative) */
static int64_t calculate_constant5(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant4(L, P, tok);

    for (;;) {
        if (tok->type == TOK_LEFT_SHIFT) {
            require_token(L, P, tok);
            left <<= calculate_constant4(L, P, tok);

        } else if (tok->type == TOK_RIGHT_SHIFT) {
            require_token(L, P, tok);
            left >>= calculate_constant4(L, P, tok);

        } else {
            return left;
        }
    }
}

/* binary <, <=, >, and >= (left associative) */
static int64_t calculate_constant6(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant5(L, P, tok);

    for (;;) {
        if (tok->type == TOK_LESS) {
            require_token(L, P, tok);
            left = (left < calculate_constant5(L, P, tok));

        } else if (tok->type == TOK_LESS_EQUAL) {
            require_token(L, P, tok);
            left = (left <= calculate_constant5(L, P, tok));

        } else if (tok->type == TOK_GREATER) {
            require_token(L, P, tok);
            left = (left > calculate_constant5(L, P, tok));

        } else if (tok->type == TOK_GREATER_EQUAL) {
            require_token(L, P, tok);
            left = (left >= calculate_constant5(L, P, tok));

        } else {
            return left;
        }
    }
}

/* binary ==, != (left associative) */
static int64_t calculate_constant7(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant6(L, P, tok);

    for (;;) {
        if (tok->type == TOK_EQUAL) {
            require_token(L, P, tok);
            left = (left == calculate_constant6(L, P, tok));

        } else if (tok->type == TOK_NOT_EQUAL) {
            require_token(L, P, tok);
            left = (left != calculate_constant6(L, P, tok));

        } else {
            return left;
        }
    }
}

/* binary & (left associative) */
static int64_t calculate_constant8(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant7(L, P, tok);

    for (;;) {
        if (tok->type == TOK_BITWISE_AND) {
            require_token(L, P, tok);
            left = (left & calculate_constant7(L, P, tok));

        } else {
            return left;
        }
    }
}

/* binary ^ (left associative) */
static int64_t calculate_constant9(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant8(L, P, tok);

    for (;;) {
        if (tok->type == TOK_BITWISE_XOR) {
            require_token(L, P, tok);
            left = (left ^ calculate_constant8(L, P, tok));

        } else {
            return left;
        }
    }
}

/* binary | (left associative) */
static int64_t calculate_constant10(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant9(L, P, tok);

    for (;;) {
        if (tok->type == TOK_BITWISE_OR) {
            require_token(L, P, tok);
            left = (left | calculate_constant9(L, P, tok));

        } else {
            return left;
        }
    }
}

/* binary && (left associative) */
static int64_t calculate_constant11(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant10(L, P, tok);

    for (;;) {
        if (tok->type == TOK_LOGICAL_AND) {
            require_token(L, P, tok);
            left = (left && calculate_constant10(L, P, tok));

        } else {
            return left;
        }
    }
}

/* binary || (left associative) */
static int64_t calculate_constant12(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t left = calculate_constant11(L, P, tok);

    for (;;) {
        if (tok->type == TOK_LOGICAL_OR) {
            require_token(L, P, tok);
            left = (left || calculate_constant11(L, P, tok));

        } else {
            return left;
        }
    }
}

/* ternary ?: (right associative) */
static int64_t calculate_constant13(lua_State* L, parser_t* P, token_t* tok)
{
    int64_t middle, right;
    int64_t left = calculate_constant12(L, P, tok);

    if (tok->type == TOK_QUESTION) {
        require_token(L, P, tok);
        middle = calculate_constant13(L, P, tok);
        right = calculate_constant13(L, P, tok);
        return left ? middle : right;

    } else {
        return left;
    }
}

int64_t calculate_constant(lua_State* L, parser_t* P)
{
    token_t tok;
    int64_t ret;
    require_token(L, P, &tok);
    ret = calculate_constant13(L, P, &tok);

    if (tok.type != TOK_NIL) {
        put_back(P);
    }

    return ret;
}




