#include "mem.h"
#include <stdlib.h>
#include <string.h>

int mem_init(Mem *m, uint64_t base, uint64_t size) {
    m->data = calloc(1, size);
    if (!m->data) return -1;
    m->base = base;
    m->size = size;
    return 0;
}

void mem_free(Mem *m) {
    free(m->data);
    m->data = NULL;
}

static int check(Mem *m, uint64_t addr, uint64_t width) {
    if (addr < m->base) return -1;
    if (addr - m->base + width > m->size) return -1;
    return 0;
}

int mem_read8(Mem *m, uint64_t addr, uint8_t *out) {
    if (check(m, addr, 1)) return -1;
    *out = m->data[addr - m->base];
    return 0;
}

int mem_read16(Mem *m, uint64_t addr, uint16_t *out) {
    if (check(m, addr, 2)) return -1;
    uint8_t *p = m->data + (addr - m->base);
    /* little-endian */
    *out = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    return 0;
}

int mem_read32(Mem *m, uint64_t addr, uint32_t *out) {
    if (check(m, addr, 4)) return -1;
    uint8_t *p = m->data + (addr - m->base);
    *out = (uint32_t)p[0]        | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return 0;
}

int mem_read64(Mem *m, uint64_t addr, uint64_t *out) {
    if (check(m, addr, 8)) return -1;
    uint8_t *p = m->data + (addr - m->base);
    *out = (uint64_t)p[0]        | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
    return 0;
}

int mem_write8(Mem *m, uint64_t addr, uint8_t val) {
    if (check(m, addr, 1)) return -1;
    m->data[addr - m->base] = val;
    return 0;
}

int mem_write16(Mem *m, uint64_t addr, uint16_t val) {
    if (check(m, addr, 2)) return -1;
    uint8_t *p = m->data + (addr - m->base);
    p[0] = val & 0xFF;
    p[1] = (val >> 8) & 0xFF;
    return 0;
}

int mem_write32(Mem *m, uint64_t addr, uint32_t val) {
    if (check(m, addr, 4)) return -1;
    uint8_t *p = m->data + (addr - m->base);
    p[0] = val & 0xFF;
    p[1] = (val >> 8)  & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
    return 0;
}

int mem_write64(Mem *m, uint64_t addr, uint64_t val) {
    if (check(m, addr, 8)) return -1;
    uint8_t *p = m->data + (addr - m->base);
    p[0] = val & 0xFF;
    p[1] = (val >> 8)  & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
    p[4] = (val >> 32) & 0xFF;
    p[5] = (val >> 40) & 0xFF;
    p[6] = (val >> 48) & 0xFF;
    p[7] = (val >> 56) & 0xFF;
    return 0;
}

int mem_load(Mem *m, uint64_t addr, const uint8_t *src, size_t len) {
    if (check(m, addr, len)) return -1;
    memcpy(m->data + (addr - m->base), src, len);
    return 0;
}
