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
double add_i64(double a, double b);
float add_u64(float a, float b);

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

struct d_align {
    char a;
    double d;
};
#pragma pack(push)
#pragma pack(1)
struct d_align1 {
    char a;
    double d;
};
#pragma pack(pop)

int print_d_align(char* buf, d_align* p);
int print_d_align1(char* buf, d_align1* p);

int sprintf(char* buf, const char* format, ...);
]]

local buf = ffi.new('char[256]')

for _,c in ipairs(dlls) do
    assert(c.add_i8(1,1) == 2)
    assert(c.add_i8(256,1) == 1)
    assert(c.add_i8(127,1) == -128)
    assert(c.add_i8(-120,120) == 0)
    assert(c.add_u8(255,1) == 0)
    assert(c.add_u8(120,120) == 240)
    assert(c.add_u8(-1,0) == 255)
    assert(c.add_i16(2000,4000) == 6000)

    assert(c.print_s(buf, "foo") == 3 and ffi.string(buf) == 'foo')
    assert(c.print_i8(buf, 3) == 1 and ffi.string(buf) == '3')
    assert(c.print_u8(buf, 200) == 3 and ffi.string(buf) == '200')
    assert(c.print_d(buf, 3.4) == 3 and ffi.string(buf) == '3.4')
    assert(c.print_d_align(buf, ffi.new('d_align', {1, 3.2})) == 3 and ffi.string(buf) == '3.2')
    assert(c.print_d_align1(buf, ffi.new('d_align1', {1, 3.6})) == 3 and ffi.string(buf) == '3.6')
end

local c = ffi.C

assert(c.sprintf(buf, "%g", 5.3) == 3 and ffi.string(buf) == '5.3')
assert(c.sprintf(buf, "%d", false) == 1 and ffi.string(buf) == '0')
assert(c.sprintf(buf, "%d%g", false, 6.7) == 4 and ffi.string(buf) == '06.7')

assert(ffi.alignof('d_align') == 8)
assert(ffi.sizeof('d_align') == 16)
assert(ffi.offsetof('d_align', 'd') == 8)

assert(ffi.alignof('d_align1') == 1)
assert(ffi.sizeof('d_align1') == 9)
assert(ffi.offsetof('d_align1', 'd') == 1)

print('Test PASSED')

