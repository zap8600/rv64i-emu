#ifndef CLINT_H
#define CLINT_H

#define CLINT_BASE 0x2000000
#define CLINT_SIZE 0x10000

typedef struct CLINT {
    uint64_t mtime;
    uint64_t mtimecmp;
} CLINT;

void clint_init(CLINT* clint);
uint64_t clint_load(CLINT* clint, uint64_t addr, uint64_t size);
void clint_store(CLINT* clint, uint64_t addr, uint64_t size, uint64_t value);

#endif
