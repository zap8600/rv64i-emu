#include <stdio.h>
#include <stdint.h>
#include "./includes/plic.h"

#define PLIC_PENDING (PLIC_BASE + 0x1000)
#define PLIC_SENABLE (PLIC_BASE + 0X2000)
#define PLIC_SPRIORITY (PLIC_BASE + 0x201000)
#define PLIC_SCLAIM (PLIC_BASE + 0x201004)

void plic_init(PLIC* plic) {
    plic->pending = 0;
    plic->senable = 0;
    plic->spriority = 0;
    plic->sclaim = 0;
}

uint64_t plic_load_32(PLIC* plic, uint64_t addr) {
    switch(addr) {
        case PLIC_PENDING: return plic->pending; break;
        case PLIC_SENABLE: return plic->senable; break;
        case PLIC_SPRIORITY: return plic->spriority; break;
        case PLIC_SCLAIM: return plic->sclaim; break;
        default: ;
    }
    return 1;
}

uint64_t plic_load(PLIC* plic, uint64_t addr, uint64_t size) {
    switch (size) {
        case 32: return plic_load_32(plic, addr); break;
        default: ;
    }
    return 1;
}

void plic_store_32(PLIC* plic, uint64_t addr, uint64_t value) {
    switch (addr) {
        case PLIC_PENDING: plic->pending = value; break;
        case PLIC_SENABLE: plic->senable = value; break;
        case PLIC_SPRIORITY: plic->spriority = value; break;
        case PLIC_SCLAIM: plic->sclaim = value; break;
        default: ;
    }
}

void plic_store(PLIC* plic, uint64_t addr, uint64_t size, uint64_t value) {
    switch (size) {
        case 32: plic_store_32(plic, addr, value); break;
        default: ;
    }
}
