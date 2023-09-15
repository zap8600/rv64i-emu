#ifndef CLINT_H
#define CLINT_H

#define CLINT_BASE 0x200_0000
#define CLINT_SIZE 0x10000

#define PLIC_BASE 0xc00_0000
#define PLIC_SIZE 0x4000000

typedef struct CLINT {
    uint64_t mtime;
    uint64_t mtimecmp;
} CLINT;

uint64_t clint_load(CLINT* clint, uint64_t addr, uint64_t size);
void clint_store(CLINT* clint, uint64_t addr, uint64_t size, uint64_t value);

#endif