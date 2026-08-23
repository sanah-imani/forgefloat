#ifndef SYSCALL_H
#define SYSCALL_H

#include "cpu.h"
#include "mem.h"

/* Linux RISC-V syscall numbers (a7 register) */
#define SYS_EXIT    93
#define SYS_EXIT_GRP 94
#define SYS_READ    63
#define SYS_WRITE   64
#define SYS_BRK     214

/* Handle an ECALL from the emulated hart.
 * cpu->x[17] = syscall number
 * cpu->x[10..16] = a0..a6 (arguments)
 * Returns TRAP_OK to continue, TRAP_HALT to stop the emulator. */
int handle_syscall(RVCore *cpu, Mem *mem);

#endif
