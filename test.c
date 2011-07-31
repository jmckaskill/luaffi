/* vim: ts=4 sw=4 sts=4 et tw=78
 * Copyright (c) 2011 James R. McKaskill. See license in ffi.h
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

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

#define ALIGN_UP(TYPE, SUFFIX, FORMAT) \
    struct align_##SUFFIX {         \
        char pad;                   \
        TYPE v;                     \
    };                              \
    EXPORT int print_align_##SUFFIX(char* buf, struct align_##SUFFIX* p);   \
    EXPORT int print_align_##SUFFIX(char* buf, struct align_##SUFFIX* p) {  \
        return sprintf(buf, "%" FORMAT, p->v);                              \
    }

#define ALIGN2(A)                       \
    ALIGN_UP(uint16_t, A##_u16, PRIu16)    \
    ALIGN_UP(uint32_t, A##_u32, PRIu32)    \
    ALIGN_UP(uint64_t, A##_u64, PRIu64)    \
    ALIGN_UP(float, A##_f, "g")            \
    ALIGN_UP(double, A##_d, "g")           \
    ALIGN_UP(const char*, A##_s, "s")      \
    ALIGN_UP(void*, A##_p, "p")

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

#ifdef _MSC_VER
#define alignof(type) __alignof(type)
#else
#define alignof(type) __alignof__(type)
#endif

/* bit_fields1.cpp */
/* compile with: /LD */
struct Date {
   unsigned short nWeekDay  : 3;    /* 0..7   (3 bits) */
   unsigned short nMonthDay : 6;    /* 0..31  (6 bits) */
   unsigned short nMonth    : 5;    /* 0..12  (5 bits) */
   unsigned short nYear     : 8;    /* 0..100 (8 bits) */
};

EXPORT int print_date(size_t* sz, size_t* align, char* buf, struct Date* d);

int print_date(size_t* sz, size_t* align, char* buf, struct Date* d) {
    *sz = sizeof(struct Date);
    *align = alignof(struct Date);
    return sprintf(buf, "%d %d %d %d", d->nWeekDay, d->nMonthDay, d->nMonth, d->nYear);
}

/* bit_fields2.cpp */
/* compile with: /LD */
struct Date2 {
   unsigned nWeekDay  : 3;    /* 0..7   (3 bits) */
   unsigned nMonthDay : 6;    /* 0..31  (6 bits) */
   unsigned           : 0;    /* Force alignment to next boundary. */
   unsigned nMonth    : 5;    /* 0..12  (5 bits) */
   unsigned nYear     : 8;    /* 0..100 (8 bits) */
};

EXPORT int print_date2(size_t* sz, size_t* align, char* buf, struct Date2* d);

int print_date2(size_t* sz, size_t* align, char* buf, struct Date2* d) {
    *sz = sizeof(struct Date2);
    *align = alignof(struct Date2);
    return sprintf(buf, "%d %d %d %d", d->nWeekDay, d->nMonthDay, d->nMonth, d->nYear);
}

// Examples from SysV X86 ABI
struct sysv1 {
    int     j:5;
    int     k:6;
    int     m:7;
};

EXPORT int print_sysv1(size_t* sz, size_t* align, char* buf, struct sysv1* s);

int print_sysv1(size_t* sz, size_t* align, char* buf, struct sysv1* s) {
    *sz = sizeof(struct sysv1);
    *align = alignof(struct sysv1);
    return sprintf(buf, "%d %d %d", s->j, s->k, s->m);
}

struct sysv2 {
    short   s:9;
    int     j:9;
    char    c;
    short   t:9;
    short   u:9;
    char    d;
};

EXPORT int print_sysv2(size_t* sz, size_t* align, char* buf, struct sysv2* s);

int print_sysv2(size_t* sz, size_t* align, char* buf, struct sysv2* s) {
    *sz = sizeof(struct sysv2);
    *align = alignof(struct sysv2);
    return sprintf(buf, "%d %d %d %d %d %d", s->s, s->j, s->c, s->t, s->u, s->d);
}

struct sysv3 {
    char    c;
    short   s:8;
};

EXPORT int print_sysv3(size_t* sz, size_t* align, char* buf, struct sysv3* s);

int print_sysv3(size_t* sz, size_t* align, char* buf, struct sysv3* s) {
    *sz = sizeof(struct sysv3);
    *align = alignof(struct sysv3);
    return sprintf(buf, "%d %d", s->c, s->s);
}

union sysv4 {
    char    c;
    short   s:8;
};

EXPORT int print_sysv4(size_t* sz, size_t* align, char* buf, union sysv4* s);

int print_sysv4(size_t* sz, size_t* align, char* buf, union sysv4* s) {
    *sz = sizeof(union sysv4);
    *align = alignof(union sysv4);
    return sprintf(buf, "%d", s->s);
}

struct sysv5 {
    char    c;
    int     :0;
    char    d;
    short   :9;
    char    e;
    char    :0;
};

EXPORT int print_sysv5(size_t* sz, size_t* align, char* buf, struct sysv5* s);

int print_sysv5(size_t* sz, size_t* align, char* buf, struct sysv5* s) {
    *sz = sizeof(struct sysv5);
    *align = alignof(struct sysv5);
    return sprintf(buf, "%d %d %d", s->c, s->d, s->e);
}

struct sysv6 {
    char    c;
    int     :0;
    char    d;
    int     :9;
    char    e;
};

EXPORT int print_sysv6(size_t* sz, size_t* align, char* buf, struct sysv6* s);

int print_sysv6(size_t* sz, size_t* align, char* buf, struct sysv6* s) {
    *sz = sizeof(struct sysv6);
    *align = alignof(struct sysv6);
    return sprintf(buf, "%d %d %d", s->c, s->d, s->e);
}

struct sysv7 {
    int     j:9;
    short   s:9;
    char    c;
    short   t:9;
    short   u:9;
};

EXPORT int print_sysv7(size_t* sz, size_t* align, char* buf, struct sysv7* s);

int print_sysv7(size_t* sz, size_t* align, char* buf, struct sysv7* s) {
    *sz = sizeof(struct sysv7);
    *align = alignof(struct sysv7);
    return sprintf(buf, "%d %d %d %d %d", s->j, s->s, s->c, s->t, s->u);
}

#define CALL(TYPE, SUFFIX) \
    EXPORT TYPE call_##SUFFIX(TYPE (*func)(TYPE), TYPE arg); \
    EXPORT TYPE call_##SUFFIX(TYPE (*func)(TYPE), TYPE arg) { \
        return func(arg); \
    }

CALL(int, i)
CALL(float, f)
CALL(double, d)
CALL(const char*, s)

struct fptr {
#ifdef _MSC_VER
    int (__cdecl *p)(int);
#else
    int (*p)(int);
#endif
};

EXPORT int call_fptr(struct fptr* s, int val);

int call_fptr(struct fptr* s, int val) {
    return (s->p)(val);
}

EXPORT void set_errno(int val);
EXPORT int get_errno(void);

void set_errno(int val) {
#ifdef _WIN32
    SetLastError(val);
#else
    errno = val;
#endif
}

int get_errno(void) {
#ifdef _WIN32
    return GetLastError();
#else
    return errno;
#endif
}


