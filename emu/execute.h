#ifndef EXECUTE_H
#define EXECUTE_H

#include "cpu.h"
#include "mem.h"

/* Trap / halt codes returned by execute_one().
 * Positive values = keep running (shouldn't happen; execute_one runs one step).
 * 0              = ok, continue.
 * Negative values = fatal or halting condition. */
#define TRAP_OK          0
#define TRAP_EBREAK     -1
#define TRAP_IACCESS    -2   /* instruction fetch fault */
#define TRAP_DACCESS    -3   /* load/store fault */
#define TRAP_ILLEGAL    -4   /* illegal instruction */
#define TRAP_ECALL      -5   /* ECALL — caller handles syscall then resumes */
#define TRAP_HALT       -6   /* clean exit requested via syscall */

/* Execute a single instruction.  Returns one of the TRAP_ codes above.
 * On TRAP_ECALL: cpu->x[17] = syscall number, cpu->x[10..16] = args.
 * The caller is responsible for dispatching the syscall and advancing pc. */
int execute_one(RVCore *cpu, Mem *mem);

#endif
