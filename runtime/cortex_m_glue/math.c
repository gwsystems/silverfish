#include <stdint.h>
#include <string.h>

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

void __aeabi_memset(void* dest, size_t n, int c) {
    memset(dest, c, n);
}


void __aeabi_memset4(void* dest, size_t n, int c) {
    memset(dest, c, n);
}

void* memcpy(void* dest, const void* src, size_t len) {
    char*       d = dest;
    const char* s = src;
    while (len--)
        *d++ = *s++;
    return dest;
}

void __aeabi_memcpy4(void* dest, const void* src, size_t len) {
    memcpy(dest, src, len);
}

int strcmp(const char* l, const char* r) {
    for (; *l == *r && *l; l++, r++)
        ;
    return *(unsigned char*)l - *(unsigned char*)r;
}

size_t strlen(const char* s) {
    int i;
    for (i = 0; s[i] != '\0'; i++)
        ;
    return i;
}


int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--)
        if (*p1 != *p2)
            return *p1 - *p2;
        else
            p1++, p2++;
    return 0;
}


int stdin  = 0;
int stdout = 1;
int stderr = 2;

void __aeabi_memclr8(void* dest, size_t n) {
    memset(dest, 0, n);
}

void __aeabi_memclr4(void* dest, size_t n) {
    memset(dest, 0, n);
}

//#define WT uintptr_t
//#define WS (sizeof(WT))
//
// void *memmove(void *dest, const void *src, uintptr_t n)
//{
//	char *d = dest;
//	const char *s = src;
//
//	if (d==s) return d;
//	if (s+n <= d || d+n <= s) return memcpy(d, s, n);
//
//	if (d<s) {
//		if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
//			while ((uintptr_t)d % WS) {
//				if (!n--) return dest;
//				*d++ = *s++;
//			}
//			for (; n>=WS; n-=WS, d+=WS, s+=WS) *(WT *)d = *(WT *)s;
//		}
//		for (; n; n--) *d++ = *s++;
//	} else {
//		if ((uintptr_t)s % WS == (uintptr_t)d % WS) {
//			while ((uintptr_t)(d+n) % WS) {
//				if (!n--) return dest;
//				d[n] = s[n];
//			}
//			while (n>=WS) n-=WS, *(WT *)(d+n) = *(WT *)(s+n);
//		}
//		while (n) n--, d[n] = s[n];
//	}
//
//	return dest;
//}


double fabs(double x) {
    union {
        double   f;
        uint64_t i;
    } u = { x };
    u.i &= -1ULL / 2;
    return u.f;
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

/* Get the less significant 32 bit int from a double.  */
#define GET_LOW_WORD(lo, d)      \
    do {                         \
        union {                  \
            double   f;          \
            uint64_t i;          \
        } __u;                   \
        __u.f = (d);             \
        (lo)  = (uint32_t)__u.i; \
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

/* Set the more significant 32 bits of a double from an int.  */
#define SET_HIGH_WORD(d, hi)           \
    do {                               \
        union {                        \
            double   f;                \
            uint64_t i;                \
        } __u;                         \
        __u.f = (d);                   \
        __u.i &= 0xffffffff;           \
        __u.i |= (uint64_t)(hi) << 32; \
        (d) = __u.f;                   \
    } while (0)

/* Set the less significant 32 bits of a double from an int.  */
#define SET_LOW_WORD(d, lo)             \
    do {                                \
        union {                         \
            double   f;                 \
            uint64_t i;                 \
        } __u;                          \
        __u.f = (d);                    \
        __u.i &= 0xffffffff00000000ull; \
        __u.i |= (uint32_t)(lo);        \
        (d) = __u.f;                    \
    } while (0)


static __inline unsigned __FLOAT_BITS(float __f) {
    union {
        float    __f;
        unsigned __i;
    } __u;
    __u.__f = __f;
    return __u.__i;
}

static __inline unsigned long long __DOUBLE_BITS(double __f) {
    union {
        double             __f;
        unsigned long long __i;
    } __u;
    __u.__f = __f;
    return __u.__i;
}

#define isnan(x)                                                              \
    (sizeof(x) == sizeof(float) ? (__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 \
                                : sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL >> 1) > 0x7ffULL << 52 : 0)


#define EPS (0x1p-52)

double trunc(double x) {
    union {
        double   f;
        uint64_t i;
    } u        = { x };
    int      e = (int)(u.i >> 52 & 0x7ff) - 0x3ff + 12;
    uint64_t m;

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
        float    f;
        uint32_t i;
    } u        = { x };
    int      e = (int)(u.i >> 23 & 0xff) - 0x7f + 9;
    uint32_t m;

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

float fabsf(float x) {
    union {
        float    f;
        uint32_t i;
    } u = { x };
    u.i &= 0x7fffffff;
    return u.f;
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
            // TODO: drop excess precision here once double is used
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

double cos(double x) {
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


static const double atanhi[] = {
    4.63647609000806093515e-01, /* atan(0.5)hi 0x3FDDAC67, 0x0561BB4F */
    7.85398163397448278999e-01, /* atan(1.0)hi 0x3FE921FB, 0x54442D18 */
    9.82793723247329054082e-01, /* atan(1.5)hi 0x3FEF730B, 0xD281F69B */
    1.57079632679489655800e+00, /* atan(inf)hi 0x3FF921FB, 0x54442D18 */
};

static const double atanlo[] = {
    2.26987774529616870924e-17, /* atan(0.5)lo 0x3C7A2B7F, 0x222F65E2 */
    3.06161699786838301793e-17, /* atan(1.0)lo 0x3C81A626, 0x33145C07 */
    1.39033110312309984516e-17, /* atan(1.5)lo 0x3C700788, 0x7AF0CBBD */
    6.12323399573676603587e-17, /* atan(inf)lo 0x3C91A626, 0x33145C07 */
};

static const double aT[] = {
    3.33333333333329318027e-01,  /* 0x3FD55555, 0x5555550D */
    -1.99999999998764832476e-01, /* 0xBFC99999, 0x9998EBC4 */
    1.42857142725034663711e-01,  /* 0x3FC24924, 0x920083FF */
    -1.11111104054623557880e-01, /* 0xBFBC71C6, 0xFE231671 */
    9.09088713343650656196e-02,  /* 0x3FB745CD, 0xC54C206E */
    -7.69187620504482999495e-02, /* 0xBFB3B0F2, 0xAF749A6D */
    6.66107313738753120669e-02,  /* 0x3FB10D66, 0xA0D03D51 */
    -5.83357013379057348645e-02, /* 0xBFADDE2D, 0x52DEFD9A */
    4.97687799461593236017e-02,  /* 0x3FA97B4B, 0x24760DEB */
    -3.65315727442169155270e-02, /* 0xBFA2B444, 0x2C6A6C2F */
    1.62858201153657823623e-02,  /* 0x3F90AD3A, 0xE322DA11 */
};

double atan(double x) {
    double   w, s1, s2, z;
    uint32_t ix, sign;
    int      id;

    GET_HIGH_WORD(ix, x);
    sign = ix >> 31;
    ix &= 0x7fffffff;
    if (ix >= 0x44100000) { /* if |x| >= 2^66 */
        if (isnan(x)) return x;
        z = atanhi[3] + 0x1p-120f;
        return sign ? -z : z;
    }
    if (ix < 0x3fdc0000) {       /* |x| < 0.4375 */
        if (ix < 0x3e400000) {   /* |x| < 2^-27 */
            if (ix < 0x00100000) /* raise underflow for subnormal x */
                FORCE_EVAL((float)x);
            return x;
        }
        id = -1;
    } else {
        x = fabs(x);
        if (ix < 0x3ff30000) {     /* |x| < 1.1875 */
            if (ix < 0x3fe60000) { /*  7/16 <= |x| < 11/16 */
                id = 0;
                x  = (2.0 * x - 1.0) / (2.0 + x);
            } else { /* 11/16 <= |x| < 19/16 */
                id = 1;
                x  = (x - 1.0) / (x + 1.0);
            }
        } else {
            if (ix < 0x40038000) { /* |x| < 2.4375 */
                id = 2;
                x  = (x - 1.5) / (1.0 + 1.5 * x);
            } else { /* 2.4375 <= |x| < 2^66 */
                id = 3;
                x  = -1.0 / x;
            }
        }
    }
    /* end of argument reduction */
    z = x * x;
    w = z * z;
    /* break sum from i=0 to 10 aT[i]z**(i+1) into odd and even poly */
    s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0) return x - x * (s1 + s2);
    z = atanhi[id] - (x * (s1 + s2) - atanlo[id] - x);
    return sign ? -z : z;
}

static const double pio2_hi = 1.57079632679489655800e+00,  /* 0x3FF921FB, 0x54442D18 */
  pio2_lo                   = 6.12323399573676603587e-17,  /* 0x3C91A626, 0x33145C07 */
  pS0                       = 1.66666666666666657415e-01,  /* 0x3FC55555, 0x55555555 */
  pS1                       = -3.25565818622400915405e-01, /* 0xBFD4D612, 0x03EB6F7D */
  pS2                       = 2.01212532134862925881e-01,  /* 0x3FC9C155, 0x0E884455 */
  pS3                       = -4.00555345006794114027e-02, /* 0xBFA48228, 0xB5688F3B */
  pS4                       = 7.91534994289814532176e-04,  /* 0x3F49EFE0, 0x7501B288 */
  pS5                       = 3.47933107596021167570e-05,  /* 0x3F023DE1, 0x0DFDF709 */
  qS1                       = -2.40339491173441421878e+00, /* 0xC0033A27, 0x1C8A2D4B */
  qS2                       = 2.02094576023350569471e+00,  /* 0x40002AE5, 0x9C598AC8 */
  qS3                       = -6.88283971605453293030e-01, /* 0xBFE6066C, 0x1B8D0159 */
  qS4                       = 7.70381505559019352791e-02;  /* 0x3FB3B8C5, 0xB12E9282 */

static double R(double z) {
    double p, q;
    p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
    q = 1.0 + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
    return p / q;
}

double acos(double x) {
    double   z, w, s, c, df;
    uint32_t hx, ix;

    GET_HIGH_WORD(hx, x);
    ix = hx & 0x7fffffff;
    /* |x| >= 1 or nan */
    if (ix >= 0x3ff00000) {
        uint32_t lx;

        GET_LOW_WORD(lx, x);
        if ((ix - 0x3ff00000 | lx) == 0) {
            /* acos(1)=0, acos(-1)=pi */
            if (hx >> 31) return 2 * pio2_hi + 0x1p-120f;
            return 0;
        }
        return 0 / (x - x);
    }
    /* |x| < 0.5 */
    if (ix < 0x3fe00000) {
        if (ix <= 0x3c600000) /* |x| < 2**-57 */
            return pio2_hi + 0x1p-120f;
        return pio2_hi - (x - (pio2_lo - x * R(x * x)));
    }
    /* x < -0.5 */
    if (hx >> 31) {
        z = (1.0 + x) * 0.5;
        s = sqrt(z);
        w = R(z) * s - pio2_lo;
        return 2 * (pio2_hi - (s + w));
    }
    /* x > 0.5 */
    z  = (1.0 - x) * 0.5;
    s  = sqrt(z);
    df = s;
    SET_LOW_WORD(df, 0);
    c = (z - df * df) / (s + df);
    w = R(z) * s + c;
    return 2 * (df + w);
}

float scalbnf(float x, int n) {
    union {
        float    f;
        uint32_t i;
    } u;
    float y = x;

    if (n > 127) {
        y *= 0x1p127f;
        n -= 127;
        if (n > 127) {
            y *= 0x1p127f;
            n -= 127;
            if (n > 127) n = 127;
        }
    } else if (n < -126) {
        y *= 0x1p-126f;
        n += 126;
        if (n < -126) {
            y *= 0x1p-126f;
            n += 126;
            if (n < -126) n = -126;
        }
    }
    u.i = (uint32_t)(0x7f + n) << 23;
    x   = y * u.f;
    return x;
}
