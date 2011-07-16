io.stdout:setvbuf('no')
local ffi = require 'ffi'
local dlls = {ffi.load('test_cdecl')}

if ffi.arch == 'x86' and ffi.os == 'Windows' then
    dlls[2] = ffi.load('test_stdcall')
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

int print_i8(char* buf, int8_t val);
int print_u8(char* buf, uint8_t val);
int print_i16(char* buf, int16_t val);
int print_u16(char* buf, uint16_t val);
int print_i32(char* buf, int32_t val);
int print_u32(char* buf, uint32_t val);
int print_i64(char* buf, int64_t val);
int print_u64(char* buf, uint64_t val);
int print_s(char* buf, const char* val);
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
}

local buf = ffi.new('char[256]')

local function checkbuf(type, suffix, ret)
    local str = tostring(test_values[type]):gsub('^cdata%b<>: ', '')
    assert(ffi.string(buf) == str and ret == #str)
end

for i,c in ipairs(dlls) do
    assert(c.add_i8(1,1) == 2)
    assert(c.add_i8(256,1) == 1)
    assert(c.add_i8(127,1) == -128)
    assert(c.add_i8(-120,120) == 0)
    assert(c.add_u8(255,1) == 0)
    assert(c.add_u8(120,120) == 240)
    assert(c.add_u8(-1,0) == 255)
    assert(c.add_i16(2000,4000) == 6000)
    assert(c.add_d(20, 12) == 32)
    assert(c.add_f(40, 32) == 72)

    for suffix, type in pairs{d = 'double', f = 'float', u64 = 'uint64_t', u32 = 'uint32_t', u16 = 'uint16_t', s = 'const char*', p = 'void*'} do
        local test = test_values[type]
        checkbuf(type, suffix, c['print_' .. suffix](buf, test))

        if i == 1 then
            ffi.cdef(align:gsub('SUFFIX', suffix):gsub('TYPE', type):gsub('ALIGN', 0))
        end

        local v = ffi.new('struct align_0_' .. suffix, {0, test})
        checkbuf(type, suffix, c['print_align_0_' .. suffix](buf, v))

        for _,align in ipairs{1,2,4,8,16} do
            if i == 1 then
                ffi.cdef(palign:gsub('SUFFIX', suffix):gsub('TYPE', type):gsub('ALIGN', align))
            end

            local v = ffi.new('struct align_' .. align .. '_' .. suffix, {0, test})
            checkbuf(type, suffix, c['print_align_' .. align .. '_' .. suffix](buf, v))
        end
    end

    local psz = ffi.new('size_t[1]')
    local palign = ffi.new('size_t[1]')
    local function check(type, test, ret)
        assert(ret == #test)
        assert(test == ffi.string(buf))
        assert(tonumber(psz[0]) == ffi.sizeof(type))
        assert(tonumber(palign[0]) == ffi.alignof(type))
    end

    check('struct Date', '1 2 3 4', c.print_date(psz, palign, buf, {1,2,3,4}))
    check('struct Date2', '1 2 3 4', c.print_date2(psz, palign, buf, {1,2,3,4}))
    check('struct sysv1', '1 2 3', c.print_sysv1(psz, palign, buf, {1,2,3}))
    check('struct sysv2', '1 2 3 4 5 6', c.print_sysv2(psz, palign, buf, {1,2,3,4,5,6}))
    check('struct sysv3', '1 2', c.print_sysv3(psz, palign, buf, {1,2}))
    check('union sysv4', '1', c.print_sysv4(psz, palign, buf, {1}))
    check('struct sysv5', '1 2 3', c.print_sysv5(psz, palign, buf, {1,2,3}))
    check('struct sysv6', '1 2 3', c.print_sysv6(psz, palign, buf, {1,2,3}))
    check('struct sysv7', '1 2 3 4 5', c.print_sysv7(psz, palign, buf, {1,2,3,4,5}))
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



print('Test PASSED')

