#ifndef CPU_H
#define CPU_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "bus.h"

#define CPU_PAGE_SIZE 4069

typedef enum AccessType {
    Instruction,
    Load,
    Store,
} AccessType;

typedef enum Mode {
    User = 0b00,
    Supervisor = 0b01,
    Machine = 0b11,
} Mode;

typedef enum Exception {
    InstructionAddressMisaligned = 0,
    InstructionAccessFault = 1,
    IllegalInstruction = 2,
    Breakpoint = 3,
    LoadAddressMisaligned = 4,
    LoadAccessFault = 5,
    StoreAMOAddressMisaligned = 6,
    StoreAMOAccessFault = 7,
    EnvironmentCallFromUMode = 8,
    EnvironmentCallFromSMode = 9,
    EnvironmentCallFromMMode = 11,
    InstructionPageFault = 12,
    LoadPageFault = 13,
    StoreAMOPageFault = 15,
} Exception;

typedef enum Interrupt {
    None = -1,
    UserSoftwareInterrupt = 0,
    SupervisorSoftwareInterrupt = 1,
    MachineSoftwareInterrupt = 3,
    UserTimerInterrupt = 4,
    SupervisorTimerInterrupt = 5,
    MachineTimerInterrupt = 7,
    UserExternalInterrupt = 8,
    SupervisorExternalInterrupt = 9,
    MachineExternalInterrupt = 11,
} Interrupt;

typedef struct CPU {
    uint64_t regs[32];
    uint64_t pc;
    Mode mode;
    uint64_t csr[4069];
    struct BUS bus;
    uint64_t trap;
    Interrupt intr;
    bool enable_paging;
    uint64_t page_table;
    FILE* debug_log;
} CPU;

void cpu_init(struct CPU *cpu);
uint32_t cpu_fetch(struct CPU *cpu);
void cpu_check_interrupt(CPU* cpu);
int cpu_execute(struct CPU *cpu, uint32_t inst);
void dump_registers(struct CPU *cpu);
void dump_csr(struct CPU *cpu);
void take_trap(CPU* cpu, bool interrupting);
bool is_fatal(CPU* cpu);
void virtio_disk_access(CPU* cpu);

#endif
