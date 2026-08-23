#ifndef DECODE_H
#define DECODE_H

#include <stdint.h>
#include "cpu.h"

/* ------------------------------------------------------------------ */
/* Instruction field extractors                                         */
/* ------------------------------------------------------------------ */

#define INST_OPCODE(i)   ((i) & 0x7F)
#define INST_RD(i)       (((i) >>  7) & 0x1F)
#define INST_FUNCT3(i)   (((i) >> 12) & 0x07)
#define INST_RS1(i)      (((i) >> 15) & 0x1F)
#define INST_RS2(i)      (((i) >> 20) & 0x1F)
#define INST_FUNCT7(i)   (((i) >> 25) & 0x7F)
#define INST_FUNCT5(i)   (((i) >> 27) & 0x1F)
#define INST_RM(i)       (((i) >> 12) & 0x07)
#define INST_FMT(i)      (((i) >> 25) & 0x03)

/* ------------------------------------------------------------------ */
/* Opcode constants                                                     */
/* ------------------------------------------------------------------ */

#define OP_LOAD      0x03
#define OP_LOAD_FP   0x07
#define OP_IMM       0x13
#define OP_AUIPC     0x17
#define OP_IMM32     0x1B
#define OP_STORE     0x23
#define OP_STORE_FP  0x27
#define OP_OP        0x33
#define OP_LUI       0x37
#define OP_OP32      0x3B
#define OP_BRANCH    0x63
#define OP_JALR      0x67
#define OP_JAL       0x6F
#define OP_SYSTEM    0x73
#define OP_FP        0x53

/* ------------------------------------------------------------------ */
/* Float funct5 constants (bits 31:27 of OP-FP instruction)            */
/* ------------------------------------------------------------------ */

#define F5_FADD      0x00
#define F5_FSUB      0x01
#define F5_FMUL      0x02
#define F5_FDIV      0x03
#define F5_FSQRT     0x0B
#define F5_FSGNJ     0x10
#define F5_FMINMAX   0x05
#define F5_FCMP      0x14
#define F5_FCVT_FF   0x08
#define F5_FCVT_FI   0x18
#define F5_FCVT_IF   0x1A
#define F5_FMV_XF    0x1C
#define F5_FMV_FX    0x1E

/* ------------------------------------------------------------------ */
/* Immediate decoders — all return int64_t, all sign-extended           */
/* ------------------------------------------------------------------ */

/* I-type: inst[31:20] → imm[11:0] */
static inline int64_t imm_i(uint32_t i) {
    return (int64_t)(int32_t)(i & 0xFFF00000) >> 20;
}

/* S-type: inst[31:25]→imm[11:5], inst[11:7]→imm[4:0] */
static inline int64_t imm_s(uint32_t i) {
    return (int64_t)(int32_t)(
        (i  & 0xFE000000)       |
        ((i & 0x00000F80) << 13)
    ) >> 20;
}

/* B-type: inst[31]→imm[12], inst[7]→imm[11],
           inst[30:25]→imm[10:5], inst[11:8]→imm[4:1], imm[0]=0 */
static inline int64_t imm_b(uint32_t i) {
    return (int64_t)(int32_t)(
        (i  & 0x80000000)        |
        ((i & 0x00000080) << 23) |
        ((i & 0x7E000000) >>  1) |
        ((i & 0x00000F00) << 12)
    ) >> 19;
}

/* U-type: inst[31:12] → imm[31:12], imm[11:0]=0 */
static inline int64_t imm_u(uint32_t i) {
    return (int64_t)(int32_t)(i & 0xFFFFF000);
}

/* J-type: inst[31]→imm[20], inst[19:12]→imm[19:12],
           inst[20]→imm[11], inst[30:21]→imm[10:1], imm[0]=0 */
static inline int64_t imm_j(uint32_t i) {
    return (int64_t)(int32_t)(
        (i  & 0x80000000)        |
        ((i & 0x000FF000) << 11) |
        ((i & 0x00100000) <<  2) |
        ((i & 0x7FE00000) >> 9)
    ) >> 11;
}

/* ------------------------------------------------------------------ */
/* Rounding mode resolver                                               */
/* rm field == 0x7 means use fcsr.frm dynamically                      */
/* ------------------------------------------------------------------ */

static inline int inst_rm(uint32_t inst, RVCore *cpu) {
    int rm = INST_RM(inst);
    return (rm == 0x7) ? cpu_frm(cpu) : rm;
}

#endif
