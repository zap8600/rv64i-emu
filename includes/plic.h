#ifndef PLIC_H
#define PLIC_H

#define PLIC_BASE 0xc000000
#define PLIC_SIZE 0x4000000

typedef struct PLIC {
    uint64_t pending;
    uint64_t senable;
    uint64_t spriority;
    uint64_t sclaim;
} PLIC;

void plic_init(PLIC* plic);
uint64_t plic_load(PLIC* plic, uint64_t addr, uint64_t size);
void plic_store(PLIC* plic, uint64_t addr, uint64_t size, uint64_t value);

#endif
