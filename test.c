/* vim: ts=4 sw=4 sts=4 et tw=78
 * Copyright (c) 2011 James R. McKaskill. See license in ffi.h
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

#if __STDC_VERSION__+0 >= 199901L
#include <complex.h>
#define HAVE_COMPLEX
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

EXPORT bool have_complex();

bool have_complex()
{
#ifdef HAVE_COMPLEX
    return 1;
#else
    return 0;
#endif
}

EXPORT bool is_msvc();

bool is_msvc()
{
#ifdef _MSC_VER
    return 1;
#else
    return 0;
#endif
}

EXPORT int test_pow(int v);
int test_pow(int v)
{ return v * v; }

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
#ifdef HAVE_COMPLEX
ADD(double complex, add_dc)
ADD(float complex, add_fc)
#endif

EXPORT _Bool not_b(_Bool v);
EXPORT _Bool not_b2(_Bool v);

_Bool not_b(_Bool v) {return !v;}
_Bool not_b2(_Bool v) {return !v;}

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

#ifdef HAVE_COMPLEX
EXPORT int print_dc(char* buf, double complex val);
EXPORT int print_fc(char* buf, float complex val);
int print_dc(char* buf, double complex val) {return sprintf(buf, "%g+%gi", creal(val), cimag(val));}
int print_fc(char* buf, float complex val) {return sprintf(buf, "%g+%gi", creal(val), cimag(val));}
#endif

EXPORT int print_b(char* buf, _Bool val);
EXPORT int print_b2(char* buf, _Bool val);
int print_b(char* buf, _Bool val) {return sprintf(buf, "%s", val ? "true" : "false");}
int print_b2(char* buf, _Bool val) {return sprintf(buf, "%s", val ? "true" : "false");}

#define OFFSETOF(STRUCT, MEMBER) ((int) ((char*) &STRUCT.MEMBER - (char*) &S - 1))

#define ALIGN_UP(VALUE, ALIGNMENT, SUFFIX) \
    struct align_##ALIGNMENT##_##SUFFIX {   \
        char pad;                   \
        VALUE;                       \
    };                              \
    EXPORT int print_align_##ALIGNMENT##_##SUFFIX(char* buf, struct align_##ALIGNMENT##_##SUFFIX* p);   \
    int print_align_##ALIGNMENT##_##SUFFIX(char* buf, struct align_##ALIGNMENT##_##SUFFIX* p) { \
        struct {char ch; struct align_##ALIGNMENT##_##SUFFIX v;} s; \
        int off = sprintf(buf, "size %d offset %d align %d value ", \
                (int) sizeof(s.v), \
                (int) (((char*) &p->v) - (char*) p), \
                (int) (((char*) &s.v) - (char*) &s)); \
        return print_##SUFFIX(buf+off, p->v); \
    }

#ifdef HAVE_COMPLEX
#define COMPLEX_ALIGN(ALIGNMENT, ATTR) \
    ALIGN_UP(ATTR(double complex), ALIGNMENT, dc) \
    ALIGN_UP(ATTR(float complex), ALIGNMENT, fc)
#else
#define COMPLEX_ALIGN(ALIGNMENT, ATTR)
#endif

#define ALIGN2(ALIGNMENT, ATTR)                   \
    ALIGN_UP(ATTR(uint16_t), ALIGNMENT, u16)      \
    ALIGN_UP(ATTR(uint32_t), ALIGNMENT, u32)      \
    ALIGN_UP(ATTR(uint64_t), ALIGNMENT, u64)      \
    ALIGN_UP(ATTR(float), ALIGNMENT, f)           \
    ALIGN_UP(ATTR(double), ALIGNMENT, d)          \
    ALIGN_UP(ATTR(const char*), ALIGNMENT, s)     \
    ALIGN_UP(ATTR(void*), ALIGNMENT, p)           \
    ALIGN_UP(ATTR(_Bool), ALIGNMENT, b)           \
    ALIGN_UP(ATTR(_Bool), ALIGNMENT, b2)          \
    COMPLEX_ALIGN(ALIGNMENT, ATTR)

#define NO_ATTR(TYPE) TYPE v
ALIGN2(0, NO_ATTR)

#pragma pack(push)
#pragma pack(1)
ALIGN2(1, NO_ATTR)
#pragma pack(2)
ALIGN2(2, NO_ATTR)
#pragma pack(4)
ALIGN2(4, NO_ATTR)
#pragma pack(8)
ALIGN2(8, NO_ATTR)
#pragma pack(16)
ALIGN2(16, NO_ATTR)
#pragma pack(pop)

#ifdef _MSC_VER
#define ATTR_(TYPE, ALIGN) __declspec(align(ALIGN)) TYPE v
#else
#define ATTR_(TYPE, ALIGN) TYPE v __attribute__((aligned(ALIGN)))
#endif

#define ATTR1(TYPE) ATTR_(TYPE, 1)
#define ATTR2(TYPE) ATTR_(TYPE, 2)
#define ATTR4(TYPE) ATTR_(TYPE, 4)
#define ATTR8(TYPE) ATTR_(TYPE, 8)
#define ATTR16(TYPE) ATTR_(TYPE, 16)

ALIGN2(attr_1, ATTR1)
ALIGN2(attr_2, ATTR2)
ALIGN2(attr_4, ATTR4)
ALIGN2(attr_8, ATTR8)
ALIGN2(attr_16, ATTR16)

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

/* Now some targeting bitfield tests */

/* Bitfield alignment */
#define BITALIGN(TNUM,BNUM) \
    struct ba_##TNUM##_##BNUM { \
        char a; \
        uint##TNUM##_t b : BNUM; \
    }; \
    EXPORT int print_ba_##TNUM##_##BNUM(size_t* sz, size_t* align, char* buf, struct ba_##TNUM##_##BNUM* s); \
    int print_ba_##TNUM##_##BNUM(size_t* sz, size_t* align, char* buf, struct ba_##TNUM##_##BNUM* s) { \
        *sz = sizeof(struct ba_##TNUM##_##BNUM); \
        *align = alignof(struct ba_##TNUM##_##BNUM); \
        return sprintf(buf, "%d %d", (int) s->a, (int) s->b); \
    }

BITALIGN(8,7)
BITALIGN(16,7)
BITALIGN(16,15)
BITALIGN(32,7)
BITALIGN(32,15)
BITALIGN(32,31)
BITALIGN(64,7)
BITALIGN(64,15)
BITALIGN(64,31)
BITALIGN(64,63)

/* Do unsigned and signed coallesce */
#define BITCOALESCE(NUM) \
    struct bc##NUM { \
        uint##NUM##_t a : 3; \
        int##NUM##_t b : 3; \
    }; \
    EXPORT int print_bc##NUM(size_t* sz, size_t* align, char* buf, struct bc##NUM* s); \
    int print_bc##NUM(size_t* sz, size_t* align, char* buf, struct bc##NUM* s) { \
        *sz = sizeof(struct bc##NUM); \
        *align = alignof(struct bc##NUM); \
        return sprintf(buf, "%d %d", (int) s->a, (int) s->b); \
    }

BITCOALESCE(8)
BITCOALESCE(16)
BITCOALESCE(32)
BITCOALESCE(64)

// Do different sizes coallesce
struct bdsz {
    uint8_t a : 3;
    uint16_t b : 3;
    uint32_t c : 3;
    uint64_t d : 3;
};

EXPORT int print_bdsz(size_t* sz, size_t* align, char* buf, struct bdsz* s);
int print_bdsz(size_t* sz, size_t* align, char* buf, struct bdsz* s) {
    *sz = sizeof(struct bdsz);
    *align = alignof(struct bdsz);
    return sprintf(buf, "%d %d %d %d", (int) s->a, (int) s->b, (int) s->c, (int) s->d);
}

// Does coallesence upgrade the storage unit
struct bcup {
    uint8_t a : 7;
    uint16_t b : 9;
    uint32_t c : 17;
    uint64_t d : 33;
};

EXPORT int print_bcup(size_t* sz, size_t* align, char* buf, struct bcup* s);
int print_bcup(size_t* sz, size_t* align, char* buf, struct bcup* s) {
    *sz = sizeof(struct bcup);
    *align = alignof(struct bcup);
    return sprintf(buf, "%d %d %d %"PRIu64, (int) s->a, (int) s->b, (int) s->c, (uint64_t) s->d);
}

// Is unaligned access allowed
struct buna {
    uint32_t a : 31;
    uint32_t b : 31;
};

EXPORT int print_buna(size_t* sz, size_t* align, char* buf, struct buna* s);
int print_buna(size_t* sz, size_t* align, char* buf, struct buna* s) {
    *sz = sizeof(struct buna);
    *align = alignof(struct buna);
    return sprintf(buf, "%d %d", (int) s->a, (int) s->b);
}

/* What does a lone :0 do */
#define BITLONEZERO(NUM) \
    struct blz##NUM { \
        uint##NUM##_t a; \
        uint##NUM##_t :0; \
        uint##NUM##_t b; \
    }; \
    EXPORT int print_##blz##NUM(size_t* sz, size_t* align, char* buf, struct blz##NUM* s); \
    int print_blz##NUM(size_t* sz, size_t* align, char* buf, struct blz##NUM* s) { \
        *sz = sizeof(struct blz##NUM); \
        *align = alignof(struct blz##NUM); \
        return sprintf(buf, "%d %d", (int) s->a, (int) s->b); \
    }

BITLONEZERO(8)
BITLONEZERO(16)
BITLONEZERO(32)
BITLONEZERO(64)

/* What does a :0 or unnamed :# of the same or different type do */
#define BITZERO(NUM, ZNUM, BNUM) \
    struct bz_##NUM##_##ZNUM##_##BNUM { \
        uint8_t a; \
        uint##NUM##_t b : 3; \
        uint##ZNUM##_t :BNUM; \
        uint##NUM##_t c : 3; \
    }; \
    EXPORT int print_bz_##NUM##_##ZNUM##_##BNUM(size_t* sz, size_t* align, char* buf, struct bz_##NUM##_##ZNUM##_##BNUM* s); \
    int print_bz_##NUM##_##ZNUM##_##BNUM(size_t* sz, size_t* align, char* buf, struct bz_##NUM##_##ZNUM##_##BNUM* s) { \
        *sz = sizeof(struct bz_##NUM##_##ZNUM##_##BNUM); \
        *align = alignof(struct bz_##NUM##_##ZNUM##_##BNUM); \
        return sprintf(buf, "%d %d %d", (int) s->a, (int) s->b, (int) s->c); \
    }

BITZERO(8,8,0)
BITZERO(8,8,7)
BITZERO(8,16,0)
BITZERO(8,16,7)
BITZERO(8,16,15)
BITZERO(8,32,0)
BITZERO(8,32,7)
BITZERO(8,32,15)
BITZERO(8,32,31)
BITZERO(8,64,0)
BITZERO(8,64,7)
BITZERO(8,64,15)
BITZERO(8,64,31)
BITZERO(8,64,63)
BITZERO(16,8,0)
BITZERO(16,8,7)
BITZERO(16,16,0)
BITZERO(16,16,7)
BITZERO(16,16,15)
BITZERO(16,32,0)
BITZERO(16,32,7)
BITZERO(16,32,15)
BITZERO(16,32,31)
BITZERO(16,64,0)
BITZERO(16,64,7)
BITZERO(16,64,15)
BITZERO(16,64,31)
BITZERO(16,64,63)
BITZERO(32,8,0)
BITZERO(32,8,7)
BITZERO(32,16,0)
BITZERO(32,16,7)
BITZERO(32,16,15)
BITZERO(32,32,0)
BITZERO(32,32,7)
BITZERO(32,32,15)
BITZERO(32,32,31)
BITZERO(32,64,0)
BITZERO(32,64,7)
BITZERO(32,64,15)
BITZERO(32,64,31)
BITZERO(32,64,63)
BITZERO(64,8,0)
BITZERO(64,8,7)
BITZERO(64,16,0)
BITZERO(64,16,7)
BITZERO(64,16,15)
BITZERO(64,32,0)
BITZERO(64,32,7)
BITZERO(64,32,15)
BITZERO(64,32,31)
BITZERO(64,64,0)
BITZERO(64,64,7)
BITZERO(64,64,15)
BITZERO(64,64,31)
BITZERO(64,64,63)

#define CALL(TYPE, SUFFIX) \
    EXPORT TYPE call_##SUFFIX(TYPE (*func)(TYPE), TYPE arg); \
    TYPE call_##SUFFIX(TYPE (*func)(TYPE), TYPE arg) { \
        return func(arg); \
    }

CALL(int, i)
CALL(float, f)
CALL(double, d)
CALL(const char*, s)
CALL(_Bool, b)
#ifdef HAVE_COMPLEX
CALL(double complex, dc)
CALL(float complex, fc)
#endif

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


