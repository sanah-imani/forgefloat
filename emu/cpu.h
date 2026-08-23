#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "../softfloat.h"

/* RISC-V privilege levels */
#define PRV_U 0
#define PRV_S 1
#define PRV_M 3

/* fcsr field masks */
#define FCSR_FFLAGS_MASK  0x1Fu   /* bits 4:0  — exception flags */
#define FCSR_FRM_MASK     0xE0u   /* bits 7:5  — rounding mode */
#define FCSR_FRM_SHIFT    5

typedef struct {
    uint64_t x[32];   /* integer registers x0-x31; x0 is hardwired 0 */
    uint64_t f[32];   /* float registers f0-f31; NaN-boxed for f32 values */
    uint64_t pc;      /* program counter */
    uint32_t fcsr;    /* float CSR: frm[7:5] + fflags[4:0] */
    int      prv;     /* current privilege level */
} RVCore;

/* ------------------------------------------------------------------ */
/* NaN-boxing helpers                                                   */
/*                                                                      */
/* RISC-V spec -- a float32 stored in a 64-bit f-register must    */
/* have its upper 32 bits set to all 1s.  Reading a register whose    */
/* upper bits are not all 1s yields a canonical quiet NaN.            */
/* ------------------------------------------------------------------ */

static inline void fset32(uint64_t *freg, float32 val) {
    *freg = 0xFFFFFFFF00000000ull | (uint64_t)val;
}

static inline float32 fget32(uint64_t freg) {
    if ((freg >> 32) != 0xFFFFFFFFu)
        return 0x7FC00000u;  /* canonical quiet NaN */
    return (float32)(freg & 0xFFFFFFFFu);
}

/* float64 uses the full register directly — no boxing needed */
static inline void fset64(uint64_t *freg, float64 val) {
    *freg = val;
}

static inline float64 fget64(uint64_t freg) {
    return freg;
}

/* ------------------------------------------------------------------ */
/* fcsr helpers                                                         */
/* ------------------------------------------------------------------ */

static inline int cpu_frm(RVCore *cpu) {
    return (cpu->fcsr & FCSR_FRM_MASK) >> FCSR_FRM_SHIFT;
}

/* Merge softfloat exception flags back into fcsr.fflags */
static inline void cpu_accum_flags(RVCore *cpu, int sf_flags) {
    cpu->fcsr |= (sf_flags & FCSR_FFLAGS_MASK);
}

/* Build an SFState from the current cpu rounding mode */
static inline SFState cpu_sfstate(RVCore *cpu) {
    return sf_init(cpu_frm(cpu));
}

/* ------------------------------------------------------------------ */
/* Integer register helpers (enforce x0 == 0)                          */
/* ------------------------------------------------------------------ */

static inline uint64_t rx(RVCore *cpu, int reg) {
    return reg ? cpu->x[reg] : 0;
}

static inline void wx(RVCore *cpu, int reg, uint64_t val) {
    if (reg) cpu->x[reg] = val;
}

/* ------------------------------------------------------------------ */
/* Core init                                                            */
/* ------------------------------------------------------------------ */

static inline void cpu_init(RVCore *cpu, uint64_t entry) {
    int i;
    for (i = 0; i < 32; i++) { cpu->x[i] = 0; cpu->f[i] = 0xFFFFFFFF7FC00000ull; }
    cpu->pc   = entry;
    cpu->fcsr = 0;
    cpu->prv  = PRV_M;
}

#endif
