#include "../includes/bus.h"
#include <stdio.h>

void bus_init(BUS* bus) {
    dram_init(&(bus->dram));
    plic_init(&(bus->plic));
    clint_init(&(bus->clint));
    uart_init(&(bus->uart));
    virtio_init(&(bus->virtio));
}

uint64_t bus_load(BUS* bus, uint64_t addr, uint64_t size) {
    if (CLINT_BASE <= addr && addr < (CLINT_BASE + CLINT_SIZE)) {
        return clint_load(&(bus->clint), addr, size);
    } else if (PLIC_BASE <= addr && addr < (PLIC_BASE + PLIC_SIZE)) {
        return plic_load(&(bus->plic), addr, size);
    } else if (UART_BASE <= addr && addr < (UART_BASE + UART_SIZE)) {
        return uart_load(&(bus->uart), addr, size);
    } else if (VIRTIO_BASE <= addr && addr < (VIRTIO_BASE + VIRTIO_SIZE)) {
        return virtio_load(&(bus->virtio), addr, size);
    } else if (DRAM_BASE <= addr && addr < (DRAM_BASE + DRAM_SIZE)) {
        return dram_load(&(bus->dram), addr, size);
    }
    fprintf(stderr, "Load Access Fault!\n");
    return 1;
}
void bus_store(BUS* bus, uint64_t addr, uint64_t size, uint64_t value) {
    if (CLINT_BASE <= addr && addr < (CLINT_BASE + CLINT_SIZE)) {
        clint_store(&(bus->clint), addr, size, value);
    } else if (PLIC_BASE <= addr && addr < (PLIC_BASE + PLIC_SIZE)) {
        plic_store(&(bus->plic), addr, size, value);
    } else if (UART_BASE <= addr && addr < (UART_BASE + UART_SIZE)) {
        uart_store(&(bus->uart), addr, size, value);
    } else if (VIRTIO_BASE <= addr && addr < (VIRTIO_BASE + VIRTIO_SIZE)) {
        virtio_store(&(bus->virtio), addr, size, value);
    } else if (DRAM_BASE <= addr) {
        dram_store(&(bus->dram), addr, size, value);
    } else {
        fprintf(stderr, "Store AMO Access Fault!\n");
    }
}
