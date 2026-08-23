#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t  *data;
    uint64_t  base;   /* guest physical address of data[0] */
    uint64_t  size;
} Mem;

int      mem_init(Mem *m, uint64_t base, uint64_t size);
void     mem_free(Mem *m);

/* Returns 0 on success, -1 on out-of-range */
int mem_read8 (Mem *m, uint64_t addr, uint8_t  *out);
int mem_read16(Mem *m, uint64_t addr, uint16_t *out);
int mem_read32(Mem *m, uint64_t addr, uint32_t *out);
int mem_read64(Mem *m, uint64_t addr, uint64_t *out);

int mem_write8 (Mem *m, uint64_t addr, uint8_t  val);
int mem_write16(Mem *m, uint64_t addr, uint16_t val);
int mem_write32(Mem *m, uint64_t addr, uint32_t val);
int mem_write64(Mem *m, uint64_t addr, uint64_t val);

/* Load a raw blob into memory at addr (e.g. from ELF segment) */
int mem_load(Mem *m, uint64_t addr, const uint8_t *src, size_t len);

#endif
