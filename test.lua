-- vim: ts=4 sw=4 sts=4 et tw=78
-- Copyright (c) 2011 James R. McKaskill. See license in ffi.h

io.stdout:setvbuf('no')
local ffi = require 'ffi'
local dlls = {}

dlls.__cdecl = ffi.load('test_cdecl')

if ffi.arch == 'x86' and ffi.os == 'Windows' then
    dlls.__stdcall = ffi.load('test_stdcall')
    dlls.__fastcall = ffi.load('test_fastcall')
end

local function check(a, b)
    --print('check', a, b)
    return _G.assert(a == b)
end

print('Running test')

ffi.cdef [[
int8_t add_i8(int8_t a, int8_t b);
uint8_t add_u8(uint8_t a, uint8_t b);
int16_t add_i16(int16_t a, int16_t b);
uint16_t add_i16(uint16_t a, uint16_t b);
int32_t add_i32(int32_t a, int32_t b);
uint32_t add_u32(uint32_t a, uint32_t b);
int64_t add_i64(int64_t a, int64_t b);
uint64_t add_u64(uint64_t a, uint64_t b);
double add_d(double a, double b);
float add_f(float a, float b);
bool ret_b(bool v);
_Bool ret_b2(_Bool v);

int print_i8(char* buf, int8_t val);
int print_u8(char* buf, uint8_t val);
int print_i16(char* buf, int16_t val);
int print_u16(char* buf, uint16_t val);
int print_i32(char* buf, int32_t val);
int print_u32(char* buf, uint32_t val);
int print_i64(char* buf, int64_t val);
int print_u64(char* buf, uint64_t val);
int print_s(char* buf, const char* val);
int print_b(char* buf, bool val);
int print_b2(char* buf, _Bool val);
int print_d(char* buf, double val);
int print_f(char* buf, float val);
int print_p(char* buf, void* val);
int sprintf(char* buf, const char* format, ...);

// Examples from MSDN

// bit_fields1.cpp
// compile with: /LD
struct Date {
   unsigned short nWeekDay  : 3;    // 0..7   (3 bits)
   unsigned short nMonthDay : 6;    // 0..31  (6 bits)
   unsigned short nMonth    : 5;    // 0..12  (5 bits)
   unsigned short nYear     : 8;    // 0..100 (8 bits)
};

// bit_fields2.cpp
// compile with: /LD
struct Date2 {
   unsigned nWeekDay  : 3;    // 0..7   (3 bits)
   unsigned nMonthDay : 6;    // 0..31  (6 bits)
   unsigned           : 0;    // Force alignment to next boundary.
   unsigned nMonth    : 5;    // 0..12  (5 bits)
   unsigned nYear     : 8;    // 0..100 (8 bits)
};

// Examples from SysV X86 ABI
struct sysv1 {
    int     j:5;
    int     k:6;
    int     m:7;
};

struct sysv2 {
    short   s:9;
    int     j:9;
    char    c;
    short   t:9;
    short   u:9;
    char    d;
};

struct sysv3 {
    char    c;
    short   s:8;
};

union sysv4 {
    char    c;
    short   s:8;
};

struct sysv5 {
    char    c;
    int     :0;
    char    d;
    short   :9;
    char    e;
    char    :0;
};

struct sysv6 {
    char    c;
    int     :0;
    char    d;
    int     :9;
    char    e;
};

struct sysv7 {
    int     j:9;
    short   s:9;
    char    c;
    short   t:9;
    short   u:9;
};

int print_date(size_t* sz, size_t* align, char* buf, struct Date* s);
int print_date2(size_t* sz, size_t* align, char* buf, struct Date2* s);
int print_sysv1(size_t* sz, size_t* align, char* buf, struct sysv1* s);
int print_sysv2(size_t* sz, size_t* align, char* buf, struct sysv2* s);
int print_sysv3(size_t* sz, size_t* align, char* buf, struct sysv3* s);
int print_sysv4(size_t* sz, size_t* align, char* buf, union sysv4* s);
int print_sysv5(size_t* sz, size_t* align, char* buf, struct sysv5* s);
int print_sysv6(size_t* sz, size_t* align, char* buf, struct sysv6* s);
int print_sysv7(size_t* sz, size_t* align, char* buf, struct sysv7* s);

struct fptr {
    int (__cdecl *p)(int);
};
int call_fptr(struct fptr* s, int val);

void set_errno(int val);
int get_errno(void);
]]

local align = [[
struct align_ALIGN_SUFFIX {
    char pad;
    TYPE v;
};

int print_align_ALIGN_SUFFIX(char* buf, struct align_ALIGN_SUFFIX* p);
]]

local palign = [[
#pragma pack(push)
#pragma pack(ALIGN)
]] .. align .. [[
#pragma pack(pop)
]]

local test_values = {
    ['void*'] = ffi.new('char[3]'),
    ['const char*'] = 'foo',
    float = 3.4,
    double = 5.6,
    uint16_t = 65000,
    uint32_t = 700000056,
    uint64_t = 12345678901234,
    bool = true,
    _Bool = false,
}

local buf = ffi.new('char[256]')

local function checkbuf(type, suffix, ret)
    local str = tostring(test_values[type]):gsub('^cdata%b<>: ', '')
    check(ffi.string(buf), str)
    check(ret, #str)
end

local first = true

for convention,c in pairs(dlls) do
    check(c.add_i8(1,1), 2)
    check(c.add_i8(256,1), 1)
    check(c.add_i8(127,1), -128)
    check(c.add_i8(-120,120), 0)
    check(c.add_u8(255,1), 0)
    check(c.add_u8(120,120), 240)
    check(c.add_i16(2000,4000), 6000)
    check(c.add_d(20, 12), 32)
    check(c.add_f(40, 32), 72)
    check(c.ret_b(true), false)
    check(c.ret_b2(false), true)

    for suffix, type in pairs{b = 'bool', b2 = '_Bool', d = 'double', f = 'float', u64 = 'uint64_t', u32 = 'uint32_t', u16 = 'uint16_t', s = 'const char*', p = 'void*'} do
        local test = test_values[type]
        --print('checkbuf', suffix, type, buf, test)
        checkbuf(type, suffix, c['print_' .. suffix](buf, test))

        if first then
            ffi.cdef(align:gsub('SUFFIX', suffix):gsub('TYPE', type):gsub('ALIGN', 0))
        end

        local v = ffi.new('struct align_0_' .. suffix, {0, test})
        checkbuf(type, suffix, c['print_align_0_' .. suffix](buf, v))

        for _,align in ipairs{1,2,4,8,16} do
            if first then
                ffi.cdef(palign:gsub('SUFFIX', suffix):gsub('TYPE', type):gsub('ALIGN', align))
            end

            local v = ffi.new('struct align_' .. align .. '_' .. suffix, {0, test})
            checkbuf(type, suffix, c['print_align_' .. align .. '_' .. suffix](buf, v))
        end
    end

    local psz = ffi.new('size_t[1]')
    local palign = ffi.new('size_t[1]')
    local function check_align(type, test, ret)
        --print('check_align', type, test, ret, ffi.string(buf))
        check(ret, #test)
        check(test, ffi.string(buf))
        --print(psz, psz[0], tonumber, ffi.number)
        check(tonumber(psz[0]), ffi.sizeof(type))
        check(tonumber(palign[0]), ffi.alignof(type))
    end

    check_align('struct Date', '1 2 3 4', c.print_date(psz, palign, buf, {1,2,3,4}))
    check_align('struct Date2', '1 2 3 4', c.print_date2(psz, palign, buf, {1,2,3,4}))
    check_align('struct sysv1', '1 2 3', c.print_sysv1(psz, palign, buf, {1,2,3}))
    check_align('struct sysv2', '1 2 3 4 5 6', c.print_sysv2(psz, palign, buf, {1,2,3,4,5,6}))
    check_align('struct sysv3', '1 2', c.print_sysv3(psz, palign, buf, {1,2}))
    check_align('union sysv4', '1', c.print_sysv4(psz, palign, buf, {1}))
    check_align('struct sysv5', '1 2 3', c.print_sysv5(psz, palign, buf, {1,2,3}))
    check_align('struct sysv6', '1 2 3', c.print_sysv6(psz, palign, buf, {1,2,3}))
    check_align('struct sysv7', '1 2 3 4 5', c.print_sysv7(psz, palign, buf, {1,2,3,4,5}))

    local cbs = [[
    typedef const char* (*__cdecl sfunc)(const char*);
    int call_i(int (*__cdecl func)(int), int arg);
    float call_f(float (*__cdecl func)(float), float arg);
    double call_d(double (*__cdecl func)(double), double arg);
    const char* call_s(sfunc func, const char* arg);
    _Bool call_b(_Bool (*__cdecl func)(_Bool), _Bool arg);
    ]]

    ffi.cdef(cbs:gsub('__cdecl', convention))

    local u3 = ffi.new('uint64_t', 3)
    check(c.call_i(function(a) return 2*a end, 3), 6)
    assert(math.abs(c.call_d(function(a) return 2*a end, 3.2) - 6.4) < 0.0000000001)
    assert(math.abs(c.call_f(function(a) return 2*a end, 3.2) - 6.4) < 0.000001)
    check(ffi.string(c.call_s(function(s) return s + u3 end, 'foobar')), 'bar')
    check(c.call_b(function(v) return not v end, true), false)

    local u2 = ffi.new('uint64_t', 2)
    local cb = ffi.new('sfunc', function(s) return s + u3 end)
    check(ffi.string(cb('foobar')), 'bar')
    check(ffi.string(c.call_s(cb, 'foobar')), 'bar')
    cb:set(function(s) return s + u2 end)
    check(ffi.string(c.call_s(cb, 'foobar')), 'obar')

    local fp = ffi.new('struct fptr')
    assert(fp.p == ffi.C.NULL)
    fp.p = function(a) return 2*a end
    assert(fp.p ~= ffi.C.NULL)
    check(c.call_fptr(fp, 4), 8)
    local suc, err = pcall(function() fp.p:set(function() end) end)
    assert(not suc)
    check(err:gsub('^.*: ',''), "can't set the function for a non-lua callback")

    check(c.call_fptr({function(a) return 3*a end}, 5), 15)

    local suc, err = pcall(c.call_s, function(s) error(ffi.string(s), 0) end, 'my error')
    check(suc, false)
    check(err, 'my error')

    check(ffi.errno(), c.get_errno())
    c.set_errno(3)
    check(ffi.errno(), 3)
    check(c.get_errno(), 3)
    check(ffi.errno(4), 3)
    check(ffi.errno(), 4)
    check(c.get_errno(), 4)

    first = false
end

local c = ffi.C

assert(c.sprintf(buf, "%g", 5.3) == 3 and ffi.string(buf) == '5.3')
assert(c.sprintf(buf, "%d", false) == 1 and ffi.string(buf) == '0')
assert(c.sprintf(buf, "%d%g", false, 6.7) == 4 and ffi.string(buf) == '06.7')

assert(ffi.sizeof('uint32_t[?]', 32) == 32 * 4)
assert(ffi.sizeof(ffi.new('uint32_t[?]', 32)) == 32 * 4)

ffi.cdef [[
struct vls {
    struct {
        char a;
        struct {
            char b;
            char v[?];
        } c;
    } d;
};
]]

assert(ffi.sizeof('struct vls', 3) == 5)
assert(ffi.sizeof(ffi.new('struct vls', 4).d.c) == 5)

ffi.cdef [[ static const int DUMMY = 8 << 2; ]]
assert(ffi.C.DUMMY == 32)

ffi.new('struct {const char* foo;}', {'foo'})

assert(not pcall(function()
    ffi.new('struct {char* foo;}', {'ff'})
end))

local mt = {}
local vls = ffi.new(ffi.metatype('struct vls', mt), 1)

assert(not pcall(function() return vls.key end))

mt.__index = function(vls, key)
    return function(vls, a, b)
        return 'in index ' .. key .. ' ' .. vls.d.a .. ' ' .. a .. ' ' .. b
    end
end

vls.d.a = 3
check(vls:key('a', 'b'), 'in index key 3 a b')

assert(not pcall(function() vls.k = 3 end))

mt.__newindex = function(vls, key, val)
    error('in newindex ' .. key .. ' ' .. vls.d.a .. ' ' .. val, 0)
end

vls.d.a = 4
local suc, err = pcall(function() vls.key = 'val' end)
assert(not suc)
check(err, 'in newindex key 4 val')

local u64 = ffi.typeof('uint64_t')

mt.__add = function(vls, a) return vls.d.a + a end
mt.__sub = function(vls, a) return vls.d.a - a end
mt.__mul = function(vls, a) return vls.d.a * a end
mt.__div = function(vls, a) return vls.d.a / a end
mt.__mod = function(vls, a) return vls.d.a % a end
mt.__pow = function(vls, a) return vls.d.a ^ a end
mt.__eq = function(vls, a) return u64(vls.d.a) == a end
mt.__lt = function(vls, a) return u64(vls.d.a) < a end
mt.__le = function(vls, a) return u64(vls.d.a) <= a end
mt.__call = function(vls, a, b) return '__call', vls.d.a .. a .. (b or 'nil')  end
mt.__unm = function(vls) return -vls.d.a end
mt.__concat = function(vls, a) return vls.d.a .. a end
mt.__len = function(vls) return vls.d.a end

vls.d.a = 5
check(vls + 5, 10)
check(vls - 5, 0)
check(vls * 5, 25)
check(vls / 5, 1)
check(vls % 3, 2)
check(vls ^ 3, 125)
check(vls == u64(4), false)
check(vls == u64(5), true)
check(vls == u64(6), false)
check(vls < u64(4), false)
check(vls < u64(5), false)
check(vls < u64(6), true)
check(vls <= u64(4), false)
check(vls <= u64(5), true)
check(vls <= u64(6), true)
check(-vls, -5)
local a,b = vls('6')
check(a, '__call')
check(b, '56nil')

if _VERSION ~= 'Lua 5.1' then
    check(vls .. 'str', '5str')
    check(#vls, 5)
end

print('Test PASSED')

