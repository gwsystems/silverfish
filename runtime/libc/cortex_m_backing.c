#include "../runtime.h"

int printf_(const char* format, ...);

volatile int CORTEX_M_ARG_C = 0;

int cortexm_entry() {
    char* argv[] = { "", "", "" };

    //    printf_("Entering runtime to start wasm...\n");
    runtime_main(CORTEX_M_ARG_C, argv);
    //    printf_("Program finished.\n");
    return memory_size;
}


void* memcpy(void* destination, const void* source, size_t num) {
    int         i;
    char*       d = destination;
    const char* s = source;
    for (i = 0; i < num; i++) {
        d[i] = s[i];
    }
    return destination;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

size_t strlen(const char* str) {
    const char* s;
    for (s = str; *s; ++s) {}
    return (s - str);
}

char* strcpy(char* dest, const char* src) {
    unsigned i;
    for (i = 0; src[i] != '\0'; ++i)
        dest[i] = src[i];

    // Ensure trailing null byte is copied
    dest[i] = '\0';

    return dest;
}

struct wasm_iovec {
    i32 base_offset;
    i32 len;
};

#define SYS_MMAP 9

// We HACK here to improve memory usage

// This symbol let's us hackilly get our max capacity
extern u32 cortex_mem_size;

u32 wasm_mmap(i32 addr, i32 len, i32 prot, i32 flags, i32 fd, i32 offset) {
    if (addr != 0) {
        printf_("parameter void *addr is not supported!\n");
        awsm_assert(0);
    }

    if (fd != -1) {
        printf_("file mapping is not supported!\n");
        awsm_assert(0);
    }

    //    printf_("MMAP: expanding %d to %d\n", len, memory_size + len);

    return allocate_n_bytes(len);
}


#define SYS_WRITEV 20
i32 wasm_writev(i32 fd, i32 iov_offset, i32 iovcnt) {
    struct wasm_iovec* iov = get_memory_ptr_void(iov_offset, iovcnt * sizeof(struct wasm_iovec));

    if (fd == 1 || fd == 2) {
        int sum = 0;
        for (int i = 0; i < iovcnt; i++) {
            i32   len = iov[i].len;
            void* ptr = get_memory_ptr_void(iov[i].base_offset, len);

            printf_("%.*s", len, ptr);
            sum += len;
        }
        return sum;
    }
    awsm_assert(0);
}

#define SYS_MUNMAP 11

#define SYS_BRK 12

#define SYS_RT_SIGACTION 13

#define SYS_RT_SIGPROGMASK 14

#define SYS_IOCTL 16

#define SYS_SET_THREAD_AREA 205

#define SYS_SET_TID_ADDRESS 218

#define SYS_GET_TIME 228
struct wasm_time_spec {
    u32 sec;
    u32 nanosec;
};

i32 wasm_get_time(i32 clock_id, i32 timespec_off) {
    struct wasm_time_spec* timespec = get_memory_ptr_void(timespec_off, sizeof(struct wasm_time_spec));
    timespec->sec                   = 0;
    timespec->nanosec               = 0;

    return -1;
}


i32 inner_syscall_handler(i32 n, i32 a, i32 b, i32 c, i32 d, i32 e, i32 f) {
    if (n == SYS_MMAP) { return wasm_mmap(a, b, c, d, e, f); }

    if (n == SYS_WRITEV) { return wasm_writev(a, b, c); }

    if (n == SYS_MUNMAP || n == SYS_BRK || n == SYS_RT_SIGACTION || n == SYS_RT_SIGPROGMASK || n == SYS_IOCTL
        || n == SYS_SET_THREAD_AREA || n == SYS_SET_TID_ADDRESS) {
        return 0;
    }

    if (n == SYS_GET_TIME) { return wasm_get_time(a, b); }

    printf_("unknown syscall %d\n", n);
    awsm_assert(0);
}

i32 env_syscall_handler(i32 n, i32 a, i32 b, i32 c, i32 d, i32 e, i32 f) {
    switch_into_runtime();
    i32 i = inner_syscall_handler(n, a, b, c, d, e, f);
    switch_out_of_runtime();
    return i;
}

i32 env___syscall(i32 n, i32 a, i32 b, i32 c, i32 d, i32 e, i32 f) {
    return env_syscall_handler(n, a, b, c, d, e, f);
}

// Elf auxilary vector values (see google for what those are)
#define AT_NULL          0
#define AT_IGNORE        1
#define AT_EXECFD        2
#define AT_PHDR          3
#define AT_PHENT         4
#define AT_PHNUM         5
#define AT_PAGESZ        6
#define AT_BASE          7
#define AT_FLAGS         8
#define AT_ENTRY         9
#define AT_NOTELF        10
#define AT_UID           11
#define AT_EUID          12
#define AT_GID           13
#define AT_EGID          14
#define AT_CLKTCK        17
#define AT_SECURE        23
#define AT_BASE_PLATFORM 24
#define AT_RANDOM        25

#define UID 0xFF
#define GID 0xFE


// The symbol the binary gives us to init libc
void wasmf___init_libc(i32 envp, i32 pn);

char program_name[] = "wasm_program";

#define LC_PAGE_SIZE 128

// offset = a WASM ptr to memory the runtime can use
void stub_init(i32 offset) {
    // What program name will we put in the auxiliary vectors
    char program_name[] = "wasm_program";
    // Copy the program name into WASM accessible memory
    u32 program_name_offset = allocate_n_bytes(sizeof(program_name));
    strcpy(get_memory_ptr_for_runtime(program_name_offset, sizeof(program_name)), program_name);

    // The construction of this is:
    // evn1, env2, ..., NULL, auxv_n1, auxv_1, auxv_n2, auxv_2 ..., NULL
    i32 env_vec[] = {
        // Env variables would live here, but we don't supply any
        0,
        // We supply only the bare minimum AUX vectors
        AT_PAGESZ,
        WASM_PAGE_SIZE,
        AT_UID,
        UID,
        AT_EUID,
        UID,
        AT_GID,
        GID,
        AT_EGID,
        GID,
        AT_SECURE,
        0,
        AT_RANDOM,
        (i32)rand(), // It's pretty stupid to use rand here, but w/e
        0,
    };
    u32 env_vec_offset = allocate_n_bytes(sizeof(env_vec));
    memcpy(get_memory_ptr_for_runtime(env_vec_offset, sizeof(env_vec)), env_vec, sizeof(env_vec));

    switch_out_of_runtime();
    wasmf___init_libc(env_vec_offset, program_name_offset);
    switch_into_runtime();
}

void env_do_crash(i32 i) {
    printf_("env_do_crash triggered, spinning!!!\n");
    while (1)
        ;
}

void abort() {
    printf_("abort triggered, crashing!!!\n");
    env_do_crash(1);
    while (1)
        ;
}

// INLINE double env_sin(double x) {
//   int terms = 5;
//
//   double sign = 1.0;
//   double fact = 1.0;
//   double x_mul = x;
//
//   double sum = x;
//   for (int i = 1; i <= terms; i++) {
//       sign *= -1.0;
//       fact *= (2*i * (2*i + 1));
//       x_mul *= (x*x);
//
//       sum += ((sign * x_mul) / fact);
//   }
//
//   return sum;
//}
//
// INLINE double env_cos(double x) {
//    return env_sin(x + 1.57080);
//}

INLINE i32 env_a_ctz_32(u32 x) {
    static const char debruijn32[32] = { 0,  1,  23, 2,  29, 24, 19, 3,  30, 27, 25, 11, 20, 8, 4,  13,
                                         31, 22, 28, 18, 26, 10, 7,  12, 21, 17, 9,  6,  16, 5, 15, 14 };
    return debruijn32[(x & -x) * 0x076be629 >> 27];
}


i32 env_a_cas(i32 p_off, i32 old_val, i32 new_val) {
    awsm_assert(sizeof(i32) == sizeof(volatile int));
    i32* p = get_memory_ptr_void(p_off, sizeof(i32));

    i32 val = *p;
    if (val == old_val) *p = new_val;
    return val;
}

i32 env_a_swap(i32 x_off, i32 v) {
    awsm_assert(sizeof(i32) == sizeof(volatile int));
    volatile int* p = get_memory_ptr_void(x_off, sizeof(i32));

    int old;
    do
        old = *p;
    while (env_a_cas(x_off, old, v) != old);
    return old;
}

i32 env_a_fetch_add(i32 x_off, i32 v) {
    awsm_assert(sizeof(i32) == sizeof(volatile int));
    volatile int* p = get_memory_ptr_void(x_off, sizeof(i32));

    i32 old = *p;
    *p += v;

    return old;
}

int env_a_ctz_l(u32 x) {
    static const char debruijn32[32] = { 0,  1,  23, 2,  29, 24, 19, 3,  30, 27, 25, 11, 20, 8, 4,  13,
                                         31, 22, 28, 18, 26, 10, 7,  12, 21, 17, 9,  6,  16, 5, 15, 14 };
    return debruijn32[(x & -x) * 0x076be629 >> 27];
}

int env_a_ctz_64(u64 x) {
    u32 y = x;
    if (!y) {
        y = x >> 32;
        return 32 + env_a_ctz_l(y);
    }
    return env_a_ctz_l(y);
}

void env_a_store(i32 p_off, i32 x) {
    //    assert(sizeof(i32) == sizeof(volatile int));
    volatile int* p = get_memory_ptr_void(p_off, sizeof(i32));
    *p              = x;
}

void env_a_dec(i32 x_off) {
    volatile int* x = get_memory_ptr_void(x_off, sizeof(i32));
    *x--;
}

void env_do_spin(i32 i) {
    printf_("Spinning! %d\n", i);
    while (1)
        ;
}

void env_a_inc(i32 x_off) {
    volatile int* x = get_memory_ptr_void(x_off, sizeof(i32));
    *x++;
}

INLINE void env_a_and_64(i32 p_off, u64 v) {
    u64* p = get_memory_ptr_void(p_off, sizeof(u64));
    *p &= v;
}

INLINE void env_a_or_64(i32 p_off, u64 v) {
    u64* p = get_memory_ptr_void(p_off, sizeof(u64));
    *p |= v;
}


// HACK of HACKs
int __aeabi_unwind_cpp_pr0() {
    printf_("UNWIND 0 triggered! spinning...");
    while (1)
        ;
}

int __aeabi_unwind_cpp_pr1() {
    printf_("UNWIND 1 triggered! spinning...");
    while (1)
        ;
}


// MUSL magic:

/* Get two 32 bit ints from a double.  */
#define EXTRACT_WORDS(hi, lo, d) \
    do {                         \
        union {                  \
            double   f;          \
            uint64_t i;          \
        } __u;                   \
        __u.f = (d);             \
        (hi)  = __u.i >> 32;     \
        (lo)  = (uint32_t)__u.i; \
    } while (0)

/* Set a double from two 32 bit ints.  */
#define INSERT_WORDS(d, hi, lo)                          \
    do {                                                 \
        union {                                          \
            double   f;                                  \
            uint64_t i;                                  \
        } __u;                                           \
        __u.i = ((uint64_t)(hi) << 32) | (uint32_t)(lo); \
        (d)   = __u.f;                                   \
    } while (0)

/* Get the more significant 32 bit int from a double.  */
#define GET_HIGH_WORD(hi, d) \
    do {                     \
        union {              \
            double   f;      \
            uint64_t i;      \
        } __u;               \
        __u.f = (d);         \
        (hi)  = __u.i >> 32; \
    } while (0)

#define FORCE_EVAL(x)                             \
    do {                                          \
        if (sizeof(x) == sizeof(float)) {         \
            volatile float __x;                   \
            __x = (x);                            \
        } else if (sizeof(x) == sizeof(double)) { \
            volatile double __x;                  \
            __x = (x);                            \
        } else {                                  \
            volatile long double __x;             \
            __x = (x);                            \
        }                                         \
    } while (0)

#define EPS (0x1p-52)

double trunc(double x) {
    union {
        double f;
        u64    i;
    } u   = { x };
    int e = (int)(u.i >> 52 & 0x7ff) - 0x3ff + 12;
    u64 m;

    if (e >= 52 + 12) return x;
    if (e < 12) e = 1;
    m = -1ULL >> e;
    if ((u.i & m) == 0) return x;
    FORCE_EVAL(x + 0x1p120f);
    u.i &= ~m;
    return u.f;
}

float truncf(float x) {
    union {
        float f;
        u32   i;
    } u   = { x };
    int e = (int)(u.i >> 23 & 0xff) - 0x7f + 9;
    u32 m;

    if (e >= 23 + 9) return x;
    if (e < 9) e = 1;
    m = -1U >> e;
    if ((u.i & m) == 0) return x;
    FORCE_EVAL(x + 0x1p120f);
    u.i &= ~m;
    return u.f;
}


static const double toint = 1 / EPS;

double floor(double x) {
    union {
        double   f;
        uint64_t i;
    } u      = { x };
    int    e = u.i >> 52 & 0x7ff;
    double y;

    if (e >= 0x3ff + 52 || x == 0) return x;
    /* y = int(x) - x, where int(x) is an integer neighbor of x */
    if (u.i >> 63)
        y = x - toint + toint - x;
    else
        y = x + toint - toint - x;
    /* special case because of non-nearest rounding modes */
    if (e <= 0x3ff - 1) {
        FORCE_EVAL(y);
        return u.i >> 63 ? -1 : 0;
    }
    if (y > 0) return x + y - 1;
    return x + y;
}


double scalbn(double x, int n) {
    union {
        double   f;
        uint64_t i;
    } u;
    double y = x;

    if (n > 1023) {
        y *= 0x1p1023;
        n -= 1023;
        if (n > 1023) {
            y *= 0x1p1023;
            n -= 1023;
            if (n > 1023) n = 1023;
        }
    } else if (n < -1022) {
        /* make sure final n < -53 to avoid double
           rounding in the subnormal range */
        y *= 0x1p-1022 * 0x1p53;
        n += 1022 - 53;
        if (n < -1022) {
            y *= 0x1p-1022 * 0x1p53;
            n += 1022 - 53;
            if (n < -1022) n = -1022;
        }
    }
    u.i = (uint64_t)(0x3ff + n) << 52;
    x   = y * u.f;
    return x;
}

static const double tiny = 1.0e-300;

double sqrt(double x) {
    double   z;
    int32_t  sign = (int)0x80000000;
    int32_t  ix0, s0, q, m, t, i;
    uint32_t r, t1, s1, ix1, q1;

    EXTRACT_WORDS(ix0, ix1, x);

    /* take care of Inf and NaN */
    if ((ix0 & 0x7ff00000) == 0x7ff00000) { return x * x + x; /* sqrt(NaN)=NaN, sqrt(+inf)=+inf, sqrt(-inf)=sNaN */ }
    /* take care of zero */
    if (ix0 <= 0) {
        if (((ix0 & ~sign) | ix1) == 0) return x; /* sqrt(+-0) = +-0 */
        if (ix0 < 0) return (x - x) / (x - x);    /* sqrt(-ve) = sNaN */
    }
    /* normalize x */
    m = ix0 >> 20;
    if (m == 0) { /* subnormal x */
        while (ix0 == 0) {
            m -= 21;
            ix0 |= (ix1 >> 11);
            ix1 <<= 21;
        }
        for (i = 0; (ix0 & 0x00100000) == 0; i++)
            ix0 <<= 1;
        m -= i - 1;
        ix0 |= ix1 >> (32 - i);
        ix1 <<= i;
    }
    m -= 1023; /* unbias exponent */
    ix0 = (ix0 & 0x000fffff) | 0x00100000;
    if (m & 1) { /* odd m, double x to make it even */
        ix0 += ix0 + ((ix1 & sign) >> 31);
        ix1 += ix1;
    }
    m >>= 1; /* m = [m/2] */

    /* generate sqrt(x) bit by bit */
    ix0 += ix0 + ((ix1 & sign) >> 31);
    ix1 += ix1;
    q = q1 = s0 = s1 = 0;          /* [q,q1] = sqrt(x) */
    r                = 0x00200000; /* r = moving bit from right to left */

    while (r != 0) {
        t = s0 + r;
        if (t <= ix0) {
            s0 = t + r;
            ix0 -= t;
            q += r;
        }
        ix0 += ix0 + ((ix1 & sign) >> 31);
        ix1 += ix1;
        r >>= 1;
    }

    r = sign;
    while (r != 0) {
        t1 = s1 + r;
        t  = s0;
        if (t < ix0 || (t == ix0 && t1 <= ix1)) {
            s1 = t1 + r;
            if ((t1 & sign) == sign && (s1 & sign) == 0) s0++;
            ix0 -= t;
            if (ix1 < t1) ix0--;
            ix1 -= t1;
            q1 += r;
        }
        ix0 += ix0 + ((ix1 & sign) >> 31);
        ix1 += ix1;
        r >>= 1;
    }

    /* use floating add to find out rounding direction */
    if ((ix0 | ix1) != 0) {
        z = 1.0 - tiny; /* raise inexact flag */
        if (z >= 1.0) {
            z = 1.0 + tiny;
            if (q1 == (uint32_t)0xffffffff) {
                q1 = 0;
                q++;
            } else if (z > 1.0) {
                if (q1 == (uint32_t)0xfffffffe) q++;
                q1 += 2;
            } else
                q1 += q1 & 1;
        }
    }
    ix0 = (q >> 1) + 0x3fe00000;
    ix1 = q1 >> 1;
    if (q & 1) ix1 |= sign;
    INSERT_WORDS(z, ix0 + ((uint32_t)m << 20), ix1);
    return z;
}

static const int init_jk[] = { 3, 4, 4, 6 }; /* initial value for jk */

/*
 * Table of constants for 2/pi, 396 Hex digits (476 decimal) of 2/pi
 *
 *              integer array, contains the (24*i)-th to (24*i+23)-th
 *              bit of 2/pi after binary point. The corresponding
 *              floating value is
 *
 *                      ipio2[i] * 2^(-24(i+1)).
 *
 * NB: This table must have at least (e0-3)/24 + jk terms.
 *     For quad precision (e0 <= 16360, jk = 6), this is 686.
 */
static const int32_t ipio2[] = {
    0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62, 0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7,
    0x246E3A, 0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129, 0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C,
    0x7026B4, 0x5F7E41, 0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8, 0x97FFDE, 0x05980F, 0xEF2F11,
    0x8B5A0A, 0x6D1F6D, 0x367ECF, 0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5, 0xF17B3D, 0x0739F7,
    0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08, 0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3, 0x91615E,
    0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880, 0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,

#if LDBL_MAX_EXP > 1024
    0x47C419, 0xC367CD, 0xDCE809, 0x2A8359, 0xC4768B, 0x961CA6, 0xDDAF44, 0xD15719, 0x053EA5, 0xFF0705, 0x3F7E33,
    0xE832C2, 0xDE4F98, 0x327DBB, 0xC33D26, 0xEF6B1E, 0x5EF89F, 0x3A1F35, 0xCAF27F, 0x1D87F1, 0x21907C, 0x7C246A,
    0xFA6ED5, 0x772D30, 0x433B15, 0xC614B5, 0x9D19C3, 0xC2C4AD, 0x414D2C, 0x5D000C, 0x467D86, 0x2D71E3, 0x9AC69B,
    0x006233, 0x7CD2B4, 0x97A7B4, 0xD55537, 0xF63ED7, 0x1810A3, 0xFC764D, 0x2A9D64, 0xABD770, 0xF87C63, 0x57B07A,
    0xE71517, 0x5649C0, 0xD9D63B, 0x3884A7, 0xCB2324, 0x778AD6, 0x23545A, 0xB91F00, 0x1B0AF1, 0xDFCE19, 0xFF319F,
    0x6A1E66, 0x615799, 0x47FBAC, 0xD87F7E, 0xB76522, 0x89E832, 0x60BFE6, 0xCDC4EF, 0x09366C, 0xD43F5D, 0xD7DE16,
    0xDE3B58, 0x929BDE, 0x2822D2, 0xE88628, 0x4D58E2, 0x32CAC6, 0x16E308, 0xCB7DE0, 0x50C017, 0xA71DF3, 0x5BE018,
    0x34132E, 0x621283, 0x014883, 0x5B8EF5, 0x7FB0AD, 0xF2E91E, 0x434A48, 0xD36710, 0xD8DDAA, 0x425FAE, 0xCE616A,
    0xA4280A, 0xB499D3, 0xF2A606, 0x7F775C, 0x83C2A3, 0x883C61, 0x78738A, 0x5A8CAF, 0xBDD76F, 0x63A62D, 0xCBBFF4,
    0xEF818D, 0x67C126, 0x45CA55, 0x36D9CA, 0xD2A828, 0x8D61C2, 0x77C912, 0x142604, 0x9B4612, 0xC459C4, 0x44C5C8,
    0x91B24D, 0xF31700, 0xAD43D4, 0xE54929, 0x10D5FD, 0xFCBE00, 0xCC941E, 0xEECE70, 0xF53E13, 0x80F1EC, 0xC3E7B3,
    0x28F8C7, 0x940593, 0x3E71C1, 0xB3092E, 0xF3450B, 0x9C1288, 0x7B20AB, 0x9FB52E, 0xC29247, 0x2F327B, 0x6D550C,
    0x90A772, 0x1FE76B, 0x96CB31, 0x4A1679, 0xE27941, 0x89DFF4, 0x9794E8, 0x84E6E2, 0x973199, 0x6BED88, 0x365F5F,
    0x0EFDBB, 0xB49A48, 0x6CA467, 0x427271, 0x325D8D, 0xB8159F, 0x09E5BC, 0x25318D, 0x3974F7, 0x1C0530, 0x010C0D,
    0x68084B, 0x58EE2C, 0x90AA47, 0x02E774, 0x24D6BD, 0xA67DF7, 0x72486E, 0xEF169F, 0xA6948E, 0xF691B4, 0x5153D1,
    0xF20ACF, 0x339820, 0x7E4BF5, 0x6863B2, 0x5F3EDD, 0x035D40, 0x7F8985, 0x295255, 0xC06437, 0x10D86D, 0x324832,
    0x754C5B, 0xD4714E, 0x6E5445, 0xC1090B, 0x69F52A, 0xD56614, 0x9D0727, 0x50045D, 0xDB3BB4, 0xC576EA, 0x17F987,
    0x7D6B49, 0xBA271D, 0x296996, 0xACCCC6, 0x5414AD, 0x6AE290, 0x89D988, 0x50722C, 0xBEA404, 0x940777, 0x7030F3,
    0x27FC00, 0xA871EA, 0x49C266, 0x3DE064, 0x83DD97, 0x973FA3, 0xFD9443, 0x8C860D, 0xDE4131, 0x9D3992, 0x8C70DD,
    0xE7B717, 0x3BDF08, 0x2B3715, 0xA0805C, 0x93805A, 0x921110, 0xD8E80F, 0xAF806C, 0x4BFFDB, 0x0F9038, 0x761859,
    0x15A562, 0xBBCB61, 0xB989C7, 0xBD4010, 0x04F2D2, 0x277549, 0xF6B6EB, 0xBB22DB, 0xAA140A, 0x2F2689, 0x768364,
    0x333B09, 0x1A940E, 0xAA3A51, 0xC2A31D, 0xAEEDAF, 0x12265C, 0x4DC26D, 0x9C7A2D, 0x9756C0, 0x833F03, 0xF6F009,
    0x8C402B, 0x99316D, 0x07B439, 0x15200C, 0x5BC3D8, 0xC492F5, 0x4BADC6, 0xA5CA4E, 0xCD37A7, 0x36A9E6, 0x9492AB,
    0x6842DD, 0xDE6319, 0xEF8C76, 0x528B68, 0x37DBFC, 0xABA1AE, 0x3115DF, 0xA1AE00, 0xDAFB0C, 0x664D64, 0xB705ED,
    0x306529, 0xBF5657, 0x3AFF47, 0xB9F96A, 0xF3BE75, 0xDF9328, 0x3080AB, 0xF68C66, 0x15CB04, 0x0622FA, 0x1DE4D9,
    0xA4B33D, 0x8F1B57, 0x09CD36, 0xE9424E, 0xA4BE13, 0xB52333, 0x1AAAF0, 0xA8654F, 0xA5C1D2, 0x0F3F0B, 0xCD785B,
    0x76F923, 0x048B7B, 0x721789, 0x53A6C6, 0xE26E6F, 0x00EBEF, 0x584A9B, 0xB7DAC4, 0xBA66AA, 0xCFCF76, 0x1D02D1,
    0x2DF1B1, 0xC1998C, 0x77ADC3, 0xDA4886, 0xA05DF7, 0xF480C6, 0x2FF0AC, 0x9AECDD, 0xBC5C3F, 0x6DDED0, 0x1FC790,
    0xB6DB2A, 0x3A25A3, 0x9AAF00, 0x9353AD, 0x0457B6, 0xB42D29, 0x7E804B, 0xA707DA, 0x0EAA76, 0xA1597B, 0x2A1216,
    0x2DB7DC, 0xFDE5FA, 0xFEDB89, 0xFDBE89, 0x6C76E4, 0xFCA906, 0x70803E, 0x156E85, 0xFF87FD, 0x073E28, 0x336761,
    0x86182A, 0xEABD4D, 0xAFE7B3, 0x6E6D8F, 0x396795, 0x5BBF31, 0x48D784, 0x16DF30, 0x432DC7, 0x356125, 0xCE70C9,
    0xB8CB30, 0xFD6CBF, 0xA200A4, 0xE46C05, 0xA0DD5A, 0x476F21, 0xD21262, 0x845CB9, 0x496170, 0xE0566B, 0x015299,
    0x375550, 0xB7D51E, 0xC4F133, 0x5F6E13, 0xE4305D, 0xA92E85, 0xC3B21D, 0x3632A1, 0xA4B708, 0xD4B1EA, 0x21F716,
    0xE4698F, 0x77FF27, 0x80030C, 0x2D408D, 0xA0CD4F, 0x99A520, 0xD3A2B3, 0x0A5D2F, 0x42F9B4, 0xCBDA11, 0xD0BE7D,
    0xC1DB9B, 0xBD17AB, 0x81A2CA, 0x5C6A08, 0x17552E, 0x550027, 0xF0147F, 0x8607E1, 0x640B14, 0x8D4196, 0xDEBE87,
    0x2AFDDA, 0xB6256B, 0x34897B, 0xFEF305, 0x9EBFB9, 0x4F6A68, 0xA82A4A, 0x5AC44F, 0xBCF82D, 0x985AD7, 0x95C7F4,
    0x8D4D0D, 0xA63A20, 0x5F57A4, 0xB13F14, 0x953880, 0x0120CC, 0x86DD71, 0xB6DEC9, 0xF560BF, 0x11654D, 0x6B0701,
    0xACB08C, 0xD0C0B2, 0x485551, 0x0EFB1E, 0xC37295, 0x3B06A3, 0x3540C0, 0x7BDC06, 0xCC45E0, 0xFA294E, 0xC8CAD6,
    0x41F3E8, 0xDE647C, 0xD8649B, 0x31BED9, 0xC397A4, 0xD45877, 0xC5E369, 0x13DAF0, 0x3C3ABA, 0x461846, 0x5F7555,
    0xF5BDD2, 0xC6926E, 0x5D2EAC, 0xED440E, 0x423E1C, 0x87C461, 0xE9FD29, 0xF3D6E7, 0xCA7C22, 0x35916F, 0xC5E008,
    0x8DD7FF, 0xE26A6E, 0xC6FDB0, 0xC10893, 0x745D7C, 0xB2AD6B, 0x9D6ECD, 0x7B723E, 0x6A11C6, 0xA9CFF7, 0xDF7329,
    0xBAC9B5, 0x5100B7, 0x0DB2E2, 0x24BA74, 0x607DE5, 0x8AD874, 0x2C150D, 0x0C1881, 0x94667E, 0x162901, 0x767A9F,
    0xBEFDFD, 0xEF4556, 0x367ED9, 0x13D9EC, 0xB9BA8B, 0xFC97C4, 0x27A831, 0xC36EF1, 0x36C594, 0x56A8D8, 0xB5A8B4,
    0x0ECCCF, 0x2D8912, 0x34576F, 0x89562C, 0xE3CE99, 0xB920D6, 0xAA5E6B, 0x9C2A3E, 0xCC5F11, 0x4A0BFD, 0xFBF4E1,
    0x6D3B8E, 0x2C86E2, 0x84D4E9, 0xA9B4FC, 0xD1EEEF, 0xC9352E, 0x61392F, 0x442138, 0xC8D91B, 0x0AFC81, 0x6A4AFB,
    0xD81C2F, 0x84B453, 0x8C994E, 0xCC2254, 0xDC552A, 0xD6C6C0, 0x96190B, 0xB8701A, 0x649569, 0x605A26, 0xEE523F,
    0x0F117F, 0x11B5F4, 0xF5CBFC, 0x2DBC34, 0xEEBC34, 0xCC5DE8, 0x605EDD, 0x9B8E67, 0xEF3392, 0xB817C9, 0x9B5861,
    0xBC57E1, 0xC68351, 0x103ED8, 0x4871DD, 0xDD1C2D, 0xA118AF, 0x462C21, 0xD7F359, 0x987AD9, 0xC0549E, 0xFA864F,
    0xFC0656, 0xAE79E5, 0x362289, 0x22AD38, 0xDC9367, 0xAAE855, 0x382682, 0x9BE7CA, 0xA40D51, 0xB13399, 0x0ED7A9,
    0x480569, 0xF0B265, 0xA7887F, 0x974C88, 0x36D1F9, 0xB39221, 0x4A827B, 0x21CF98, 0xDC9F40, 0x5547DC, 0x3A74E1,
    0x42EB67, 0xDF9DFE, 0x5FD45E, 0xA4677B, 0x7AACBA, 0xA2F655, 0x23882B, 0x55BA41, 0x086E59, 0x862A21, 0x834739,
    0xE6E389, 0xD49EE5, 0x40FB49, 0xE956FF, 0xCA0F1C, 0x8A59C5, 0x2BFA94, 0xC5C1D3, 0xCFC50F, 0xAE5ADB, 0x86C547,
    0x624385, 0x3B8621, 0x94792C, 0x876110, 0x7B4C2A, 0x1A2C80, 0x12BF43, 0x902688, 0x893C78, 0xE4C4A8, 0x7BDBE5,
    0xC23AC4, 0xEAF426, 0x8A67F7, 0xBF920D, 0x2BA365, 0xB1933D, 0x0B7CBD, 0xDC51A4, 0x63DD27, 0xDDE169, 0x19949A,
    0x9529A8, 0x28CE68, 0xB4ED09, 0x209F44, 0xCA984E, 0x638270, 0x237C7E, 0x32B90F, 0x8EF5A7, 0xE75614, 0x08F121,
    0x2A9DB5, 0x4D7E6F, 0x5119A5, 0xABF9B5, 0xD6DF82, 0x61DD96, 0x023616, 0x9F3AC4, 0xA1A283, 0x6DED72, 0x7A8D39,
    0xA9B882, 0x5C326B, 0x5B2746, 0xED3400, 0x7700D2, 0x55F4FC, 0x4D5901, 0x8071E0,
#endif
};

static const double PIo2[] = {
    1.57079625129699707031e+00, /* 0x3FF921FB, 0x40000000 */
    7.54978941586159635335e-08, /* 0x3E74442D, 0x00000000 */
    5.39030252995776476554e-15, /* 0x3CF84698, 0x80000000 */
    3.28200341580791294123e-22, /* 0x3B78CC51, 0x60000000 */
    1.27065575308067607349e-29, /* 0x39F01B83, 0x80000000 */
    1.22933308981111328932e-36, /* 0x387A2520, 0x40000000 */
    2.73370053816464559624e-44, /* 0x36E38222, 0x80000000 */
    2.16741683877804819444e-51, /* 0x3569F31D, 0x00000000 */
};

int __rem_pio2_large(double* x, double* y, int e0, int nx, int prec) {
    int32_t jz, jx, jv, jp, jk, carry, n, iq[20], i, j, k, m, q0, ih;
    double  z, fw, f[20], fq[20], q[20];

    /* initialize jk*/
    jk = init_jk[prec];
    jp = jk;

    /* determine jx,jv,q0, note that 3>q0 */
    jx = nx - 1;
    jv = (e0 - 3) / 24;
    if (jv < 0) jv = 0;
    q0 = e0 - 24 * (jv + 1);

    /* set up f[0] to f[jx+jk] where f[jx+jk] = ipio2[jv+jk] */
    j = jv - jx;
    m = jx + jk;
    for (i = 0; i <= m; i++, j++)
        f[i] = j < 0 ? 0.0 : (double)ipio2[j];

    /* compute q[0],q[1],...q[jk] */
    for (i = 0; i <= jk; i++) {
        for (j = 0, fw = 0.0; j <= jx; j++)
            fw += x[j] * f[jx + i - j];
        q[i] = fw;
    }

    jz = jk;
recompute:
    /* distill q[] into iq[] reversingly */
    for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
        fw    = (double)(int32_t)(0x1p-24 * z);
        iq[i] = (int32_t)(z - 0x1p24 * fw);
        z     = q[j - 1] + fw;
    }

    /* compute n */
    z = scalbn(z, q0);           /* actual value of z */
    z -= 8.0 * floor(z * 0.125); /* trim off integer >= 8 */
    n = (int32_t)z;
    z -= (double)n;
    ih = 0;
    if (q0 > 0) { /* need iq[jz-1] to determine n */
        i = iq[jz - 1] >> (24 - q0);
        n += i;
        iq[jz - 1] -= i << (24 - q0);
        ih = iq[jz - 1] >> (23 - q0);
    } else if (q0 == 0)
        ih = iq[jz - 1] >> 23;
    else if (z >= 0.5)
        ih = 2;

    if (ih > 0) { /* q > 0.5 */
        n += 1;
        carry = 0;
        for (i = 0; i < jz; i++) { /* compute 1-q */
            j = iq[i];
            if (carry == 0) {
                if (j != 0) {
                    carry = 1;
                    iq[i] = 0x1000000 - j;
                }
            } else
                iq[i] = 0xffffff - j;
        }
        if (q0 > 0) { /* rare case: chance is 1 in 12 */
            switch (q0) {
                case 1: iq[jz - 1] &= 0x7fffff; break;
                case 2: iq[jz - 1] &= 0x3fffff; break;
            }
        }
        if (ih == 2) {
            z = 1.0 - z;
            if (carry != 0) z -= scalbn(1.0, q0);
        }
    }

    /* check if recomputation is needed */
    if (z == 0.0) {
        j = 0;
        for (i = jz - 1; i >= jk; i--)
            j |= iq[i];
        if (j == 0) { /* need recomputation */
            for (k = 1; iq[jk - k] == 0; k++)
                ; /* k = no. of terms needed */

            for (i = jz + 1; i <= jz + k; i++) { /* add q[jz+1] to q[jz+k] */
                f[jx + i] = (double)ipio2[jv + i];
                for (j = 0, fw = 0.0; j <= jx; j++)
                    fw += x[j] * f[jx + i - j];
                q[i] = fw;
            }
            jz += k;
            goto recompute;
        }
    }

    /* chop off zero terms */
    if (z == 0.0) {
        jz -= 1;
        q0 -= 24;
        while (iq[jz] == 0) {
            jz--;
            q0 -= 24;
        }
    } else { /* break z into 24-bit if necessary */
        z = scalbn(z, -q0);
        if (z >= 0x1p24) {
            fw     = (double)(int32_t)(0x1p-24 * z);
            iq[jz] = (int32_t)(z - 0x1p24 * fw);
            jz += 1;
            q0 += 24;
            iq[jz] = (int32_t)fw;
        } else
            iq[jz] = (int32_t)z;
    }

    /* convert integer "bit" chunk to floating-point value */
    fw = scalbn(1.0, q0);
    for (i = jz; i >= 0; i--) {
        q[i] = fw * (double)iq[i];
        fw *= 0x1p-24;
    }

    /* compute PIo2[0,...,jp]*q[jz,...,0] */
    for (i = jz; i >= 0; i--) {
        for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++)
            fw += PIo2[k] * q[i + k];
        fq[jz - i] = fw;
    }

    /* compress fq[] into y[] */
    switch (prec) {
        case 0:
            fw = 0.0;
            for (i = jz; i >= 0; i--)
                fw += fq[i];
            y[0] = ih == 0 ? fw : -fw;
            break;
        case 1:
        case 2:
            fw = 0.0;
            for (i = jz; i >= 0; i--)
                fw += fq[i];
            // TODO: drop excess precision here once double_t is used
            fw   = (double)fw;
            y[0] = ih == 0 ? fw : -fw;
            fw   = fq[0] - fw;
            for (i = 1; i <= jz; i++)
                fw += fq[i];
            y[1] = ih == 0 ? fw : -fw;
            break;
        case 3: /* painful */
            for (i = jz; i > 0; i--) {
                fw = fq[i - 1] + fq[i];
                fq[i] += fq[i - 1] - fw;
                fq[i - 1] = fw;
            }
            for (i = jz; i > 1; i--) {
                fw = fq[i - 1] + fq[i];
                fq[i] += fq[i - 1] - fw;
                fq[i - 1] = fw;
            }
            for (fw = 0.0, i = jz; i >= 2; i--)
                fw += fq[i];
            if (ih == 0) {
                y[0] = fq[0];
                y[1] = fq[1];
                y[2] = fw;
            } else {
                y[0] = -fq[0];
                y[1] = -fq[1];
                y[2] = -fw;
            }
    }
    return n & 7;
}


/*
 * invpio2:  53 bits of 2/pi
 * pio2_1:   first  33 bit of pi/2
 * pio2_1t:  pi/2 - pio2_1
 * pio2_2:   second 33 bit of pi/2
 * pio2_2t:  pi/2 - (pio2_1+pio2_2)
 * pio2_3:   third  33 bit of pi/2
 * pio2_3t:  pi/2 - (pio2_1+pio2_2+pio2_3)
 */
static const double invpio2 = 6.36619772367581382433e-01, /* 0x3FE45F30, 0x6DC9C883 */
  pio2_1                    = 1.57079632673412561417e+00, /* 0x3FF921FB, 0x54400000 */
  pio2_1t                   = 6.07710050650619224932e-11, /* 0x3DD0B461, 0x1A626331 */
  pio2_2                    = 6.07710050630396597660e-11, /* 0x3DD0B461, 0x1A600000 */
  pio2_2t                   = 2.02226624879595063154e-21, /* 0x3BA3198A, 0x2E037073 */
  pio2_3                    = 2.02226624871116645580e-21, /* 0x3BA3198A, 0x2E000000 */
  pio2_3t                   = 8.47842766036889956997e-32; /* 0x397B839A, 0x252049C1 */

int __rem_pio2(double x, double* y) {
    union {
        double   f;
        uint64_t i;
    } u = { x };
    double   z, w, t, r, fn;
    double   tx[3], ty[2];
    uint32_t ix;
    int      sign, n, ex, ey, i;

    sign = u.i >> 63;
    ix   = u.i >> 32 & 0x7fffffff;
    if (ix <= 0x400f6a7a) {            /* |x| ~<= 5pi/4 */
        if ((ix & 0xfffff) == 0x921fb) /* |x| ~= pi/2 or 2pi/2 */
            goto medium;               /* cancellation -- use medium case */
        if (ix <= 0x4002d97c) {        /* |x| ~<= 3pi/4 */
            if (!sign) {
                z    = x - pio2_1; /* one round good to 85 bits */
                y[0] = z - pio2_1t;
                y[1] = (z - y[0]) - pio2_1t;
                return 1;
            } else {
                z    = x + pio2_1;
                y[0] = z + pio2_1t;
                y[1] = (z - y[0]) + pio2_1t;
                return -1;
            }
        } else {
            if (!sign) {
                z    = x - 2 * pio2_1;
                y[0] = z - 2 * pio2_1t;
                y[1] = (z - y[0]) - 2 * pio2_1t;
                return 2;
            } else {
                z    = x + 2 * pio2_1;
                y[0] = z + 2 * pio2_1t;
                y[1] = (z - y[0]) + 2 * pio2_1t;
                return -2;
            }
        }
    }
    if (ix <= 0x401c463b) {       /* |x| ~<= 9pi/4 */
        if (ix <= 0x4015fdbc) {   /* |x| ~<= 7pi/4 */
            if (ix == 0x4012d97c) /* |x| ~= 3pi/2 */
                goto medium;
            if (!sign) {
                z    = x - 3 * pio2_1;
                y[0] = z - 3 * pio2_1t;
                y[1] = (z - y[0]) - 3 * pio2_1t;
                return 3;
            } else {
                z    = x + 3 * pio2_1;
                y[0] = z + 3 * pio2_1t;
                y[1] = (z - y[0]) + 3 * pio2_1t;
                return -3;
            }
        } else {
            if (ix == 0x401921fb) /* |x| ~= 4pi/2 */
                goto medium;
            if (!sign) {
                z    = x - 4 * pio2_1;
                y[0] = z - 4 * pio2_1t;
                y[1] = (z - y[0]) - 4 * pio2_1t;
                return 4;
            } else {
                z    = x + 4 * pio2_1;
                y[0] = z + 4 * pio2_1t;
                y[1] = (z - y[0]) + 4 * pio2_1t;
                return -4;
            }
        }
    }
    if (ix < 0x413921fb) { /* |x| ~< 2^20*(pi/2), medium size */
    medium:
        /* rint(x/(pi/2)), Assume round-to-nearest. */
        fn   = (double)x * invpio2 + toint - toint;
        n    = (int32_t)fn;
        r    = x - fn * pio2_1;
        w    = fn * pio2_1t; /* 1st round, good to 85 bits */
        y[0] = r - w;
        u.f  = y[0];
        ey   = u.i >> 52 & 0x7ff;
        ex   = ix >> 20;
        if (ex - ey > 16) { /* 2nd round, good to 118 bits */
            t    = r;
            w    = fn * pio2_2;
            r    = t - w;
            w    = fn * pio2_2t - ((t - r) - w);
            y[0] = r - w;
            u.f  = y[0];
            ey   = u.i >> 52 & 0x7ff;
            if (ex - ey > 49) { /* 3rd round, good to 151 bits, covers all cases */
                t    = r;
                w    = fn * pio2_3;
                r    = t - w;
                w    = fn * pio2_3t - ((t - r) - w);
                y[0] = r - w;
            }
        }
        y[1] = (r - y[0]) - w;
        return n;
    }
    /*
     * all other (large) arguments
     */
    if (ix >= 0x7ff00000) { /* x is inf or NaN */
        y[0] = y[1] = x - x;
        return 0;
    }
    /* set z = scalbn(|x|,-ilogb(x)+23) */
    u.f = x;
    u.i &= (uint64_t)-1 >> 12;
    u.i |= (uint64_t)(0x3ff + 23) << 52;
    z = u.f;
    for (i = 0; i < 2; i++) {
        tx[i] = (double)(int32_t)z;
        z     = (z - tx[i]) * 0x1p24;
    }
    tx[i] = z;
    /* skip zero terms, first term is non-zero */
    while (tx[i] == 0.0)
        i--;
    n = __rem_pio2_large(tx, ty, (int)(ix >> 20) - (0x3ff + 23), i + 1, 1);
    if (sign) {
        y[0] = -ty[0];
        y[1] = -ty[1];
        return -n;
    }
    y[0] = ty[0];
    y[1] = ty[1];
    return n;
}


static const double S1 = -1.66666666666666324348e-01, /* 0xBFC55555, 0x55555549 */
  S2                   = 8.33333333332248946124e-03,  /* 0x3F811111, 0x1110F8A6 */
  S3                   = -1.98412698298579493134e-04, /* 0xBF2A01A0, 0x19C161D5 */
  S4                   = 2.75573137070700676789e-06,  /* 0x3EC71DE3, 0x57B1FE7D */
  S5                   = -2.50507602534068634195e-08, /* 0xBE5AE5E6, 0x8A2B9CEB */
  S6                   = 1.58969099521155010221e-10;  /* 0x3DE5D93A, 0x5ACFD57C */

double __sin(double x, double y, int iy) {
    double z, r, v, w;

    z = x * x;
    w = z * z;
    r = S2 + z * (S3 + z * S4) + z * w * (S5 + z * S6);
    v = z * x;
    if (iy == 0)
        return x + v * (S1 + z * r);
    else
        return x - ((z * (0.5 * y - v * r) - y) - v * S1);
}

static const double C1 = 4.16666666666666019037e-02,  /* 0x3FA55555, 0x5555554C */
  C2                   = -1.38888888888741095749e-03, /* 0xBF56C16C, 0x16C15177 */
  C3                   = 2.48015872894767294178e-05,  /* 0x3EFA01A0, 0x19CB1590 */
  C4                   = -2.75573143513906633035e-07, /* 0xBE927E4F, 0x809C52AD */
  C5                   = 2.08757232129817482790e-09,  /* 0x3E21EE9E, 0xBDB4B1C4 */
  C6                   = -1.13596475577881948265e-11; /* 0xBDA8FAE9, 0xBE8838D4 */

double __cos(double x, double y) {
    double hz, z, r, w;

    z  = x * x;
    w  = z * z;
    r  = z * (C1 + z * (C2 + z * C3)) + w * w * (C4 + z * (C5 + z * C6));
    hz = 0.5 * z;
    w  = 1.0 - hz;
    return w + (((1.0 - w) - hz) + (z * r - x * y));
}


double env_sin(double x) {
    double  y[2], z = 0.0;
    int32_t n, ix;

    /* High word of x. */
    GET_HIGH_WORD(ix, x);

    /* |x| ~< pi/4 */
    ix &= 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e500000) { /* |x| < 2**-26 */
            /* raise inexact if x != 0 */
            if ((int)x == 0) return x;
        }
        return __sin(x, z, 0);
    }

    /* sin(Inf or NaN) is NaN */
    if (ix >= 0x7ff00000) return x - x;

    /* argument reduction needed */
    n = __rem_pio2(x, y);
    switch (n & 3) {
        case 0: return __sin(y[0], y[1], 1);
        case 1: return __cos(y[0], y[1]);
        case 2: return -__sin(y[0], y[1], 1);
        default: return -__cos(y[0], y[1]);
    }
}

double env_cos(double x) {
    double   y[2];
    uint32_t ix;
    unsigned n;

    GET_HIGH_WORD(ix, x);
    ix &= 0x7fffffff;

    /* |x| ~< pi/4 */
    if (ix <= 0x3fe921fb) {
        if (ix < 0x3e46a09e) { /* |x| < 2**-27 * sqrt(2) */
            /* raise inexact if x!=0 */
            FORCE_EVAL(x + 0x1p120f);
            return 1.0;
        }
        return __cos(x, 0);
    }

    /* cos(Inf or NaN) is NaN */
    if (ix >= 0x7ff00000) return x - x;

    /* argument reduction */
    n = __rem_pio2(x, y);
    switch (n & 3) {
        case 0: return __cos(y[0], y[1]);
        case 1: return -__sin(y[0], y[1], 1);
        case 2: return -__cos(y[0], y[1]);
        default: return __sin(y[0], y[1], 1);
    }
}
