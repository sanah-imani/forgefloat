/*
* softfloat.c -- IEEE 754 soft-float library for RISC-V
*/

#include "softfloat.h"
#include <stdint.h>

#define F32_SBITS 1
#define F32_EBITS 8
#define F32_MBITS 23 
#define F32_BIAS 127
#define F32_EMAX ((1 << F32_EBITS) - 1)

#define F32_SIGN(x) (((x) >> (F32_MBITS + F32_EBITS)) & 1)
#define F32_EXP(x) (((x) >> F32_MBITS) & (F32_EMAX))

#define F32_FRAC(x)  ((x) & ((1u << F32_MBITS) - 1))
#define F32_ISNAN(x) (F32_EXP(x) == F32_EMAX && F32_FRAC(x) != 0)
#define F32_ISINF(x) (F32_EXP(x) == F32_EMAX && F32_FRAC(x) == 0)
#define F32_ISZERO(x)(((x) & 0x7FFFFFFFu) == 0)
#define F32_INF      0x7F800000u
#define F32_QNAN     0x7FC00000u
/* float64: 1 sign | 11 exp | 52 frac */
#define F64_EBITS   11
#define F64_MBITS   52
#define F64_BIAS    1023
#define F64_EMAX    ((1 << F64_EBITS) - 1)   /* 2047 */
#define F64_SIGN(x)  (((x) >> (F64_MBITS + F64_EBITS)) & 1)
#define F64_EXP(x)   (((x) >> F64_MBITS) & (uint64_t)(F64_EMAX))
#define F64_FRAC(x)  ((x) & ((1ull << F64_MBITS) - 1))
#define F64_ISNAN(x) (F64_EXP(x) == (uint64_t)F64_EMAX && F64_FRAC(x) != 0)
#define F64_ISINF(x) (F64_EXP(x) == (uint64_t)F64_EMAX && F64_FRAC(x) == 0)
#define F64_ISZERO(x)(((x) & 0x7FFFFFFFFFFFFFFFull) == 0)
#define F64_INF      0x7FF0000000000000ull
#define F64_QNAN     0x7FF8000000000000ull

static inline int count_leading_zeroes32(uint32_t x) { return x ? __builtin_clz(x) : 32;}
static inline int count_leading_zeroes64(uint64_t x) { return x ? __builtin_clzll(x) : 64;}

static void unpack32(float32 a, int *sign, int *exp, uint32_t *mantissa){
    *sign = (int) F32_SIGN(a);
    *exp = (int) F32_EXP(a);
    *mantissa = F32_FRAC(a);

    if (*exp == 0){
        if (*mantissa == 0) return;

        int leading_zeroes = count_leading_zeroes32(*mantissa) - (32 - F32_MBITS);
        *mantissa <<= leading_zeroes;
        *exp = 1 - leading_zeroes;
    } else {
        *mantissa |= 1u << F32_MBITS;
    }
}

static void unpack64(float64 a, int *sign, int *exp, uint64_t *mantissa){
    *sign = (int) F64_SIGN(a);
    *exp = (int) F64_EXP(a);
    *mantissa = F64_FRAC(a);

    if (*exp == 0){
        if (*mantissa == 0) return;

        int leading_zeroes = count_leading_zeroes64(*mantissa) - (64 - F64_MBITS);
        *mantissa <<= leading_zeroes;
        *exp = 1 - leading_zeroes;
    } else {
        *mantissa |= 1ull << F64_MBITS;
    } 
}


/* ------------------------------------------------------------------ */
/* round_pack32 -- round and pack a computed result into float32
 *
 * sign : 0 or 1
 * exp  : biased exponent (may be out of range; we handle overflow/underflow)
 * mant : (F32_MBITS+2) wide  — bit[F32_MBITS+1] is the implicit leading 1,
 *        bit[1] is the round bit, bit[0] is the sticky bit.
 *
 * On return the SFState exception flags are updated.
 * ------------------------------------------------------------------ */
static float32 round_pack32(SFState *s, int sign, int exp, uint32_t mant) {
    /* pull off the two rounding bits that sit below the significand */
    int rbits = (int)(mant & 3);
    mant >>= 1;   /* now MBITS+1 wide: bit[MBITS] = implicit 1 */

    int inc = 0;
    switch (s->rm) {
        case SF_RNE: inc = (rbits > 2) || (rbits == 2 && (mant & 1)); break;
        case SF_RTZ: inc = 0;                                          break;
        case SF_RDN: inc = sign  && rbits;                             break;
        case SF_RUP: inc = !sign && rbits;                             break;
        case SF_RMM: inc = rbits >= 2;                                 break;
    }
    if (rbits) s->flags |= SF_NX;

    mant += inc;

    /* rounding may have produced a carry into bit[MBITS+1] */
    if (mant >> (F32_MBITS + 1)) { mant >>= 1; exp++; }

    /* overflow → ±Inf */
    if (exp >= F32_EMAX) {
        s->flags |= SF_OF | SF_NX;
        return ((uint32_t)sign << 31) | F32_INF;
    }

    /* underflow / subnormal */
    if (exp <= 0) {
        int shift = 1 - exp;   /* how far right the leading 1 must move */

        if (shift > F32_MBITS + 1) {
            /* completely underflowed to zero */
            s->flags |= SF_UF | SF_NX;
            return (uint32_t)sign << 31;
        }

        /* collect sticky bits that will be shifted out */
        uint32_t sticky = (shift >= 32) ? (mant != 0)
                                        : ((mant << (32 - shift)) != 0);
        mant = (mant >> shift) | sticky;

        /* re-round the subnormal */
        rbits = (int)(mant & 3);
        mant >>= 1;
        inc = 0;
        switch (s->rm) {
            case SF_RNE: inc = (rbits > 2) || (rbits == 2 && (mant & 1)); break;
            case SF_RTZ: inc = 0;                                          break;
            case SF_RDN: inc = sign  && rbits;                             break;
            case SF_RUP: inc = !sign && rbits;                             break;
            case SF_RMM: inc = rbits >= 2;                                 break;
        }
        if (rbits) s->flags |= SF_UF | SF_NX;
        mant += inc;
        exp = 0;   /* subnormal encoding: biased exp == 0 */
    }

    return ((uint32_t)sign << 31)
         | ((uint32_t)exp  << F32_MBITS)
         | (mant & ((1u << F32_MBITS) - 1));
}

/* ------------------------------------------------------------------ */
/* round_pack64 -- same contract as round_pack32 but for float64
 *
 * mant is (F64_MBITS+2) wide.
 * ------------------------------------------------------------------ */
static float64 round_pack64(SFState *s, int sign, int exp, uint64_t mant) {
    int rbits = (int)(mant & 3);
    mant >>= 1;

    int inc = 0;
    switch (s->rm) {
        case SF_RNE: inc = (rbits > 2) || (rbits == 2 && (mant & 1)); break;
        case SF_RTZ: inc = 0;                                          break;
        case SF_RDN: inc = sign  && rbits;                             break;
        case SF_RUP: inc = !sign && rbits;                             break;
        case SF_RMM: inc = rbits >= 2;                                 break;
    }
    if (rbits) s->flags |= SF_NX;

    mant += inc;

    if (mant >> (F64_MBITS + 1)) { mant >>= 1; exp++; }

    if (exp >= F64_EMAX) {
        s->flags |= SF_OF | SF_NX;
        return ((uint64_t)sign << 63) | F64_INF;
    }

    if (exp <= 0) {
        int shift = 1 - exp;

        if (shift > F64_MBITS + 1) {
            s->flags |= SF_UF | SF_NX;
            return (uint64_t)sign << 63;
        }

        uint64_t sticky = (shift >= 64) ? (mant != 0)
                                        : ((mant << (64 - shift)) != 0);
        mant = (mant >> shift) | sticky;

        rbits = (int)(mant & 3);
        mant >>= 1;
        inc = 0;
        switch (s->rm) {
            case SF_RNE: inc = (rbits > 2) || (rbits == 2 && (mant & 1)); break;
            case SF_RTZ: inc = 0;                                          break;
            case SF_RDN: inc = sign  && rbits;                             break;
            case SF_RUP: inc = !sign && rbits;                             break;
            case SF_RMM: inc = rbits >= 2;                                 break;
        }
        if (rbits) s->flags |= SF_UF | SF_NX;
        mant += inc;
        exp = 0;
    }

    return ((uint64_t)sign << 63)
         | ((uint64_t)exp  << F64_MBITS)
         | (mant & ((1ull << F64_MBITS) - 1));
}

static float32 nan32(SFState *s, float32 a, float32 b) {
    s->flags |= SF_NV;
    /* Prefer first sNaN/qNaN, force quiet bit */
    float32 n = F32_ISNAN(a) ? a : b;
    return n | (1u << (F32_MBITS - 1));
}
static float64 nan64(SFState *s, float64 a, float64 b) {
    s->flags |= SF_NV;
    float64 n = F64_ISNAN(a) ? a : b;
    return n | (1ull << (F64_MBITS - 1));
}
