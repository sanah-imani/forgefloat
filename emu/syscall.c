#include "syscall.h"
#include "execute.h"
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* brk heap — grows up from end of loaded binary */
static uint64_t brk_base = 0;
static uint64_t brk_cur  = 0;

void syscall_set_brk(uint64_t base) {
    brk_base = base;
    brk_cur  = base;
}

int handle_syscall(RVCore *cpu, Mem *mem) {
    uint64_t num = rx(cpu, 17);  /* a7 */
    uint64_t a0  = rx(cpu, 10);
    uint64_t a1  = rx(cpu, 11);
    uint64_t a2  = rx(cpu, 12);
    int64_t  ret = -1;

    switch (num) {

    case SYS_EXIT:
    case SYS_EXIT_GRP:
        /* a0 = exit code */
        return TRAP_HALT;

    case SYS_WRITE:
        /* a0=fd, a1=buf_addr, a2=count */
        {
            if (a2 == 0) { ret = 0; break; }
            /* copy from guest memory into a host buffer and write */
            uint64_t count = a2 > 4096 ? 4096 : a2;
            uint8_t  buf[4096];
            uint64_t i;
            for (i = 0; i < count; i++) {
                uint8_t b;
                if (mem_read8(mem, a1 + i, &b)) { ret = -1; goto done; }
                buf[i] = b;
            }
            ret = (int64_t)write((int)a0, buf, (size_t)count);
        }
        break;

    case SYS_READ:
        /* a0=fd, a1=buf_addr, a2=count */
        {
            if (a2 == 0) { ret = 0; break; }
            uint64_t count = a2 > 4096 ? 4096 : a2;
            uint8_t  buf[4096];
            ssize_t  n = read((int)a0, buf, (size_t)count);
            if (n < 0) { ret = -1; break; }
            uint64_t i;
            for (i = 0; i < (uint64_t)n; i++) {
                if (mem_write8(mem, a1 + i, buf[i])) { ret = -1; goto done; }
            }
            ret = n;
        }
        break;

    case SYS_BRK:
        /* a0 = new brk address (0 = query current) */
        if (a0 == 0 || a0 < brk_base) {
            ret = (int64_t)brk_cur;
        } else if (a0 <= mem->base + mem->size) {
            /* zero the newly allocated region */
            if (a0 > brk_cur) {
                uint64_t i;
                for (i = brk_cur; i < a0; i++)
                    mem_write8(mem, i, 0);
            }
            brk_cur = a0;
            ret = (int64_t)brk_cur;
        } else {
            ret = -1;  /* out of memory */
        }
        break;

    default:
        /* unknown syscall — return -1 */
        ret = -1;
        break;
    }

done:
    wx(cpu, 10, (uint64_t)ret);  /* return value in a0 */
    cpu->pc += 4;                /* advance past ECALL */
    return TRAP_OK;
}
