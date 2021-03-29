#include <stdint.h>

float scalbnf(float x, int n);

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

/* Get a 32 bit int from a float.  */
#define GET_FLOAT_WORD(w, d) \
    do {                     \
        union {              \
            float    f;      \
            uint32_t i;      \
        } __u;               \
        __u.f = (d);         \
        (w)   = __u.i;       \
    } while (0)

/* Set a float from a 32 bit int.  */
#define SET_FLOAT_WORD(d, w) \
    do {                     \
        union {              \
            float    f;      \
            uint32_t i;      \
        } __u;               \
        __u.i = (w);         \
        (d)   = __u.f;       \
    } while (0)


static const float half[2] = { 0.5, -0.5 }, ln2hi = 6.9314575195e-1f, /* 0x3f317200 */
  ln2lo  = 1.4286067653e-6f,                                          /* 0x35bfbe8e */
  invln2 = 1.4426950216e+0f,                                          /* 0x3fb8aa3b */
  /*
   * Domain [-0.34568, 0.34568], range ~[-4.278e-9, 4.447e-9]:
   * |x*(exp(x)+1)/(exp(x)-1) - p(x)| < 2**-27.74
   */
  P1 = 1.6666625440e-1f,  /*  0xaaaa8f.0p-26 */
  P2 = -2.7667332906e-3f; /* -0xb55215.0p-32 */

float expf(float x) {
    float    hi, lo, c, xx, y;
    int      k, sign;
    uint32_t hx;

    GET_FLOAT_WORD(hx, x);
    sign = hx >> 31;  /* sign bit of x */
    hx &= 0x7fffffff; /* high word of |x| */

    /* special cases */
    if (hx >= 0x42aeac50) {              /* if |x| >= -87.33655f or NaN */
        if (hx >= 0x42b17218 && !sign) { /* x >= 88.722839f */
            /* overflow */
            x *= 0x1p127f;
            return x;
        }
        if (sign) {
            /* underflow */
            FORCE_EVAL(-0x1p-149f / x);
            if (hx >= 0x42cff1b5) /* x <= -103.972084f */
                return 0;
        }
    }

    /* argument reduction */
    if (hx > 0x3eb17218) {   /* if |x| > 0.5 ln2 */
        if (hx > 0x3f851592) /* if |x| > 1.5 ln2 */
            k = invln2 * x + half[sign];
        else
            k = 1 - sign - sign;
        hi = x - k * ln2hi; /* k*ln2hi is exact here */
        lo = k * ln2lo;
        x  = hi - lo;
    } else if (hx > 0x39000000) { /* |x| > 2**-14 */
        k  = 0;
        hi = x;
        lo = 0;
    } else {
        /* raise inexact */
        FORCE_EVAL(0x1p127f + x);
        return 1 + x;
    }

    /* x is now in primary range */
    xx = x * x;
    c  = x - xx * (P1 + xx * P2);
    y  = 1 + (x * c / (2 - c) - lo + hi);
    if (k == 0) return y;
    return scalbnf(y, k);
}

static const float tiny = 1.0e-30;

float sqrtf(float x) {
    float    z;
    int32_t  sign = (int)0x80000000;
    int32_t  ix, s, q, m, t, i;
    uint32_t r;

    GET_FLOAT_WORD(ix, x);

    /* take care of Inf and NaN */
    if ((ix & 0x7f800000) == 0x7f800000) return x * x + x; /* sqrt(NaN)=NaN, sqrt(+inf)=+inf, sqrt(-inf)=sNaN */

    /* take care of zero */
    if (ix <= 0) {
        if ((ix & ~sign) == 0) return x;      /* sqrt(+-0) = +-0 */
        if (ix < 0) return (x - x) / (x - x); /* sqrt(-ve) = sNaN */
    }
    /* normalize x */
    m = ix >> 23;
    if (m == 0) { /* subnormal x */
        for (i = 0; (ix & 0x00800000) == 0; i++)
            ix <<= 1;
        m -= i - 1;
    }
    m -= 127; /* unbias exponent */
    ix = (ix & 0x007fffff) | 0x00800000;
    if (m & 1) /* odd m, double x to make it even */
        ix += ix;
    m >>= 1; /* m = [m/2] */

    /* generate sqrt(x) bit by bit */
    ix += ix;
    q = s = 0;          /* q = sqrt(x) */
    r     = 0x01000000; /* r = moving bit from right to left */

    while (r != 0) {
        t = s + r;
        if (t <= ix) {
            s = t + r;
            ix -= t;
            q += r;
        }
        ix += ix;
        r >>= 1;
    }

    /* use floating add to find out rounding direction */
    if (ix != 0) {
        z = 1.0f - tiny; /* raise inexact flag */
        if (z >= 1.0f) {
            z = 1.0f + tiny;
            if (z > 1.0f)
                q += 2;
            else
                q += q & 1;
        }
    }
    ix = (q >> 1) + 0x3f000000;
    ix += m << 23;
    SET_FLOAT_WORD(z, ix);
    return z;
}
