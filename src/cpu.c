#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../includes/cpu.h"
#include "../includes/opcodes.h"
#include "../includes/csr.h"
#include "../includes/uart.h"
#include "../includes/virtio.h"

#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[31m"
#define ANSI_RESET   "\x1b[0m"

#define ADDR_MISALIGNED(addr) (addr & 0x3)

// print operation for DEBUG
void print_op(char* s, CPU* cpu) {
    /*
    switch (cpu->pc-4) {
        case 0x80001180: break;
        case 0x80001184: break;
        case 0x80001188: break;
        case 0x8000118c: break;
        default: fprintf(cpu->debug_log, "%s", s); break;
    }
    */
    fprintf(cpu->debug_log, "%s", s);
}

void cpu_init(CPU *cpu) {
    cpu->regs[0] = 0x00;                    // register x0 hardwired to 0
    cpu->regs[2] = DRAM_BASE + DRAM_SIZE;   // Set stack pointer
    cpu->pc      = DRAM_BASE;               // Set program counter to the base address
    cpu->mode    = Machine;
    cpu->enable_paging = false;
    cpu->page_table = 0;
    bus_init(&(cpu->bus));
}

void cpu_check_interrupt(CPU* cpu) {
    switch (cpu->mode) {
        case Machine:
            if (((csr_read(cpu, MSTATUS) & 1) >> 3) == 0) {
                cpu->intr = -1;
                return;
            }
            break;
        case Supervisor:
            if(((csr_read(cpu, SSTATUS) & 1) >> 1) == 0) {
                cpu->intr = -1;
                return;
            }
            break;
        default: ; break;
    }

    uint64_t irq;
    if (uart_interrupting(&(cpu->bus.uart))) {
        irq = UART_IRQ;
    } else if (virtio_interrupting(&(cpu->bus.virtio))) {
        virtio_disk_access(cpu);
        irq = VIRTIO_IRQ;
    } else {
        irq = 0;
    }

    if (irq != 0) {
        bus_store(&(cpu->bus), PLIC_SCLAIM, 32, irq);
        csr_write(cpu, MIP, csr_read(cpu, MIP) | MIP_SEIP);
    }

    uint64_t pending = csr_read(cpu, MIE) & csr_read(cpu, MIP);
    if ((pending & MIP_MEIP) != 0) {
        csr_write(cpu, MIP, (csr_read(cpu, MIP) & 1) << 11);
        cpu->intr = MachineExternalInterrupt;
        return;
    }
    if ((pending & MIP_MSIP) != 0) {
        csr_write(cpu, MIP, (csr_read(cpu, MIP) & 1) << 3);
        cpu->intr = MachineSoftwareInterrupt;
        return;
    }
    if ((pending & MIP_MTIP) != 0) {
        csr_write(cpu, MIP, (csr_read(cpu, MIP) & 1) << 7);
        cpu->intr = MachineTimerInterrupt;
        return;
    }
    if ((pending & MIP_SEIP) != 0) {
        csr_write(cpu, MIP, (csr_read(cpu, MIP) & 1) << 9);
        cpu->intr = SupervisorExternalInterrupt;
        return;
    }
    if ((pending & MIP_SSIP) != 0) {
        csr_write(cpu, MIP, (csr_read(cpu, MIP) & 1) << 1);
        cpu->intr = SupervisorSoftwareInterrupt;
        return;
    }
    if ((pending & MIP_STIP) != 0) {
        csr_write(cpu, MIP, (csr_read(cpu, MIP) & 1) << 5);
        cpu->intr = SupervisorTimerInterrupt;
        return;
    }
    cpu->intr = -1;
    return;
}

void cpu_update_paging(CPU* cpu, uint64_t csr_addr) {
    if (csr_addr != SATP) {
        return;
    }
    //printf("update paging\n");

    cpu->page_table = (csr_read(cpu, SATP) & ((1ULL << 44) - 1)) * CPU_PAGE_SIZE;

    uint64_t mode = (csr_read(cpu, SATP)) >> 60;

    if (mode == 8) {
        //printf("paging on\n");
        cpu->enable_paging = true;
    } else {
        //printf("paging off\n");
        cpu->enable_paging = false;
    }
}

uint64_t cpu_translate(CPU* cpu, uint64_t addr, AccessType access_type) {
    if (!(cpu->enable_paging)) {
        return addr;
    }

    int64_t levels = 3;
    uint64_t vpn[3] = {((addr >> 12) & 0x1ff), ((addr >> 21) & 0x1ff), ((addr >> 30) & 0x1ff)};

    uint64_t a = cpu->page_table;
    int64_t i = levels - 1;
    uint64_t pte;
    while (1) {
        pte = bus_load(&(cpu->bus), a + vpn[i] * 8, 64);

        uint64_t v = pte & 1;
        uint64_t r = (pte & 1) >> 1;
        uint64_t w = (pte & 1) >> 2;
        uint64_t x = (pte & 1) >> 3;
        if (v == 0 || r == 0 && w == 1) {
            switch (access_type) {
                case Instruction: cpu->trap = InstructionPageFault; return InstructionPageFault;
                case Load: cpu->trap = LoadPageFault; return LoadPageFault;
                case Store: cpu->trap = StoreAMOPageFault; return StoreAMOPageFault;
            } break;
        }

        if (r == 1 || x == 1) {
            break;
        }
        i -= 1;
        uint64_t ppn = (pte & 0x0fffffffffff) >> 10;
        a = ppn * CPU_PAGE_SIZE;
        if (i < 0) {
            switch (access_type) {
                case Instruction: cpu->trap = InstructionPageFault; return InstructionPageFault;
                case Load: cpu->trap = LoadPageFault; return LoadPageFault;
                case Store: cpu->trap = StoreAMOPageFault; return StoreAMOPageFault;
            } break;
        }
    }

    uint64_t ppna[3] = {((pte >> 10) & 0x1ff), ((pte >> 19) & 0x1ff), ((pte >> 28) & 0x03ffffff)};
    uint64_t ppn;

    uint64_t offset = addr & 0xfff;
    switch (i) {
        case 0:
            ppn = (pte & 0x0fffffffffff) >> 10;
            fprintf(stderr, "translate= case=0 return=%lx\n", (ppn << 12) | offset);
            return (ppn << 12) | offset;
            break;
        case 1:
            fprintf(stderr, "translate= case=1 return=%lx\n", (ppna[2] << 30) | (ppna[1] << 21) | (vpn[0] << 12) | offset);
            return (ppna[2] << 30) | (ppna[1] << 21) | (vpn[0] << 12) | offset;
            break;
        case 2:
            fprintf(stderr, "translate= case=2 return=%lx\n", (ppna[2] << 30) | (vpn[1] << 21) | (vpn[0] << 12) | offset);
            return (ppna[2] << 30) | (vpn[1] << 21) | (vpn[0] << 12) | offset;
            break;
        default:
            fprintf(stderr, "mmu error\n");
            switch (access_type) {
                case Instruction: cpu->trap = InstructionPageFault; return InstructionPageFault;
                case Load: cpu->trap = LoadPageFault; return LoadPageFault;
                case Store: cpu->trap = StoreAMOPageFault; return StoreAMOPageFault;
            } break;
    }
}

uint64_t cpu_load(CPU* cpu, uint64_t addr, uint64_t size) {
    uint64_t p_addr = cpu_translate(cpu, addr, Load);
    /*
    switch (cpu->pc-4) {
        case 0x80001180: break;
        case 0x80001184: break;
        case 0x80001188: break;
        case 0x8000118c: break;
        default: fprintf(cpu->debug_log, "load=%lx %lx %lx\n", p_addr, size, bus_load(&(cpu->bus), p_addr, size));
    }
    */
    //fprintf(cpu->debug_log, "load=%lx %lx %lx\n", p_addr, size, bus_load(&(cpu->bus), p_addr, size));
    return bus_load(&(cpu->bus), p_addr, size);
}

void cpu_store(CPU* cpu, uint64_t addr, uint64_t size, uint64_t value) {
    uint64_t p_addr = cpu_translate(cpu, addr, Store);
    /*
    switch (cpu->pc-4) {
        case 0x80001180: break;
        case 0x80001184: break;
        case 0x80001188: break;
        case 0x8000118c: break;
        default: fprintf(cpu->debug_log, "store=%#-13.2lx %#-13.2lx %#-13.2lx\n", p_addr, size, value);
    }
    */
    //fprintf(cpu->debug_log, "store=%#-13.2lx %#-13.2lx %#-13.2lx\n", p_addr, size, value);
    bus_store(&(cpu->bus), p_addr, size, value);
}

uint32_t cpu_fetch(CPU *cpu) {
    uint64_t p_pc = cpu_translate(cpu, cpu->pc, Instruction);
    uint32_t inst = bus_load(&(cpu->bus), p_pc, 32);
    return inst;
}

//=====================================================================================
// Instruction Decoder Functions
//=====================================================================================

uint64_t rd(uint32_t inst) {
    return (inst >> 7) & 0x1f;    // rd in bits 11..7
}
uint64_t rs1(uint32_t inst) {
    return (inst >> 15) & 0x1f;   // rs1 in bits 19..15
}
uint64_t rs2(uint32_t inst) {
    return (inst >> 20) & 0x1f;   // rs2 in bits 24..20
}

uint64_t imm_I(uint32_t inst) {
    // imm[11:0] = inst[31:20]
    return (int64_t)((int32_t) (inst) >> 20); // right shift as signed?
}
uint64_t imm_S(uint32_t inst) {
    // imm[11:5] = inst[31:25], imm[4:0] = inst[11:7]
    return ((int64_t)(int32_t)(inst & 0xfe000000) >> 20)
        | ((inst >> 7) & 0x1f); 
}
uint64_t imm_B(uint32_t inst) {
    // imm[12|10:5|4:1|11] = inst[31|30:25|11:8|7]
    return ((int64_t)(int32_t)(inst & 0x80000000) >> 19)
        | ((inst & 0x80) << 4) // imm[11]
        | ((inst >> 20) & 0x7e0) // imm[10:5]
        | ((inst >> 7) & 0x1e); // imm[4:1]
}
uint64_t imm_U(uint32_t inst) {
    // imm[31:12] = inst[31:12]
    return (uint64_t)(int64_t)(int32_t)(inst & 0xfffff000);
}
uint64_t imm_J(uint32_t inst) {
    // imm[20|10:1|11|19:12] = inst[31|30:21|20|19:12]
    return (uint64_t)((int64_t)(int32_t)(inst & 0x80000000) >> 11)
        | (inst & 0xff000) // imm[19:12]
        | ((inst >> 9) & 0x800) // imm[11]
        | ((inst >> 20) & 0x7fe); // imm[10:1]
}

uint32_t shamt(uint32_t inst) {
    // shamt(shift amount) only required for immediate shift instructions
    // shamt[4:5] = imm[5:0]
    return (uint32_t) (rs2(inst) & 0x3f); // TODO: 0x1f / 0x3f ?
}

uint32_t shamt_W(uint32_t inst) {
    // shamt(shift amount) only required for immediate shift instructions
    // shamt[4:5] = imm[5:0]
    return (uint32_t) (rs2(inst) & 0x3f); // TODO: 0x1f / 0x3f ?
}

uint32_t shamt_I(uint32_t inst) {
    // shamt(shift amount) only required for immediate shift instructions
    // shamt[4:5] = imm[5:0]
    uint64_t imm = ((int64_t)(int32_t) (inst & 0xfff00000)) >> 20;
    return (uint32_t) (imm & 0x3f); // TODO: 0x1f / 0x3f ?
}

uint32_t shamt_IW(uint32_t inst) {
    // shamt(shift amount) only required for immediate shift instructions
    // shamt[4:5] = imm[5:0]
    return (uint32_t) (imm_I(inst) & 0x1f); // TODO: 0x1f / 0x3f ?
}

uint64_t csr(uint32_t inst) {
    // csr[11:0] = inst[31:20]
    return ((inst & 0xfff00000) >> 20);
}

//=====================================================================================
//   Instruction Execution Functions
//=====================================================================================

void exec_LUI(CPU* cpu, uint32_t inst) {
    // LUI places upper 20 bits of U-immediate value to rd
    uint64_t imm = imm_U(inst);
    cpu->regs[rd(inst)] = imm; // cpu->regs[rd(inst)] = (uint64_t)(int64_t)(int32_t)(inst & 0xfffff000);
    //print_op("lui\n", cpu);
}

void exec_AUIPC(CPU* cpu, uint32_t inst) {
    // AUIPC forms a 32-bit offset from the 20 upper bits 
    // of the U-immediate
    uint64_t imm = imm_U(inst);
    cpu->regs[rd(inst)] = (cpu->pc + imm) - 4;
    //print_op("auipc\n", cpu);
    //printf("=%#-13.2lx\n", (cpu->pc + imm) - 4);
}

void exec_JAL(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_J(inst);
    cpu->regs[rd(inst)] = cpu->pc;
    /*//print_op("JAL-> rd:%ld, pc:%lx\n", rd(inst), cpu->pc);*/
    cpu->pc = cpu->pc + (int64_t) imm - 4;
    //print_op("jal\n", cpu);
    if (ADDR_MISALIGNED(cpu->pc)) {
        fprintf(stderr, "JAL pc address misalligned");
        exit(0);
    }
}

void exec_JALR(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    uint64_t tmp = cpu->pc;
    cpu->pc = (cpu->regs[rs1(inst)] + (int64_t) imm) & 0xfffffffe;
    cpu->regs[rd(inst)] = tmp;
    //print_op("jalr\n", cpu);
    /*//print_op("NEXT -> %#lx, imm:%#lx\n", cpu->pc, imm);*/
    if (ADDR_MISALIGNED(cpu->pc)) {
        fprintf(stderr, "JAL pc address misalligned");
        exit(0);
    }
}

void exec_BEQ(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] == (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
    //print_op("beq\n", cpu);
    //printf("=%#-13.2lx %#-13.2lx\n", cpu->regs[rs1(inst)], cpu->regs[rs2(inst)]);
}
void exec_BNE(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] != (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = (cpu->pc + (int64_t) imm - 4);
    //print_op("bne\n", cpu);
}
void exec_BLT(CPU* cpu, uint32_t inst) {
    /*//print_op("Operation: BLT\n");*/
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] < (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
    //print_op("blt\n", cpu);
}
void exec_BGE(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] >= (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
    //print_op("bge\n", cpu);
}
void exec_BLTU(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if (cpu->regs[rs1(inst)] < cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
    //print_op("bltu\n", cpu);
}
void exec_BGEU(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if (cpu->regs[rs1(inst)] >= cpu->regs[rs2(inst)])
        cpu->pc = (int64_t) cpu->pc + (int64_t) imm - 4;
    //print_op("jal\n", cpu);
}
void exec_LB(CPU* cpu, uint32_t inst) {
    // load 1 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t)(int8_t) cpu_load(cpu, addr, 8);
    //print_op("lb\n", cpu);
}
void exec_LH(CPU* cpu, uint32_t inst) {
    // load 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t)(int16_t) cpu_load(cpu, addr, 16);
    //print_op("lh\n", cpu);
}
void exec_LW(CPU* cpu, uint32_t inst) {
    // load 4 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t)(int32_t) cpu_load(cpu, addr, 32);
    //print_op("lw\n", cpu);
}
void exec_LD(CPU* cpu, uint32_t inst) {
    // load 8 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t) cpu_load(cpu, addr, 64);
    //print_op("ld\n", cpu);
}
void exec_LBU(CPU* cpu, uint32_t inst) {
    // load unsigned 1 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 8);
    //print_op("lbu\n", cpu);
}
void exec_LHU(CPU* cpu, uint32_t inst) {
    // load unsigned 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 16);
    //print_op("lhu\n", cpu);
}
void exec_LWU(CPU* cpu, uint32_t inst) {
    // load unsigned 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 32);
    //print_op("lwu\n", cpu);
}
void exec_SB(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 8, cpu->regs[rs2(inst)]);
    //print_op("sb\n", cpu);
}
void exec_SH(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 16, cpu->regs[rs2(inst)]);
    //print_op("sh\n", cpu);
}
void exec_SW(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 32, cpu->regs[rs2(inst)]);
    //print_op("sw\n", cpu);
    //printf("=%#-13.2lx %#-13.2lx\n", addr, cpu->regs[rs2(inst)]);
}
void exec_SD(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 64, cpu->regs[rs2(inst)]);
    //print_op("sd\n", cpu);
}

void exec_ADDI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] + (int64_t) imm;
    //print_op("addi\n", cpu);
}

void exec_SLLI(CPU* cpu, uint32_t inst) {
    //fprintf(cpu->debug_log, "pc=%#-13.2lx  slli=%#-13.2lx << %#-13.2x = %#-13.2lx\n", cpu->pc-4, cpu->regs[rs1(inst)], shamt_I(inst), cpu->regs[rs1(inst)] << shamt(inst));
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] << shamt_I(inst);
    //print_op("slli\n", cpu);
}

void exec_SLTI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    if (((int64_t)cpu->regs[rs1(inst)]) < ((int64_t)imm)) {
        cpu->regs[rd(inst)] = 1;
    } else {
        cpu->regs[rd(inst)] = 0;
    }
    //print_op("slti\n", cpu);
}

void exec_SLTIU(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    if (cpu->regs[rs1(inst)] < imm) {
        cpu->regs[rd(inst)] = 1;
    } else {
        cpu->regs[rd(inst)] = 0;
    }
    //print_op("sltiu\n", cpu);
}

void exec_XORI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] ^ imm;
    //print_op("xori\n", cpu);
}

void exec_SRLI(CPU* cpu, uint32_t inst) {
    //fprintf(cpu->debug_log, "pc=%#-13.2lx  srli=%#-13.2lx >> %#-13.2x = %#-13.2lx\n", cpu->pc-4, cpu->regs[rs1(inst)], shamt_I(inst), cpu->regs[rs1(inst)] >> shamt(inst));
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] >> shamt_I(inst);
    //print_op("srli\n", cpu);
}

void exec_SRAI(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (uint64_t)(((int64_t)cpu->regs[rs1(inst)]) >> shamt_I(inst));
    //print_op("srai\n", cpu);
}

void exec_ORI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] | imm;
    //print_op("ori\n", cpu);
}

void exec_ANDI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] & imm;
    //print_op("andi\n", cpu);
}

void exec_ADD(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] =
        (uint64_t) ((int64_t)cpu->regs[rs1(inst)] + (int64_t)cpu->regs[rs2(inst)]);
    //print_op("add\n", cpu);
}

void exec_MUL(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] =
        (uint64_t) ((int64_t)cpu->regs[rs1(inst)] * (int64_t)cpu->regs[rs2(inst)]);
    //print_op("mul\n", cpu);
    //printf("=%#-13.2lx * %#-13.2lx = %#-13.2lx\n", cpu->regs[rs1(inst)], cpu->regs[rs2(inst)], cpu->regs[rd(inst)]);
}

void exec_SUB(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] =
        (uint64_t) ((int64_t)cpu->regs[rs1(inst)] - (int64_t)cpu->regs[rs2(inst)]);
    //print_op("sub\n", cpu);
}

void exec_SLL(CPU* cpu, uint32_t inst) {
    fprintf(cpu->debug_log, "pc=%#-13.2lx  sll=%#-13.2lx << %#-13.2x = %#-13.2lx\n", cpu->pc-4, cpu->regs[rs1(inst)], shamt(inst), cpu->regs[rs1(inst)] << shamt(inst));
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] << shamt(inst);
    //print_op("sll\n", cpu);
}

void exec_SLT(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < (int64_t) cpu->regs[rs2(inst)])?1:0;
    //print_op("slt\n", cpu);
}

void exec_SLTU(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < cpu->regs[rs2(inst)])?1:0;
    //print_op("slti\n", cpu);
}

void exec_XOR(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] ^ cpu->regs[rs2(inst)];
    //print_op("xor\n", cpu);
}

void exec_SRL(CPU* cpu, uint32_t inst) {
    //fprintf(cpu->debug_log, "pc=%#-13.2lx  srl=%#-13.2lx >> %#-13.2x = %#-13.2lx\n", cpu->pc-4, cpu->regs[rs1(inst)], shamt(inst), cpu->regs[rs1(inst)] >> shamt(inst));
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] >> shamt(inst);
    //print_op("srl\n", cpu);
}

void exec_SRA(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = ((int64_t)cpu->regs[rs1(inst)]) >> (shamt(inst));
    //print_op("sra\n", cpu);
}

void exec_OR(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] | cpu->regs[rs2(inst)];
    //print_op("or\n", cpu);
}

void exec_AND(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] & cpu->regs[rs2(inst)];
    //print_op("and\n", cpu);
}

void exec_FENCE(CPU* cpu, uint32_t inst) {
    //print_op("fence\n", cpu);
}

void exec_ECALL(CPU* cpu, uint32_t inst) {
    switch(cpu->mode) {
        case User: cpu->trap = EnvironmentCallFromUMode;
        case Supervisor: cpu->trap = EnvironmentCallFromSMode;
        case Machine: cpu->trap = EnvironmentCallFromMMode;
    }
    //print_op("ecall\n", cpu);
}
void exec_EBREAK(CPU* cpu, uint32_t inst) {
    cpu->trap = Breakpoint;
    //print_op("ebreak\n", cpu);
}

void exec_ADDIW(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (int64_t)(int32_t)(cpu->regs[rs1(inst)] + (int64_t) imm);
    //print_op("addiw\n", cpu);
}

// TODO
void exec_SLLIW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] << shamt_IW(inst));
    //print_op("slliw\n", cpu);
}
void exec_SRLIW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (((uint32_t)cpu->regs[rs1(inst)]) >> shamt_IW(inst));
    //print_op("srliw\n", cpu);
}
void exec_SRAIW(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (int64_t) (((int32_t)cpu->regs[rs1(inst)]) >> shamt_IW(inst));
    //print_op("sraiw\n", cpu);
}
void exec_ADDW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            + (int64_t) cpu->regs[rs2(inst)]);
    //print_op("addw\n", cpu);
}
void exec_MULW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            * (int64_t) cpu->regs[rs2(inst)]);
    //print_op("mulw\n", cpu);
}
void exec_SUBW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            - (int64_t) cpu->regs[rs2(inst)]);
    //print_op("subw\n", cpu);
}
void exec_DIVW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            / (int64_t) cpu->regs[rs2(inst)]);
    //print_op("divw\n", cpu);
}
void exec_SLLW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int32_t) (((uint32_t)cpu->regs[rs1(inst)]) << shamt_W(inst));
    //print_op("sllw\n", cpu);
}
void exec_SRLW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] >>  cpu->regs[rs2(inst)]);
    //print_op("srlw\n", cpu);
}
void exec_DIVUW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] / (int64_t) cpu->regs[rs2(inst)];
    //print_op("divuw\n", cpu);
}
void exec_SRAW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] >>  (uint64_t)(int64_t)(int32_t) cpu->regs[rs2(inst)]);
    //print_op("sraw\n", cpu);
}
void exec_REMW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            % (int64_t) cpu->regs[rs2(inst)]);
    //print_op("remw\n", cpu);
}
void exec_REMUW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] % (int64_t) cpu->regs[rs2(inst)];
    //print_op("remuw\n", cpu);
}

// CSR instructions
void exec_CSRRW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = csr_read(cpu, csr(inst));
    csr_write(cpu, csr(inst), cpu->regs[rs1(inst)]);
    cpu_update_paging(cpu, csr(inst));
    //print_op("csrrw\n", cpu);
}
void exec_CSRRS(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->csr[csr(inst)];
    csr_write(cpu, csr(inst), cpu->csr[csr(inst)] | cpu->regs[rs1(inst)]);
    cpu_update_paging(cpu, csr(inst));
    //print_op("csrrs\n", cpu);
}
void exec_CSRRC(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->csr[csr(inst)];
    csr_write(cpu, csr(inst), cpu->csr[csr(inst)] & !(cpu->regs[rs1(inst)]) );
    cpu_update_paging(cpu, csr(inst));
    //print_op("csrrc\n", cpu);
}
void exec_CSRRWI(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->csr[csr(inst)];
    csr_write(cpu, csr(inst), rs1(inst));
    cpu_update_paging(cpu, csr(inst));
    //print_op("csrrwi\n", cpu);
}
void exec_CSRRSI(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->csr[csr(inst)];
    csr_write(cpu, csr(inst), cpu->csr[csr(inst)] | rs1(inst));
    cpu_update_paging(cpu, csr(inst));
    //print_op("csrrsi\n", cpu);
}
void exec_CSRRCI(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->csr[csr(inst)];
    csr_write(cpu, csr(inst), cpu->csr[csr(inst)] & !rs1(inst));
    cpu_update_paging(cpu, csr(inst));
    //print_op("csrrci\n", cpu);
}

// AMO_W
void exec_LR_W(CPU* cpu, uint32_t inst) {}
void exec_SC_W(CPU* cpu, uint32_t inst) {}
void exec_AMOSWAP_W(CPU* cpu, uint32_t inst) {
    // int funct7 = (inst >> 25) & 0x7f;
    // printf("funct5=%#.8x\n", (funct7 >> 2));
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 32);
    //cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 32, cpu->regs[rs2(inst)]);
    cpu->regs[rd(inst)] = tmp;
    //print_op("amoswap.w\n", cpu);
}
void exec_AMOADD_W(CPU* cpu, uint32_t inst) {
    // int funct7 = (inst >> 25) & 0x7f;
    // printf("funct5=%#.8x\n", (funct7 >> 2));
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 32);
    uint32_t res = tmp + (uint32_t)cpu->regs[rs2(inst)];
    //cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 32, res);
    cpu->regs[rd(inst)] = tmp;
    //print_op("amoadd.w\n", cpu);
}
void exec_AMOXOR_W(CPU* cpu, uint32_t inst) {
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 32);
    uint32_t res = tmp ^ (uint32_t)cpu->regs[rs2(inst)];
    cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 32, res);
    //print_op("amoxor.w\n", cpu);
} 
void exec_AMOAND_W(CPU* cpu, uint32_t inst) {
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 32);
    uint32_t res = tmp & (uint32_t)cpu->regs[rs2(inst)];
    cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 32, res);
    //print_op("amoand.w\n", cpu);
} 
void exec_AMOOR_W(CPU* cpu, uint32_t inst) {
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 32);
    uint32_t res = tmp | (uint32_t)cpu->regs[rs2(inst)];
    cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 32, res);
    //print_op("amoor.w\n", cpu);
}

void exec_AMOMIN_W(CPU* cpu, uint32_t inst) {}
void exec_AMOMAX_W(CPU* cpu, uint32_t inst) {}
void exec_AMOMINU_W(CPU* cpu, uint32_t inst) {}
void exec_AMOMAXU_W(CPU* cpu, uint32_t inst) {}

// AMO_D TODO
void exec_LR_D(CPU* cpu, uint32_t inst) {}
void exec_SC_D(CPU* cpu, uint32_t inst) {}
void exec_AMOSWAP_D(CPU* cpu, uint32_t inst) {
    // int funct7 = (inst >> 25) & 0x7f;
    // printf("funct5=%#.8x\n", (funct7 >> 2));
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 64);
    //cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 64, cpu->regs[rs2(inst)]);
    cpu->regs[rd(inst)] = tmp;
    //print_op("amoswap.d\n", cpu);
}

void exec_AMOADD_D(CPU* cpu, uint32_t inst) {
    // int funct7 = (inst >> 25) & 0x7f;
    // printf("funct5=%#.8x\n", (funct7 >> 2));
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 64);
    uint32_t res = tmp + (uint32_t)cpu->regs[rs2(inst)];
    //cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 64, res);
    cpu->regs[rd(inst)] = tmp;
    //print_op("amoadd.d\n", cpu);
}

void exec_AMOXOR_D(CPU* cpu, uint32_t inst) {
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 32);
    uint32_t res = tmp ^ (uint32_t)cpu->regs[rs2(inst)];
    cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 32, res);
    //print_op("amoxor.d\n", cpu);
}

void exec_AMOAND_D(CPU* cpu, uint32_t inst) {
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 32);
    uint32_t res = tmp & (uint32_t)cpu->regs[rs2(inst)];
    cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 32, res);
    //print_op("amoand.d\n", cpu);
}

void exec_AMOOR_D(CPU* cpu, uint32_t inst) {
    uint32_t tmp = cpu_load(cpu, cpu->regs[rs1(inst)], 32);
    uint32_t res = tmp | (uint32_t)cpu->regs[rs2(inst)];
    cpu->regs[rd(inst)] = tmp;
    cpu_store(cpu, cpu->regs[rs1(inst)], 32, res);
    //print_op("amoor.d\n", cpu);
}

void exec_AMOMIN_D(CPU* cpu, uint32_t inst) {}
void exec_AMOMAX_D(CPU* cpu, uint32_t inst) {}
void exec_AMOMINU_D(CPU* cpu, uint32_t inst) {}
void exec_AMOMAXU_D(CPU* cpu, uint32_t inst) {}

void exec_SRET(CPU* cpu, uint32_t inst) {
    cpu->pc = csr_read(cpu, SEPC);
    switch ((csr_read(cpu, SSTATUS) >> 8) & 1) {
        case 1: cpu->mode = Supervisor; break;
        default: cpu->mode = User; break;
    }
    if (((csr_read(cpu, SSTATUS) >> 5) & 1) == 1) {
        csr_write(cpu, SSTATUS, csr_read(cpu, SSTATUS) | (1 << 1));
    } else {
        csr_write(cpu, SSTATUS, (csr_read(cpu, SSTATUS) & ~(1<< 1)));
    }
    csr_write(cpu, MSTATUS, csr_read(cpu, MSTATUS) | (1 << 5));
    csr_write(cpu, MSTATUS, (csr_read(cpu, MSTATUS) & ~(1<< 8)));
    //print_op("sret\n", cpu);
}

void exec_MRET(CPU* cpu, uint32_t inst) {
    cpu->pc = csr_read(cpu, MEPC);
    switch ((csr_read(cpu, MSTATUS) >> 11) & 3) {
        case 2: cpu->mode = Machine; break;
        case 1: cpu->mode = Supervisor; break;
        default: cpu->mode = User; break;
    }
    if (((csr_read(cpu, MSTATUS) >> 7) & 1) == 1) {
        csr_write(cpu, MSTATUS, csr_read(cpu, MSTATUS) | (1 << 3));
    } else {
        csr_write(cpu, MSTATUS, (csr_read(cpu, MSTATUS) & ~(1 << 3)));
    }
    csr_write(cpu, MSTATUS, csr_read(cpu, MSTATUS) | (1 << 7));
    csr_write(cpu, MSTATUS, (csr_read(cpu, MSTATUS) & ~(0b11 << 11)));
    //print_op("mret\n", cpu);
}

int cpu_execute(CPU *cpu, uint32_t inst) {
    int opcode = inst & 0x7f;           // opcode in bits 6..0
    int funct3 = (inst >> 12) & 0x7;    // funct3 in bits 14..12
    int funct7 = (inst >> 25) & 0x7f;   // funct7 in bits 31..25
    int funct5 = funct7 >> 2;
    int rs2a = (inst >> 20) & 0x1f;

    cpu->regs[0] = 0;                   // x0 hardwired to 0 at each cycle

    /*printf("%s\n%#.8lx -> Inst: %#.8x <OpCode: %#.2x, funct3:%#x, funct7:%#x> %s\n",
            ANSI_YELLOW, cpu->pc-4, inst, opcode, funct3, funct7, ANSI_RESET); // DEBUG*/
    /*
    switch (cpu->pc-4) {
        case 0x80001180: break;
        case 0x80001184: break;
        case 0x80001188: break;
        case 0x8000118c: break;
        default: fprintf(cpu->debug_log, "\npc=%#.8lx\n", cpu->pc-4);
    }
    */
    //fprintf(cpu->debug_log, "\npc=%#.8lx\n", cpu->pc-4);
    //printf("%s\n%#.8lx -> %s", ANSI_YELLOW, cpu->pc-4, ANSI_RESET); // DEBUG

    switch (opcode) {
        case LUI:   exec_LUI(cpu, inst); break;
        case AUIPC: exec_AUIPC(cpu, inst); break;

        case JAL:   exec_JAL(cpu, inst); break;
        case JALR:  exec_JALR(cpu, inst); break;

        case B_TYPE:
            switch (funct3) {
                case BEQ:   exec_BEQ(cpu, inst); break;
                case BNE:   exec_BNE(cpu, inst); break;
                case BLT:   exec_BLT(cpu, inst); break;
                case BGE:   exec_BGE(cpu, inst); break;
                case BLTU:  exec_BLTU(cpu, inst); break;
                case BGEU:  exec_BGEU(cpu, inst); break;
                default: 
                    fprintf(stderr, 
                        "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                        , opcode, funct3, funct7);
                    cpu->trap = IllegalInstruction;cpu->trap = IllegalInstruction;
                    return 0;
            } break;

        case LOAD:
            switch (funct3) {
                case LB  :  exec_LB(cpu, inst); break;  
                case LH  :  exec_LH(cpu, inst); break;  
                case LW  :  exec_LW(cpu, inst); break;  
                case LD  :  exec_LD(cpu, inst); break;  
                case LBU :  exec_LBU(cpu, inst); break; 
                case LHU :  exec_LHU(cpu, inst); break; 
                case LWU :  exec_LWU(cpu, inst); break; 
                default:
                    fprintf(stderr, 
                        "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                        , opcode, funct3, funct7);
                    cpu->trap = IllegalInstruction;cpu->trap = IllegalInstruction;
                    return 0;
            } break;

        case S_TYPE:
            switch (funct3) {
                case SB  :  exec_SB(cpu, inst); break;  
                case SH  :  exec_SH(cpu, inst); break;  
                case SW  :  exec_SW(cpu, inst); break;  
                case SD  :  exec_SD(cpu, inst); break;  
                default:
                    fprintf(stderr, 
                        "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                        , opcode, funct3, funct7);
                    cpu->trap = IllegalInstruction;cpu->trap = IllegalInstruction;
                    return 0;
            } break;

        case I_TYPE:  
            switch (funct3) {
                case ADDI:  exec_ADDI(cpu, inst); break;
                case SLLI:  exec_SLLI(cpu, inst); break;
                case SLTI:  exec_SLTI(cpu, inst); break;
                case SLTIU: exec_SLTIU(cpu, inst); break;
                case XORI:  exec_XORI(cpu, inst); break;
                case SRI:   
                    switch (funct7 >> 1) {
                        case SRLI:  exec_SRLI(cpu, inst); break;
                        case SRAI:  exec_SRAI(cpu, inst); break;
                        default:
                            fprintf(stderr, 
                                "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                                , opcode, funct3, funct7);
                            cpu->trap = IllegalInstruction;cpu->trap = IllegalInstruction;
                            return 0;
                    } break;
                case ORI:   exec_ORI(cpu, inst); break;
                case ANDI:  exec_ANDI(cpu, inst); break;
                default:
                    fprintf(stderr, 
                            "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                            , opcode, funct3, funct7);
                    cpu->trap = IllegalInstruction;
                    return 0;
            } break;

        case R_TYPE:  
            switch (funct3) {
                case ADDSUB:
                    switch (funct7) {
                        case ADD: exec_ADD(cpu, inst); break;
                        case SUB: exec_SUB(cpu, inst); break;
                        case MUL: exec_MUL(cpu, inst); break;
                        default:
                            fprintf(stderr, 
                                "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                                , opcode, funct3, funct7);
                            cpu->trap = IllegalInstruction;cpu->trap = IllegalInstruction;
                            return 0;
                    } break;
                case SLL:  exec_SLL(cpu, inst); break;
                case SLT:  exec_SLT(cpu, inst); break;
                case SLTU: exec_SLTU(cpu, inst); break;
                case XOR:  exec_XOR(cpu, inst); break;
                case SR:   
                    switch (funct7) {
                        case SRL:  exec_SRL(cpu, inst); break;
                        case SRA:  exec_SRA(cpu, inst); break;
                        default:
                            fprintf(stderr, 
                                "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                                , opcode, funct3, funct7);
                            cpu->trap = IllegalInstruction;cpu->trap = IllegalInstruction;
                            return 0;
                    }
                case OR:   exec_OR(cpu, inst); break;
                case AND:  exec_AND(cpu, inst); break;
                default:
                    fprintf(stderr, 
                            "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                            , opcode, funct3, funct7);
                    cpu->trap = IllegalInstruction;
                    return 0;
            } break;

        case FENCE: exec_FENCE(cpu, inst); break;

        case I_TYPE_64:
            switch (funct3) {
                case ADDIW: exec_ADDIW(cpu, inst); break;
                case SLLIW: exec_SLLIW(cpu, inst); break;
                case SRIW : 
                    switch (funct7) {
                        case SRLIW: exec_SRLIW(cpu, inst); break;
                        case SRAIW: exec_SRLIW(cpu, inst); break;
                        default:
                            fprintf(stderr, 
                                "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                                , opcode, funct3, funct7);
                            cpu->trap = IllegalInstruction;cpu->trap = IllegalInstruction;
                            return 0;
                    } break;
            } break;

        case R_TYPE_64:
            switch (funct3) {
                case ADDSUB:
                    switch (funct7) {
                        case ADDW:  exec_ADDW(cpu, inst); break;
                        case SUBW:  exec_SUBW(cpu, inst); break;
                        case MULW:  exec_MULW(cpu, inst); break;
                    } break;
                case DIVW:  exec_DIVW(cpu, inst); break;
                case SLLW:  exec_SLLW(cpu, inst); break;
                case SRW:
                    switch (funct7) {
                        case SRLW:  exec_SRLW(cpu, inst); break;
                        case SRAW:  exec_SRAW(cpu, inst); break;
                        case DIVUW: exec_DIVUW(cpu, inst); break;
                    } break;
                case REMW:  exec_REMW(cpu, inst); break;
                case REMUW: exec_REMUW(cpu, inst); break;
                default:
                    fprintf(stderr, 
                        "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                        , opcode, funct3, funct7);
                    cpu->trap = IllegalInstruction;cpu->trap = IllegalInstruction;
                    return 0;
            } break;

        case CSR:
            switch (funct3) {
                case ECALLBREAK:
                    switch (rs2a) {
                        case 0x0: exec_ECALL(cpu, inst); return 0; break;
                        case 0x1: exec_EBREAK(cpu, inst); return 0; break;
                        case 0x2:
                            switch (funct7) {
                                case 0x8: exec_SRET(cpu, inst); break;
                                case 0x18: exec_MRET(cpu, inst); break;
                                default: 
                                    fprintf(stderr, 
                                            "[-] ERROR-> opcode:0x%x, funct3:0x%x, rs2:0x%x, funct7:0x%x\n"
                                            , opcode, funct3, rs2a, funct7);
                                    cpu->trap = IllegalInstruction;
                                    return 0;
                            } break;
                        default: 
                            fprintf(stderr, 
                                    "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                                    , opcode, funct3, funct7);
                            cpu->trap = IllegalInstruction;
                            return 0;
                    } break;
                case CSRRW  :  exec_CSRRW(cpu, inst); break;  
                case CSRRS  :  exec_CSRRS(cpu, inst); break;  
                case CSRRC  :  exec_CSRRC(cpu, inst); break;  
                case CSRRWI :  exec_CSRRWI(cpu, inst); break; 
                case CSRRSI :  exec_CSRRSI(cpu, inst); break; 
                case CSRRCI :  exec_CSRRCI(cpu, inst); break; 
                default:
                    fprintf(stderr, 
                            "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                            , opcode, funct3, funct7);
                    cpu->trap = IllegalInstruction;
                    return 0;
            } break;

        case AMO:
            switch(funct3) {
                case AMO_W:
                    switch (funct5) {
                        case LR_W      :  exec_LR_W(cpu, inst); break;  
                        case SC_W      :  exec_SC_W(cpu, inst); break;  
                        case AMOSWAP_W :  exec_AMOSWAP_W(cpu, inst); break;  
                        case AMOADD_W  :  exec_AMOADD_W(cpu, inst); break; 
                        case AMOXOR_W  :  exec_AMOXOR_W(cpu, inst); break; 
                        case AMOAND_W  :  exec_AMOAND_W(cpu, inst); break; 
                        case AMOOR_W   :  exec_AMOOR_W(cpu, inst); break; 
                        case AMOMIN_W  :  exec_AMOMIN_W(cpu, inst); break; 
                        case AMOMAX_W  :  exec_AMOMAX_W(cpu, inst); break; 
                        case AMOMINU_W :  exec_AMOMINU_W(cpu, inst); break; 
                        case AMOMAXU_W :  exec_AMOMAXU_W(cpu, inst); break;
                        default:
                            fprintf(stderr, 
                                "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct5:0x%x\n"
                                , opcode, funct3, funct5);
                            cpu->trap = IllegalInstruction;
                            return 0;
                    } break;
                case AMO_D:
                    switch (funct5) {
                        case LR_D      :  exec_LR_D(cpu, inst); break;  
                        case SC_D      :  exec_SC_D(cpu, inst); break;  
                        case AMOSWAP_D :  exec_AMOSWAP_D(cpu, inst); break;  
                        case AMOADD_D  :  exec_AMOADD_D(cpu, inst); break; 
                        case AMOXOR_D  :  exec_AMOXOR_D(cpu, inst); break; 
                        case AMOAND_D  :  exec_AMOAND_D(cpu, inst); break; 
                        case AMOOR_D   :  exec_AMOOR_D(cpu, inst); break; 
                        case AMOMIN_D  :  exec_AMOMIN_D(cpu, inst); break; 
                        case AMOMAX_D  :  exec_AMOMAX_D(cpu, inst); break; 
                        case AMOMINU_D :  exec_AMOMINU_D(cpu, inst); break; 
                        case AMOMAXU_D :  exec_AMOMAXU_D(cpu, inst); break;
                        default:
                            fprintf(stderr, 
                                "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct5:0x%x\n"
                                , opcode, funct3, funct5);
                            cpu->trap = IllegalInstruction;
                            return 0;
                    } break;
                default: cpu->trap = IllegalInstruction; return 0;
            } break;
            default:
                fprintf(stderr, 
                        "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct7:0x%x\n"
                        , opcode, funct3, funct7);
                cpu->trap = IllegalInstruction;
                return 0;
        }
    return 1;
}

void dump_registers(CPU *cpu) {
    char* abi[] = { // Application Binary Interface registers
        "zero", "ra",  "sp",  "gp",
          "tp", "t0",  "t1",  "t2",
          "s0", "s1",  "a0",  "a1",
          "a2", "a3",  "a4",  "a5",
          "a6", "a7",  "s2",  "s3",
          "s4", "s5",  "s6",  "s7",
          "s8", "s9", "s10", "s11",
          "t3", "t4",  "t5",  "t6",
    };

    /*for (int i=0; i<8; i++) {*/
        /*printf("%4s| x%02d: %#-8.2lx\t", abi[i],    i,    cpu->regs[i]);*/
        /*printf("%4s| x%02d: %#-8.2lx\t", abi[i+8],  i+8,  cpu->regs[i+8]);*/
        /*printf("%4s| x%02d: %#-8.2lx\t", abi[i+16], i+16, cpu->regs[i+16]);*/
        /*printf("%4s| x%02d: %#-8.2lx\n", abi[i+24], i+24, cpu->regs[i+24]);*/
    /*}*/

    for (int i=0; i<8; i++) {
        fprintf(cpu->debug_log, "   %4s: %#-13.2lx  ", abi[i],    cpu->regs[i]);
        fprintf(cpu->debug_log, "   %2s: %#-13.2lx  ", abi[i+8],  cpu->regs[i+8]);
        fprintf(cpu->debug_log, "   %2s: %#-13.2lx  ", abi[i+16], cpu->regs[i+16]);
        fprintf(cpu->debug_log, "   %3s: %#-13.2lx\n", abi[i+24], cpu->regs[i+24]);
    }
}

void dump_csr(CPU* cpu) {
    fprintf(cpu->debug_log, "   mstatus: %#-13.2lx  ", csr_read(cpu, MSTATUS));
    fprintf(cpu->debug_log, "   mtvec: %#-13.2lx  ", csr_read(cpu, MTVEC));
    fprintf(cpu->debug_log, "   mepc: %#-13.2lx  ", csr_read(cpu, MEPC));
    fprintf(cpu->debug_log, "   mcause: %#-13.2lx\n", csr_read(cpu, MCAUSE));
}

void take_trap(CPU* cpu, bool interrupting) {
    uint64_t exec_pc = cpu->pc - 4;
    Mode prev_mode = cpu->mode;

    if (interrupting) {
        cpu->trap = (1ULL << 63) | cpu->trap;
    }

    if ((prev_mode <= Supervisor) && (csr_read(cpu, MEDELEG) >> (uint32_t)cpu->trap) != 0) {
        cpu->mode = Supervisor;

        if (interrupting) {
            uint64_t vector;
            switch (csr_read(cpu, STVEC) & 1) {
                case 1: vector = 4 * cpu->trap; break;
                default: vector = 0; break;
            }
            cpu->pc = (csr_read(cpu, STVEC) & !1) + vector;
        } else {
            cpu->pc = csr_read(cpu, STVEC);
        }
        csr_write(cpu, SEPC, exec_pc);
        csr_write(cpu, SCAUSE, cpu->trap);
        csr_write(cpu, STVAL, 0);
        if (((csr_read(cpu, SSTATUS) >> 1) & 1) == 1) {
            csr_write(cpu, SSTATUS, csr_read(cpu, SSTATUS) | (1 << 5));
        } else {
            csr_write(cpu, SSTATUS, (csr_read(cpu, SSTATUS) & ~(1 << 5)));
        }
        csr_write(cpu, SSTATUS, (csr_read(cpu, SSTATUS) & ~(1 << 1)));
        switch (prev_mode) {
            case User: csr_write(cpu, SSTATUS, (csr_read(cpu, SSTATUS) & ~(1 << 8))); break;
            default: csr_write(cpu, SSTATUS, csr_read(cpu, SSTATUS) | (1 << 8)); break;
        }
    } else {
        cpu->mode = Machine;

        if (interrupting) {
            uint64_t vector;
            switch (csr_read(cpu, MTVEC) & 1) {
                case 1: vector = 4 * cpu->trap; break;
                default: vector = 0; break;
            }
            cpu->pc = (csr_read(cpu, MTVEC) & !1) + vector;
        } else {
            cpu->pc = csr_read(cpu, MTVEC);
        }
        csr_write(cpu, MEPC, exec_pc);
        csr_write(cpu, MCAUSE, cpu->trap);
        csr_write(cpu, MTVAL, 0);
        if (((csr_read(cpu, MSTATUS) >> 3) & 1) == 1) {
            csr_write(cpu, MSTATUS, csr_read(cpu, MSTATUS) | (1 << 7));
        } else {
            csr_write(cpu, MSTATUS, (csr_read(cpu, MSTATUS) & ~(1 << 7)));
        }
        csr_write(cpu, MSTATUS, (csr_read(cpu, MSTATUS) & ~(1 << 3)));
        csr_write(cpu, MSTATUS, (csr_read(cpu, MSTATUS) & ~(0b11 << 11)));
    }
}

bool is_fatal(CPU* cpu) {
    switch (cpu->trap) {
        case InstructionAddressMisaligned: return true; break;
        case InstructionAccessFault: return true; break;
        case LoadAccessFault: return true; break;
        case StoreAMOAddressMisaligned: return true; break;
        case StoreAMOAccessFault: return true; break;
        default: return false; break;
    }
}

void virtio_disk_access(CPU* cpu) {
    uint64_t desc_addr = virtio_desc_addr(&(cpu->bus.virtio));
    uint64_t avail_addr = virtio_desc_addr(&(cpu->bus.virtio)) + 0x40;
    uint64_t used_addr = virtio_desc_addr(&(cpu->bus.virtio)) + 4096;

    uint64_t offset = bus_load(&(cpu->bus), avail_addr + 1, 16);
    uint64_t index = bus_load(&(cpu->bus), avail_addr + (offset % DESC_NUM) + 2, 16);

    uint64_t desc_addr0 = desc_addr + VRING_DESC_SIZE * index;
    uint64_t addr0 = bus_load(&(cpu->bus), desc_addr0, 64);
    uint64_t next0 = bus_load(&(cpu->bus), desc_addr0 + 14, 64);

    uint64_t desc_addr1 = desc_addr + VRING_DESC_SIZE * next0;
    uint64_t addr1 = bus_load(&(cpu->bus), desc_addr0, 64);
    uint64_t len1 = bus_load(&(cpu->bus), desc_addr1 + 8, 32);
    uint64_t flags1 = bus_load(&(cpu->bus), desc_addr1 + 12, 16);

    uint64_t blk_sector = bus_load(&(cpu->bus), addr0 + 8, 64);

    if ((flags1 & 2) == 0) {
        for (uint64_t i = 0; i < len1; i++) {
            uint64_t data = bus_load(&(cpu->bus), addr1 + i, 8);
            virtio_write_disk(&(cpu->bus.virtio), blk_sector * 512 + i, data);
        }
    } else {
        for (uint64_t i = 0; i < len1; i++) {
            uint64_t data = virtio_read_disk(&(cpu->bus.virtio), blk_sector * 512 + 1);
            bus_store(&(cpu->bus), addr1 + i, 8, data);
        }
    }

    uint64_t new_id = virtio_get_new_id(&(cpu->bus.virtio));
    bus_store(&(cpu->bus), used_addr + 2, 16, new_id % 8);
}
