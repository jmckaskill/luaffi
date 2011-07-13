#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef __cplusplus
# define EXTERN_C extern "C"
#else
# define EXTERN_C extern
#endif

#ifdef _WIN32
#define EXPORT EXTERN_C __declspec(dllexport)
#elif defined __GNUC__
#define EXPORT EXTERN_C __attribute__((visibility("default")))
#else
#define EXPORT EXTERN_C
#endif

#define ADD(TYPE, NAME) \
    EXPORT TYPE NAME(TYPE a, TYPE b); \
    TYPE NAME(TYPE a, TYPE b) { return a + b; }

ADD(int8_t, add_i8)
ADD(uint8_t, add_u8)
ADD(int16_t, add_i16)
ADD(uint16_t, add_u16)
ADD(int32_t, add_i32)
ADD(uint32_t, add_u32)
ADD(int64_t, add_i64)
ADD(uint64_t, add_u64)
ADD(double, add_d)
ADD(float, add_f)

#define PRINT(TYPE, NAME, FORMAT) \
    EXPORT int NAME(char* buf, TYPE val); \
    int NAME(char* buf, TYPE val) {return sprintf(buf, "%" FORMAT, val);}

PRINT(int8_t, print_i8, PRId8)
PRINT(uint8_t, print_u8, PRIu8)
PRINT(int16_t, print_i16, PRId16)
PRINT(uint16_t, print_u16, PRIu16)
PRINT(int32_t, print_i32, PRId32)
PRINT(uint32_t, print_u32, PRIu32)
PRINT(int64_t, print_i64, PRId64)
PRINT(uint64_t, print_u64, PRIu64)
PRINT(double, print_d, "g")
PRINT(float, print_f, "g")
PRINT(const char*, print_s, "s")
PRINT(void*, print_p, "p")

#define ALIGN(TYPE, SUFFIX, FORMAT) \
    struct align_##SUFFIX {         \
        char pad;                   \
        TYPE v;                     \
    };                              \
    EXPORT int print_align_##SUFFIX(char* buf, struct align_##SUFFIX* p);   \
    EXPORT int print_align_##SUFFIX(char* buf, struct align_##SUFFIX* p) {  \
        return sprintf(buf, "%" FORMAT, p->v);                              \
    }

#define ALIGN2(A)                       \
    ALIGN(uint16_t, A##_u16, PRIu16)    \
    ALIGN(uint32_t, A##_u32, PRIu32)    \
    ALIGN(uint64_t, A##_u64, PRIu64)    \
    ALIGN(float, A##_f, "g")            \
    ALIGN(double, A##_d, "g")           \
    ALIGN(const char*, A##_s, "s")      \
    ALIGN(void*, A##_p, "p")

ALIGN2(0)

#pragma pack(push)
#pragma pack(1)
ALIGN2(1)
#pragma pack(2)
ALIGN2(2)
#pragma pack(4)
ALIGN2(4)
#pragma pack(8)
ALIGN2(8)
#pragma pack(16)
ALIGN2(16)
#pragma pack(pop)





