#include "execute.h"
#include "decode.h"
#include "../softfloat.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* sign-extend a value of 'bits' width to 64 bits */
static inline int64_t sext(uint64_t val, int bits) {
    int shift = 64 - bits;
    return (int64_t)(val << shift) >> shift;
}

/* resolve rounding mode: inst rm field 0x7 → use fcsr.frm */
static inline int get_rm(uint32_t inst, RVCore *cpu) {
    int rm = INST_RM(inst);
    return (rm == 0x7) ? cpu_frm(cpu) : rm;
}

/* ------------------------------------------------------------------ */
/* execute_one                                                          */
/* ------------------------------------------------------------------ */

int execute_one(RVCore *cpu, Mem *mem) {
    uint32_t inst;
    if (mem_read32(mem, cpu->pc, &inst)) return TRAP_IACCESS;

    int opcode = INST_OPCODE(inst);
    int rd     = INST_RD(inst);
    int rs1    = INST_RS1(inst);
    int rs2    = INST_RS2(inst);
    int funct3 = INST_FUNCT3(inst);
    int funct7 = INST_FUNCT7(inst);

    switch (opcode) {

    /* ---------------------------------------------------------------- */
    /* LUI                                                               */
    /* ---------------------------------------------------------------- */
    case OP_LUI:
        wx(cpu, rd, (uint64_t)imm_u(inst));
        cpu->pc += 4;
        break;

    /* ---------------------------------------------------------------- */
    /* AUIPC                                                             */
    /* ---------------------------------------------------------------- */
    case OP_AUIPC:
        wx(cpu, rd, cpu->pc + (uint64_t)imm_u(inst));
        cpu->pc += 4;
        break;

    /* ---------------------------------------------------------------- */
    /* JAL                                                               */
    /* ---------------------------------------------------------------- */
    case OP_JAL:
        wx(cpu, rd, cpu->pc + 4);
        cpu->pc += (uint64_t)imm_j(inst);
        break;

    /* ---------------------------------------------------------------- */
    /* JALR                                                              */
    /* ---------------------------------------------------------------- */
    case OP_JALR:
        {
            uint64_t target = (rx(cpu, rs1) + (uint64_t)imm_i(inst)) & ~1ull;
            wx(cpu, rd, cpu->pc + 4);
            cpu->pc = target;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* BRANCH                                                            */
    /* ---------------------------------------------------------------- */
    case OP_BRANCH:
        {
            uint64_t a = rx(cpu, rs1), b = rx(cpu, rs2);
            int taken = 0;
            switch (funct3) {
                case 0x0: taken = (a == b);                        break; /* BEQ  */
                case 0x1: taken = (a != b);                        break; /* BNE  */
                case 0x4: taken = ((int64_t)a <  (int64_t)b);     break; /* BLT  */
                case 0x5: taken = ((int64_t)a >= (int64_t)b);     break; /* BGE  */
                case 0x6: taken = (a < b);                         break; /* BLTU */
                case 0x7: taken = (a >= b);                        break; /* BGEU */
                default: return TRAP_ILLEGAL;
            }
            cpu->pc += taken ? (uint64_t)imm_b(inst) : 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* LOAD                                                              */
    /* ---------------------------------------------------------------- */
    case OP_LOAD:
        {
            uint64_t addr = rx(cpu, rs1) + (uint64_t)imm_i(inst);
            uint64_t val  = 0;
            int      err  = 0;
            switch (funct3) {
                case 0x0: { uint8_t  v; err = mem_read8 (mem, addr, &v); val = sext(v,  8); break; } /* LB  */
                case 0x1: { uint16_t v; err = mem_read16(mem, addr, &v); val = sext(v, 16); break; } /* LH  */
                case 0x2: { uint32_t v; err = mem_read32(mem, addr, &v); val = sext(v, 32); break; } /* LW  */
                case 0x3: { uint64_t v; err = mem_read64(mem, addr, &v); val = v;           break; } /* LD  */
                case 0x4: { uint8_t  v; err = mem_read8 (mem, addr, &v); val = v;           break; } /* LBU */
                case 0x5: { uint16_t v; err = mem_read16(mem, addr, &v); val = v;           break; } /* LHU */
                case 0x6: { uint32_t v; err = mem_read32(mem, addr, &v); val = v;           break; } /* LWU */
                default: return TRAP_ILLEGAL;
            }
            if (err) return TRAP_DACCESS;
            wx(cpu, rd, val);
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* STORE                                                             */
    /* ---------------------------------------------------------------- */
    case OP_STORE:
        {
            uint64_t addr = rx(cpu, rs1) + (uint64_t)imm_s(inst);
            uint64_t val  = rx(cpu, rs2);
            int      err  = 0;
            switch (funct3) {
                case 0x0: err = mem_write8 (mem, addr, (uint8_t) val);  break; /* SB */
                case 0x1: err = mem_write16(mem, addr, (uint16_t)val);  break; /* SH */
                case 0x2: err = mem_write32(mem, addr, (uint32_t)val);  break; /* SW */
                case 0x3: err = mem_write64(mem, addr,            val); break; /* SD */
                default: return TRAP_ILLEGAL;
            }
            if (err) return TRAP_DACCESS;
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* OP-IMM  (ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI)        */
    /* ---------------------------------------------------------------- */
    case OP_IMM:
        {
            uint64_t a   = rx(cpu, rs1);
            int64_t  imm = imm_i(inst);
            uint64_t res = 0;
            int shamt    = (int)(inst >> 20) & 0x3F;
            switch (funct3) {
                case 0x0: res = a + (uint64_t)imm;                         break; /* ADDI  */
                case 0x1: res = a << shamt;                                 break; /* SLLI  */
                case 0x2: res = (uint64_t)((int64_t)a < imm);              break; /* SLTI  */
                case 0x3: res = (uint64_t)(a < (uint64_t)imm);             break; /* SLTIU */
                case 0x4: res = a ^ (uint64_t)imm;                         break; /* XORI  */
                case 0x5: res = funct7 & 0x20 ? (uint64_t)((int64_t)a >> shamt)
                                               : (a >> shamt);              break; /* SRLI/SRAI */
                case 0x6: res = a | (uint64_t)imm;                         break; /* ORI   */
                case 0x7: res = a & (uint64_t)imm;                         break; /* ANDI  */
            }
            wx(cpu, rd, res);
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* OP-IMM-32  (ADDIW, SLLIW, SRLIW, SRAIW) — RV64                  */
    /* ---------------------------------------------------------------- */
    case OP_IMM32:
        {
            uint64_t a  = rx(cpu, rs1);
            int shamt   = (int)(inst >> 20) & 0x1F;
            uint64_t res = 0;
            switch (funct3) {
                case 0x0: res = (uint64_t)(int64_t)(int32_t)((uint32_t)a + (uint32_t)imm_i(inst)); break; /* ADDIW */
                case 0x1: res = (uint64_t)(int64_t)(int32_t)((uint32_t)a << shamt);                break; /* SLLIW */
                case 0x5: res = funct7 & 0x20
                    ? (uint64_t)(int64_t)(int32_t)((uint32_t)a >> shamt)  /* SRAIW */
                    : (uint64_t)(int64_t)(int32_t)((uint32_t)a >> shamt); break; /* SRLIW */
                default: return TRAP_ILLEGAL;
            }
            wx(cpu, rd, res);
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* OP  (ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND)          */
    /* ---------------------------------------------------------------- */
    case OP_OP:
        {
            uint64_t a = rx(cpu, rs1), b = rx(cpu, rs2);
            uint64_t res = 0;
            int shamt = (int)(b & 0x3F);
            switch (funct3 | (funct7 << 3)) {
                case 0x0:        res = a + b;                           break; /* ADD  */
                case 0x0 | (0x20<<3): res = a - b;                     break; /* SUB  */
                case 0x1:        res = a << shamt;                      break; /* SLL  */
                case 0x2:        res = (uint64_t)((int64_t)a < (int64_t)b); break; /* SLT  */
                case 0x3:        res = (uint64_t)(a < b);               break; /* SLTU */
                case 0x4:        res = a ^ b;                           break; /* XOR  */
                case 0x5:        res = a >> shamt;                      break; /* SRL  */
                case 0x5 | (0x20<<3): res = (uint64_t)((int64_t)a >> shamt); break; /* SRA  */
                case 0x6:        res = a | b;                           break; /* OR   */
                case 0x7:        res = a & b;                           break; /* AND  */
                default: return TRAP_ILLEGAL;
            }
            wx(cpu, rd, res);
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* OP-32  (ADDW, SUBW, SLLW, SRLW, SRAW) — RV64                    */
    /* ---------------------------------------------------------------- */
    case OP_OP32:
        {
            uint32_t a = (uint32_t)rx(cpu, rs1), b = (uint32_t)rx(cpu, rs2);
            int shamt  = (int)(b & 0x1F);
            uint64_t res = 0;
            switch (funct3 | (funct7 << 3)) {
                case 0x0:             res = (uint64_t)(int64_t)(int32_t)(a + b);         break; /* ADDW */
                case 0x0|(0x20<<3):   res = (uint64_t)(int64_t)(int32_t)(a - b);         break; /* SUBW */
                case 0x1:             res = (uint64_t)(int64_t)(int32_t)(a << shamt);    break; /* SLLW */
                case 0x5:             res = (uint64_t)(int64_t)(int32_t)(a >> shamt);    break; /* SRLW */
                case 0x5|(0x20<<3):   res = (uint64_t)(int64_t)((int32_t)a >> shamt);   break; /* SRAW */
                default: return TRAP_ILLEGAL;
            }
            wx(cpu, rd, res);
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* LOAD-FP  (FLW, FLD)                                              */
    /* ---------------------------------------------------------------- */
    case OP_LOAD_FP:
        {
            uint64_t addr = rx(cpu, rs1) + (uint64_t)imm_i(inst);
            int err = 0;
            switch (funct3) {
                case 0x2: { /* FLW */
                    uint32_t v;
                    err = mem_read32(mem, addr, &v);
                    if (!err) fset32(&cpu->f[rd], v);
                    break;
                }
                case 0x3: { /* FLD */
                    uint64_t v;
                    err = mem_read64(mem, addr, &v);
                    if (!err) fset64(&cpu->f[rd], v);
                    break;
                }
                default: return TRAP_ILLEGAL;
            }
            if (err) return TRAP_DACCESS;
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* STORE-FP  (FSW, FSD)                                             */
    /* ---------------------------------------------------------------- */
    case OP_STORE_FP:
        {
            uint64_t addr = rx(cpu, rs1) + (uint64_t)imm_s(inst);
            int err = 0;
            switch (funct3) {
                case 0x2: err = mem_write32(mem, addr, (uint32_t)fget32(cpu->f[rs2])); break; /* FSW */
                case 0x3: err = mem_write64(mem, addr, fget64(cpu->f[rs2]));            break; /* FSD */
                default: return TRAP_ILLEGAL;
            }
            if (err) return TRAP_DACCESS;
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* OP-FP  — all float arithmetic                                    */
    /* ---------------------------------------------------------------- */
    case OP_FP:
        {
            int funct5 = INST_FUNCT5(inst);
            int fmt    = INST_FMT(inst);   /* 0=S (f32), 1=D (f64) */
            int rm     = get_rm(inst, cpu);
            SFState sf = sf_init(rm);

            float32 as = fget32(cpu->f[rs1]), bs = fget32(cpu->f[rs2]);
            float64 ad = fget64(cpu->f[rs1]), bd = fget64(cpu->f[rs2]);

            switch (funct5) {

            case F5_FADD:
                if (fmt == 0) fset32(&cpu->f[rd], sf_add32(&sf, as, bs));
                else          fset64(&cpu->f[rd], sf_add64(&sf, ad, bd));
                break;

            case F5_FSUB:
                if (fmt == 0) fset32(&cpu->f[rd], sf_sub32(&sf, as, bs));
                else          fset64(&cpu->f[rd], sf_sub64(&sf, ad, bd));
                break;

            case F5_FMUL:
                if (fmt == 0) fset32(&cpu->f[rd], sf_mul32(&sf, as, bs));
                else          fset64(&cpu->f[rd], sf_mul64(&sf, ad, bd));
                break;

            case F5_FDIV:
                if (fmt == 0) fset32(&cpu->f[rd], sf_div32(&sf, as, bs));
                else          fset64(&cpu->f[rd], sf_div64(&sf, ad, bd));
                break;

            case F5_FSQRT:
                if (fmt == 0) fset32(&cpu->f[rd], sf_sqrt32(&sf, as));
                else          fset64(&cpu->f[rd], sf_sqrt64(&sf, ad));
                break;

            case F5_FSGNJ:
                /* funct3 selects: 0=FSGNJ, 1=FSGNJN, 2=FSGNJX */
                if (fmt == 0) {
                    uint32_t sign;
                    switch (funct3) {
                        case 0: sign =  bs & 0x80000000u;          break; /* FSGNJ  */
                        case 1: sign = ~bs & 0x80000000u;          break; /* FSGNJN */
                        case 2: sign = (as ^ bs) & 0x80000000u;    break; /* FSGNJX */
                        default: return TRAP_ILLEGAL;
                    }
                    fset32(&cpu->f[rd], (as & 0x7FFFFFFFu) | sign);
                } else {
                    uint64_t sign;
                    switch (funct3) {
                        case 0: sign =  bd & 0x8000000000000000ull; break;
                        case 1: sign = ~bd & 0x8000000000000000ull; break;
                        case 2: sign = (ad ^ bd) & 0x8000000000000000ull; break;
                        default: return TRAP_ILLEGAL;
                    }
                    fset64(&cpu->f[rd], (ad & 0x7FFFFFFFFFFFFFFFull) | sign);
                }
                break;

            case F5_FMINMAX:
                /* funct3: 0=FMIN, 1=FMAX */
                if (fmt == 0) {
                    float32 res;
                    if (sf_cmp32(&sf, as, bs) <= 0) res = funct3 ? bs : as;
                    else                             res = funct3 ? as : bs;
                    fset32(&cpu->f[rd], res);
                } else {
                    float64 res;
                    if (sf_cmp64(&sf, ad, bd) <= 0) res = funct3 ? bd : ad;
                    else                             res = funct3 ? ad : bd;
                    fset64(&cpu->f[rd], res);
                }
                break;

            case F5_FCMP:
                /* funct3: 0=FLE, 1=FLT, 2=FEQ */
                {
                    int cmp = (fmt == 0) ? sf_cmp32(&sf, as, bs)
                                         : sf_cmp64(&sf, ad, bd);
                    int res = 0;
                    if (cmp == 2) { /* unordered — result always 0, NV already set by sf_cmp */
                        res = 0;
                    } else {
                        switch (funct3) {
                            case 0: res = (cmp <= 0); break; /* FLE */
                            case 1: res = (cmp <  0); break; /* FLT */
                            case 2: res = (cmp == 0); break; /* FEQ */
                            default: return TRAP_ILLEGAL;
                        }
                    }
                    wx(cpu, rd, (uint64_t)res);
                }
                break;

            case F5_FCVT_FF:
                /* rs2 field encodes direction: 0=to-S, 1=to-D */
                if (rs2 == 1) fset64(&cpu->f[rd], sf_f32_to_f64(&sf, as));  /* FCVT.D.S */
                else          fset32(&cpu->f[rd], sf_f64_to_f32(&sf, ad));  /* FCVT.S.D */
                break;

            case F5_FCVT_FI:
                /* float → integer. rs2 encodes width+sign:
                 * 0=W(i32), 1=WU(u32), 2=L(i64), 3=LU(u64) */
                if (fmt == 0) {
                    switch (rs2) {
                        case 0: wx(cpu, rd, (uint64_t)(int64_t)sf_f32_to_i32(&sf, as, rm, 1)); break;
                        case 1: wx(cpu, rd, (uint64_t)          sf_f32_to_u32(&sf, as, rm, 1)); break;
                        case 2: wx(cpu, rd, (uint64_t)          sf_f32_to_i64(&sf, as, rm, 1)); break;
                        case 3: wx(cpu, rd,                      sf_f32_to_u64(&sf, as, rm, 1)); break;
                        default: return TRAP_ILLEGAL;
                    }
                } else {
                    switch (rs2) {
                        case 0: wx(cpu, rd, (uint64_t)(int64_t)sf_f64_to_i32(&sf, ad, rm, 1)); break;
                        case 1: wx(cpu, rd, (uint64_t)          sf_f64_to_u32(&sf, ad, rm, 1)); break;
                        case 2: wx(cpu, rd, (uint64_t)          sf_f64_to_i64(&sf, ad, rm, 1)); break;
                        case 3: wx(cpu, rd,                      sf_f64_to_u64(&sf, ad, rm, 1)); break;
                        default: return TRAP_ILLEGAL;
                    }
                }
                break;

            case F5_FCVT_IF:
                /* integer → float. rs2: 0=W, 1=WU, 2=L, 3=LU */
                if (fmt == 0) {
                    switch (rs2) {
                        case 0: fset32(&cpu->f[rd], sf_i32_to_f32(&sf, (int32_t) rx(cpu, rs1))); break;
                        case 1: fset32(&cpu->f[rd], sf_u32_to_f32(&sf, (uint32_t)rx(cpu, rs1))); break;
                        case 2: fset32(&cpu->f[rd], sf_i64_to_f32(&sf, (int64_t) rx(cpu, rs1))); break;
                        case 3: fset32(&cpu->f[rd], sf_u64_to_f32(&sf,            rx(cpu, rs1))); break;
                        default: return TRAP_ILLEGAL;
                    }
                } else {
                    switch (rs2) {
                        case 0: fset64(&cpu->f[rd], sf_i32_to_f64(&sf, (int32_t) rx(cpu, rs1))); break;
                        case 1: fset64(&cpu->f[rd], sf_u32_to_f64(&sf, (uint32_t)rx(cpu, rs1))); break;
                        case 2: fset64(&cpu->f[rd], sf_i64_to_f64(&sf, (int64_t) rx(cpu, rs1))); break;
                        case 3: fset64(&cpu->f[rd], sf_u64_to_f64(&sf,            rx(cpu, rs1))); break;
                        default: return TRAP_ILLEGAL;
                    }
                }
                break;

            case F5_FMV_XF:
                /* FMV.X.W / FMV.X.D — move float bits to integer reg (no conversion) */
                if (fmt == 0) wx(cpu, rd, (uint64_t)(int64_t)(int32_t)fget32(cpu->f[rs1])); /* sign-ext */
                else          wx(cpu, rd, fget64(cpu->f[rs1]));
                break;

            case F5_FMV_FX:
                /* FMV.W.X / FMV.D.X — move integer bits to float reg */
                if (fmt == 0) fset32(&cpu->f[rd], (float32)rx(cpu, rs1));
                else          fset64(&cpu->f[rd], rx(cpu, rs1));
                break;

            default:
                return TRAP_ILLEGAL;
            }

            cpu_accum_flags(cpu, sf.flags);
            cpu->pc += 4;
        }
        break;

    /* ---------------------------------------------------------------- */
    /* SYSTEM  (ECALL, EBREAK, CSR)                                     */
    /* ---------------------------------------------------------------- */
    case OP_SYSTEM:
        if (funct3 == 0x0) {
            uint32_t imm12 = inst >> 20;
            if (imm12 == 0x000) {        /* ECALL  */
                return TRAP_ECALL;
            } else if (imm12 == 0x001) { /* EBREAK */
                return TRAP_EBREAK;
            }
            return TRAP_ILLEGAL;
        }
        /* CSR instructions: CSRRW=1, CSRRS=2, CSRRC=3, imm variants=5,6,7 */
        {
            uint32_t csr = inst >> 20;
            uint64_t old = 0, wval;
            int write = 1;

            /* only handle fcsr (0x003), fflags (0x001), frm (0x002) */
            switch (csr) {
                case 0x001: old = cpu->fcsr & FCSR_FFLAGS_MASK; break;
                case 0x002: old = cpu_frm(cpu);                  break;
                case 0x003: old = cpu->fcsr;                     break;
                default:    old = 0; write = 0;                  break; /* ignore unknown CSRs */
            }

            /* source value: rs1 field or zero-extended immediate */
            wval = (funct3 & 4) ? (uint64_t)rs1 : rx(cpu, rs1);

            if (rd != 0) wx(cpu, rd, old);  /* write old value to rd */

            if (write && rs1 != 0) {
                uint32_t newval = 0;
                switch (funct3 & 3) {
                    case 1: newval = (uint32_t)wval;          break; /* CSRRW */
                    case 2: newval = (uint32_t)(old |  wval); break; /* CSRRS */
                    case 3: newval = (uint32_t)(old & ~wval); break; /* CSRRC */
                }
                switch (csr) {
                    case 0x001: cpu->fcsr = (cpu->fcsr & ~FCSR_FFLAGS_MASK) | (newval & FCSR_FFLAGS_MASK); break;
                    case 0x002: cpu->fcsr = (cpu->fcsr & ~FCSR_FRM_MASK)    | ((newval & 0x7) << FCSR_FRM_SHIFT); break;
                    case 0x003: cpu->fcsr = newval & (FCSR_FFLAGS_MASK | FCSR_FRM_MASK); break;
                }
            }
            cpu->pc += 4;
        }
        break;

    default:
        return TRAP_ILLEGAL;
    }

    return TRAP_OK;
}
