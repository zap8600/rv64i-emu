#include <stdint.h>
#include <stdio.h>
#include "../includes/clint.h"

#define CLINT_MTIMECMP (CLINT_BASE + 0x4000)
#define CLINT_MTIME (CLINT_BASE + 0xbff8)

void clint_init(CLINT* clint) {
    clint->mtime = 0;
    clint->mtimecmp = 0;
}

uint64_t clint_load_64(CLINT* clint, uint64_t addr) {
    switch(addr) {
        case CLINT_MTIMECMP: return clint->mtimecmp; break;
        case CLINT_MTIME: return clint->mtime; break;
        default: ;
    }
    return 1;
}

uint64_t clint_load(CLINT* clint, uint64_t addr, uint64_t size) {
    switch (size) {
        case 64: return clint_load_64(clint, addr); break;
        default: ;
    }
    return 1;
}

void clint_store_64(CLINT* clint, uint64_t addr, uint64_t value) {
    switch (addr) {
        case CLINT_MTIMECMP: clint->mtimecmp = value; break;
        case CLINT_MTIME: clint->mtime = value; break;
        default: ;
    }
}

void clint_store(CLINT* clint, uint64_t addr, uint64_t size, uint64_t value) {
    switch (size) {
        case 64: clint_store_64(clint, addr, value); break;
        default: ;
    }
}
