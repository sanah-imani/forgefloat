#include <stdint.h>

#define INST_OPCODE(i) ((i) & 0x7F)
#define INST_RD(i) (((i) >> 7) & 0x1F)
#define INST_FUNCT3(i) (((i) >> 12) & 0x07)
#define INST_RS1(i) (((i) >> 15) &0x1F)
#define INST_RS2(i) (((i) >> 20) & 0x7F)
#define INST_FUNCT7 (((i) >> 25) &0x1F)
#define INST_RM(i) (((i) >> 12) & 0x07)
#define INST_FMT(i) (((i) >> 25) & 0x03)
#define INST_FUNCT5(i) (((i) >> 27) & 0x1F)

#define OP_LOAD      0x03
#define OP_LOAD_FP   0x07
#define OP_IMM       0x13
#define OP_AUIPC     0x17
#define OP_IMM32     0x1B   /* RV64: ADDIW etc */
#define OP_STORE     0x23
#define OP_STORE_FP  0x27
#define OP_OP        0x33
#define OP_LUI       0x37
#define OP_OP32      0x3B   /* RV64: ADDW etc */
#define OP_BRANCH    0x63
#define OP_JALR      0x67
#define OP_JAL       0x6F
#define OP_SYSTEM    0x73
#define OP_FP        0x53

static inline int inst_rm(uint32_t inst, RVCore *cpu) {
    int rm = INST_RM(inst);
    return (rm == 0x7) ? cpu_frm(cpu) : rm;
}

/* I-type: inst[31:20] sign-extended */
static inline int64_t imm_i(uint32_t i) {
    return (int64_t)(int32_t)(i & 0xFFF00000) >> 20;
}

/* S-type: {inst[31:25], inst[11:7]} sign-extended */
static inline int64_t imm_s(uint32_t i) {
    return (int64_t)(int32_t)(
        (i & 0xFE000000)        |   /* bits 31:25 → stay */
        ((i & 0x00000F80) << 13)    /* bits 11:7  → shift to 24:20 */
    ) >> 20;
}

/* B-type: {inst[31],inst[7],inst[30:25],inst[11:8],0} sign-extended */
static inline int64_t imm_b(uint32_t i) {
    return (int64_t)(int32_t)(
        ((i & 0x80000000))          |   /* bit 31 → bit 31 (sign) */
        ((i & 0x00000080) << 23)    |   /* bit  7 → bit 30 */
        ((i & 0x7E000000) >> 1)     |   /* bits 30:25 → bits 29:24 */
        ((i & 0x00000F00) << 12)        /* bits 11:8  → bits 23:20 */
    ) >> 19;
}

/* U-type: inst[31:12] << 12, sign-extended */
static inline int64_t imm_u(uint32_t i) {
    return (int64_t)(int32_t)(i & 0xFFFFF000);
}

/* J-type: {inst[31],inst[19:12],inst[20],inst[30:21],0} sign-extended */
static inline int64_t imm_j(uint32_t i) {
    return (int64_t)(int32_t)(
        ((i & 0x80000000))          |   /* bit 31 → bit 31 (sign) */
        ((i & 0x000FF000))          |   /* bits 19:12 → bits 19:12 */
        ((i & 0x00100000) >> 9)     |   /* bit 20    → bit 11 */
        ((i & 0x7FE00000) >> 20)        /* bits 30:21 → bits 10:1 */
    ) >> 11;
}