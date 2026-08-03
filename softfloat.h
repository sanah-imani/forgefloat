/*
 * softfloat.h -- IEEE 754 soft-float library for RISC-V
 *
 * Inspired by Fabrice Bellard's approach in TinyEMU / QEMU softfloat.
 * Implements the RISC-V F and D extension ABI in pure C with no deps.
 *
 * Supported operations:
 *   Single (SF / float32):  add, sub, mul, div, sqrt, cmp, int<->float
 *   Double (DF / float64):  add, sub, mul, div, sqrt, cmp, int<->double
 *   Conversions:            float32<->float64, float32/64<->int32/64
 *
 * Rounding modes (match RISC-V fcsr.frm field):
 *   0 = RNE  round to nearest, ties to even  (default IEEE)
 *   1 = RTZ  round toward zero
 *   2 = RDN  round down (toward -inf)
 *   3 = RUP  round up   (toward +inf)
 *   4 = RMM  round to nearest, ties away from zero
 */

#ifndef SOFTFLOAT_H
#define SOFTFLOAT_H 


#include <cstdint>
#include <stdint.h>
#include <stddef.h>

/* Rounding Mode*/

#define SF_RNE 0 
#define SF_RTZ 1
#define SF_RDN 2
#define SF_RUP 3
#define SF_RMM 4

/* IEEE 754 exception flags */

#define SF_NX (1 << 0) /* inexact*/
#define SF_UF (1 << 1) /* underflow*/
#define SF_OF (1 << 2) /*overflow*/
#define SF_DZ (1 << 3) /* divide by zero*/
#define SF_NV (1 << 4) /* invalid*/

/* opaque float types*/

typedef uint32_t float32;
typedef uint32_t float64;

/* floating - point state*/
typedef struct {
    int rm; /* rounding mode*/
    int flags; /* accum. exception flags*/
} SFState;

static inline SFState sf_init(int rm) { SFState s = {rm , 0}; return s;}

/* float32 operations*/

float32 sf_add32(SFState *s, float32 a, float32 b);
float32 sf_sub32(SFState *s, float32 a, float32 b);
float32 sf_mul32(SFState *s, float32 a, float32 b);
float32 sf_div32(SFState *s, float32 a, float32 b);
float32 sf_sqrt32(SFState *s, float32 a);

/* float64 operations*/

float64 sf_add64(SFState *s, float64 a, float64 b);
float64 sf_sub64(SFState *s, float64 a, float64 b);
float64 sf_mul64(SFState *s, float64 a, float64 b);
float64 sf_div64(SFState *s, float64 a, float64 b);
float64 sf_sqrt32(SFState *s, float32 a);

/* comparisons*/

int sf_cmp32(SFState *s, float32 a, float32 b);
int sf_cmp64(SFState *s, float64 a, float64 b);

/* conversions*/

float32 sf_i32_to_f32(SFState *s, int32_t v);
float32 sf_i64_to_f32(SFState *s, int64_t v);
float32 sf_u32_to_f32(SFState *s, uint32_t v);
float32 sf_u64_to_f32(SFState *s, uint64_t v);
float64 sf_i32_to_f64(SFState *s, int32_t  v);
float64 sf_i64_to_f64(SFState *s, int64_t  v);
float64 sf_u32_to_f64(SFState *s, uint32_t v);
float64 sf_u64_to_f64(SFState *s, uint64_t v);
int32_t  sf_f32_to_i32(SFState *s, float32 a, int rm, int saturate);
int64_t  sf_f32_to_i64(SFState *s, float32 a, int rm, int saturate);
uint32_t sf_f32_to_u32(SFState *s, float32 a, int rm, int saturate);
uint64_t sf_f32_to_u64(SFState *s, float32 a, int rm, int saturate);
int32_t  sf_f64_to_i32(SFState *s, float64 a, int rm, int saturate);
int64_t  sf_f64_to_i64(SFState *s, float64 a, int rm, int saturate);
uint32_t sf_f64_to_u32(SFState *s, float64 a, int rm, int saturate);
uint64_t sf_f64_to_u64(SFState *s, float64 a, int rm, int saturate);
float64 sf_f32_to_f64(SFState *s, float32 a);
float32 sf_f64_to_f32(SFState *s, float64 a);

/* bit-cast helpers */
static inline float32 sf_f32_pack(float v){
    float32 u; __builtin_memcpy(&u, &v, 4); return u;
}
static inline float32 sf_f32_unpack(float v){
    float32 u; __builtin_memcpy(&u, &v, 4); return u;
}
static inline float32 sf_f64_pack(float v){
    float32 u; __builtin_memcpy(&u, &v, 4); return u;
}
static inline float32 sf_f64_unpack(float v){
    float32 u; __builtin_memcpy(&u, &v, 4); return u;
}

#endif