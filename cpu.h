#include <stdint.h>

#include "bus.h"

typedef struct CPU {
    uint64_t regs[32];
    uint64_t pc;
    struct BUS bus;
} CPU;

void cpu_init(CPU *cpu) {
    cpu->regs[0] = 0x00;
    cpu->regs[2] = DRAM_SIZE;
    cpu->pc = DRAM_BASE;
}

uint32_t cpu_fetch(CPU *cpu) {
    uint32_t inst = bus_load(&(cpu->bus), cpu->pc, 32);
    return inst;
}

void exec_ADDI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] + (int64_t) imm;
}

void exec_ADD(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] =
        (uint64_t) ((int64_t)cpu->regs[rs1(inst)] + (int64_t)cpu->regs[rs2(inst)]);
}

int cpu_execute(CPU *cpu, uint32_t inst) {
    int opcode = inst & 0x7f;
    int rd = (inst >> 7) & 0x1f;
    int rs1 = (inst >> 15) & 0x1f;
    int rs2 = (inst >> 20) & 0x1f;

    cpu->regs[0] = 0;

    switch (opcode) {
        case 0x13: exec_ADDI(cpu, inst); break;
        case 0x33: exec_ADD(cpu, inst); break;
    }
}
