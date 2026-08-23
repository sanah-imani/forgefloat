#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cpu.h"
#include "mem.h"
#include "execute.h"
#include "syscall.h"

#define MEM_BASE  0x80000000ull
#define MEM_SIZE  (8 * 1024 * 1024)   /* 8 MB */
#define STACK_TOP (MEM_BASE + MEM_SIZE - 4096)

extern void syscall_set_brk(uint64_t base);

static int load_binary(Mem *mem, const char *path, uint64_t *entry_out) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return -1; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || (uint64_t)size > mem->size) {
        fprintf(stderr, "binary too large (%ld bytes)\n", size);
        fclose(f);
        return -1;
    }

    uint8_t *buf = malloc(size);
    if (!buf) { fclose(f); return -1; }

    if ((long)fread(buf, 1, size, f) != size) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);

    if (mem_load(mem, MEM_BASE, buf, size)) {
        free(buf);
        fprintf(stderr, "failed to load binary into guest memory\n");
        return -1;
    }

    free(buf);
    *entry_out = MEM_BASE;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <binary>\n", argv[0]);
        return 1;
    }

    Mem mem;
    if (mem_init(&mem, MEM_BASE, MEM_SIZE)) {
        fprintf(stderr, "failed to allocate guest memory\n");
        return 1;
    }

    uint64_t entry = MEM_BASE;
    if (load_binary(&mem, argv[1], &entry)) {
        mem_free(&mem);
        return 1;
    }

    RVCore cpu;
    cpu_init(&cpu, entry);

    /* set up stack pointer (x2) near top of memory */
    cpu.x[2] = STACK_TOP;

    /* brk starts after the binary — leave 1 MB gap from top of mem */
    syscall_set_brk(MEM_BASE + MEM_SIZE / 2);

    /* ---------------------------------------------------------------- */
    /* fetch-decode-execute loop                                         */
    /* ---------------------------------------------------------------- */
    int max_steps = 100000000;  /* safety limit */
    for (int i = 0; i < max_steps; i++) {
        int trap = execute_one(&cpu, &mem);

        if (trap == TRAP_OK) continue;

        if (trap == TRAP_ECALL) {
            int r = handle_syscall(&cpu, &mem);
            if (r == TRAP_HALT) break;
            continue;
        }

        if (trap == TRAP_HALT)  break;
        if (trap == TRAP_EBREAK) {
            fprintf(stderr, "[ebreak] pc=0x%llx\n", (unsigned long long)cpu.pc);
            break;
        }

        fprintf(stderr, "trap %d at pc=0x%llx inst=0x%08x\n",
                trap, (unsigned long long)cpu.pc,
                0);  /* could re-fetch inst here */
        mem_free(&mem);
        return 1;
    }

    mem_free(&mem);
    return 0;
}
