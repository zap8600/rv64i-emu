#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "bus.h"

typedef struct CPU {
    uint64_t regs[32];
    uint64_t pc;
    uint64_t csr[4069];
    struct BUS bus;
} CPU;

void cpu_init(struct CPU *cpu);
uint32_t cpu_fetch(struct CPU *cpu);
int cpu_execute(struct CPU *cpu, uint32_t inst);
void dump_registers(struct CPU *cpu); 

#endif
