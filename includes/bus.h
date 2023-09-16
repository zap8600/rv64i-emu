#ifndef BUS_H
#define BUS_H

#include "dram.h"
#include "plic.h"
#include "clint.h"
#include "uart.h"

typedef struct BUS {
    struct CLINT clint;
    struct PLIC plic;
    struct UART uart;
    struct DRAM dram;
} BUS;

void bus_init(BUS* bus);
uint64_t bus_load(BUS* bus, uint64_t addr, uint64_t size);
void bus_store(BUS* bus, uint64_t addr, uint64_t size, uint64_t value);

#endif
