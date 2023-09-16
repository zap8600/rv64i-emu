#include "./includes/bus.h"

void bus_init(BUS* bus) {
    plic_init(&(bus->plic));
    clint_init(&(bus->clint));
}

uint64_t bus_load(BUS* bus, uint64_t addr, uint64_t size) {
    if (CLINT_BASE <= addr && addr < (CLINT_BASE + CLINT_SIZE)) {
        return clint_load(&(bus->clint), addr, size);
    }
    if (PLIC_BASE <= addr && addr < (PLIC_BASE + PLIC_SIZE)) {
        return plic_load(&(bus->plic), addr, size);
    }
    if (DRAM_BASE <= addr) {
        return dram_load(&(bus->dram), addr, size);
    }
    return 1;
}
void bus_store(BUS* bus, uint64_t addr, uint64_t size, uint64_t value) {
    if (CLINT_BASE <= addr && addr < (CLINT_BASE + CLINT_SIZE)) {
        clint_store(&(bus->clint), addr, size, value);
    }
    if (PLIC_BASE <= addr && addr < (PLIC_BASE + PLIC_SIZE)) {
        plic_store(&(bus->plic), addr, size, value);
    }
    if (DRAM_BASE <= addr) {
        dram_store(&(bus->dram), addr, size, value);
    }
}
