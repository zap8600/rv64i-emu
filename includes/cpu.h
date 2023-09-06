#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "bus.h"

/*
typedef enum Mode {
    User = 0b00,
    Supervisor = 0b01,
    Machine = 0b11,
} Mode;
*/

typedef struct CPU {
    uint64_t regs[32];
    uint64_t pc;
    //Mode mode;
    uint64_t csr[4069];
    struct BUS bus;
} CPU;

void cpu_init(struct CPU *cpu);
uint32_t cpu_fetch(struct CPU *cpu);
int cpu_execute(struct CPU *cpu, uint32_t inst);
void dump_registers(struct CPU *cpu); 

#endif
