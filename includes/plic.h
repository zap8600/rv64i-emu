#ifndef PLIC_H
#define PLIC_H

#define PLIC_BASE 0xc000000
#define PLIC_SIZE 0x4000000

#define PLIC_PENDING (PLIC_BASE + 0x1000)
#define PLIC_SENABLE (PLIC_BASE + 0X2000)
#define PLIC_SPRIORITY (PLIC_BASE + 0x201000)
#define PLIC_SCLAIM (PLIC_BASE + 0x201004)

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
