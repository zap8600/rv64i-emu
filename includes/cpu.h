#include <stdint.h>

#include "bus.h"
#include "opcodes.h"

typedef struct CPU {
    uint64_t regs[32];
    uint64_t pc;
    struct BUS bus;
} CPU;

void cpu_init(CPU *cpu) {
    cpu->regs[0] = 0x00;           // register x0 hardwired to 0
    cpu->regs[2] = DRAM_SIZE;   // Set stack pointer
    cpu->pc = DRAM_BASE;        // Set program counter to the base address
}

uint32_t cpu_fetch(CPU *cpu) {
    uint32_t inst = bus_load(&(cpu->bus), cpu->pc, 32);
    return inst;
}

uint64_t cpu_load(CPU* cpu, uint64_t addr, uint64_t size) {
    return bus_load(&(cpu->bus), addr, size);
}

void cpu_store(CPU* cpu, uint64_t addr, uint64_t size, uint64_t value) {
    bus_store(&(cpu->bus), addr, size, value);
}

// Decode instruction

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
    return ((int64_t)(int32_t) (inst & 0xfff00000)) >> 20; // right shift as signed?
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
    return (int64_t)(int32_t)(inst & 0xfffff999);
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
    return (uint32_t) (imm_I(inst) & 0x1f); // TODO: 0x1f / 0x3f ?
}


void exec_LUI(CPU* cpu, uint32_t inst) {
    // LUI places upper 20 bits of U-immediate value to rd
    cpu->regs[rd(inst)] = (uint64_t)(int64_t)(int32_t)(inst & 0xfffff000);
}

void exec_AUIPC(CPU* cpu, uint32_t inst) {
    // AUIPC forms a 32-bit offset from the 20 upper bits 
    // of the U-immediate
    uint64_t imm = imm_U(inst);
    cpu->regs[rd(inst)] = ((int64_t) cpu->pc + (int64_t) imm) - 4;
}

void exec_JAL(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_J(inst);
    cpu->regs[rd(inst)] = cpu->pc;
    /*printf("JAL-> rd:%ld, pc:%lx\n", rd(inst), cpu->pc);*/
    cpu->pc = cpu->pc + (int64_t) imm - 4;
}

void exec_JALR(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    uint64_t tmp = cpu->pc;
    cpu->pc = (cpu->regs[rs1(inst)] + (int64_t) imm) & 0xfffffffe;
    cpu->regs[rd(inst)] = tmp;
    /*printf("NEXT -> %#lx, imm:%#lx\n", cpu->pc, imm);*/
}

void exec_BEQ(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] == (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
}
void exec_BNE(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] != (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = (cpu->pc + (int64_t) imm - 4);
}
void exec_BLT(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] < (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
}
void exec_BGE(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if ((int64_t) cpu->regs[rs1(inst)] >= (int64_t) cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
}
void exec_BLTU(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if (cpu->regs[rs1(inst)] < cpu->regs[rs2(inst)])
        cpu->pc = cpu->pc + (int64_t) imm - 4;
}
void exec_BGEU(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_B(inst);
    if (cpu->regs[rs1(inst)] >= cpu->regs[rs2(inst)])
        cpu->pc = (int64_t) cpu->pc + (int64_t) imm - 4;
}
void exec_LB(CPU* cpu, uint32_t inst) {
    // load 1 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t)(int8_t) cpu_load(cpu, addr, 8);
}
void exec_LH(CPU* cpu, uint32_t inst) {
    // load 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t)(int16_t) cpu_load(cpu, addr, 16);
}
void exec_LW(CPU* cpu, uint32_t inst) {
    // load 4 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t)(int32_t) cpu_load(cpu, addr, 32);
}
void exec_LD(CPU* cpu, uint32_t inst) {
    // load 8 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = (int64_t) cpu_load(cpu, addr, 64);
}
void exec_LBU(CPU* cpu, uint32_t inst) {
    // load unsigned 1 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 8);
}
void exec_LHU(CPU* cpu, uint32_t inst) {
    // load unsigned 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 16);
}
void exec_LWU(CPU* cpu, uint32_t inst) {
    // load unsigned 2 byte to rd from address in rs1
    uint64_t imm = imm_I(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu->regs[rd(inst)] = cpu_load(cpu, addr, 32);
}
void exec_SB(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 8, cpu->regs[rs2(inst)]);
}
void exec_SH(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 16, cpu->regs[rs2(inst)]);
}
void exec_SW(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 32, cpu->regs[rs2(inst)]);
}
void exec_SD(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_S(inst);
    uint64_t addr = cpu->regs[rs1(inst)] + (int64_t) imm;
    cpu_store(cpu, addr, 64, cpu->regs[rs2(inst)]);
}

void exec_ADDI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] + (int64_t) imm;
}

void exec_SLLI(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] << shamt(inst);
}

void exec_SLTI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < (int64_t) imm)?1:0;
}

void exec_SLTIU(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (cpu->regs[rs1(inst)] < imm)?1:0;
}

void exec_XORI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] ^ imm;
}

void exec_SRLI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] >> imm;
}

void exec_SRAI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (int32_t)cpu->regs[rs1(inst)] >> imm;
}

void exec_ORI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] | imm;
}

void exec_ANDI(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] & imm;
}

void exec_ADD(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] =
        (uint64_t) ((int64_t)cpu->regs[rs1(inst)] + (int64_t)cpu->regs[rs2(inst)]);
}

void exec_ADDIW(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] + (int64_t) imm;
}

// TODO
void exec_SLLIW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] <<  shamt(inst));
    print_op("slliw\n");
}
void exec_SRLIW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] >>  shamt(inst));
    print_op("srliw\n");
}
void exec_SRAIW(CPU* cpu, uint32_t inst) {
    uint64_t imm = imm_I(inst);
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] >> (uint64_t)(int64_t)(int32_t) imm);
    print_op("sraiw\n");
}
void exec_ADDW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            + (int64_t) cpu->regs[rs2(inst)]);
}
void exec_MULW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            * (int64_t) cpu->regs[rs2(inst)]);
}
void exec_SUBW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            - (int64_t) cpu->regs[rs2(inst)]);
}
void exec_DIVW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            / (int64_t) cpu->regs[rs2(inst)]);
}
void exec_SLLW(CPU* cpu, uint32_t inst) {
}
void exec_SRLW(CPU* cpu, uint32_t inst) {
}
void exec_DIVUW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] / (int64_t) cpu->regs[rs2(inst)];
}
void exec_SRAW(CPU* cpu, uint32_t inst) {
}
void exec_REMW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = (int64_t)(int32_t) (cpu->regs[rs1(inst)] 
            % (int64_t) cpu->regs[rs2(inst)]);
}
void exec_REMUW(CPU* cpu, uint32_t inst) {
    cpu->regs[rd(inst)] = cpu->regs[rs1(inst)] % (int64_t) cpu->regs[rs2(inst)];
}



int cpu_execute(CPU *cpu, uint32_t inst) {
    int opcode = inst & 0x7f;           // opcode in bits 6..0
    int funct3 = (inst >> 12) & 0x7;    // funct3 in bits 14..12
    int funct7 = (inst >> 25) & 0x7f;   // funct7 in bits 31..25

    cpu->regs[0] = 0;                    // x0 reset to 0 at each cycle

    printf("%s\nPC: %#.8lx Inst: %#.8x <OpCode: %#.2x, funct3:%#x, funct7:%#x>%s\n",
            ANSI_YELLOW, cpu->pc-4, inst, opcode, funct3, funct7, ANSI_RESET); // DEBUG

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
                default: ;
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
                default: ;
            } break;

        case S_TYPE:
            switch (funct3) {
                case SB  :  exec_SB(cpu, inst); break;  
                case SH  :  exec_SH(cpu, inst); break;  
                case SW  :  exec_SW(cpu, inst); break;  
                case SD  :  exec_SD(cpu, inst); break;  
                default: ;
            } break;

        case I_TYPE:  
            switch (funct3) {
                case ADDI:  exec_ADDI(cpu, inst); break;
                case SLLI:  exec_SLLI(cpu, inst); break;
                case SLTI:  exec_SLTI(cpu, inst); break;
                case SLTIU: exec_SLTIU(cpu, inst); break;
                case XORI:  exec_XORI(cpu, inst); break;
                case SRI:   
                    switch (funct7) {
                        case SRLI:  exec_XORI(cpu, inst); break;
                        case SRAI:  exec_XORI(cpu, inst); break;
                        default: ;
                    } break;
                case ORI:   exec_ORI(cpu, inst); break;
                case ANDI:  exec_ANDI(cpu, inst); break;
                default:
                    fprintf(stderr, 
                            "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct3:0x%x\n"
                            , opcode, funct3, funct7);
                    return 0;
            } break;

        case R_TYPE:  
            switch (funct3) {
                case ADD:  exec_ADD(cpu, inst); break;
                case SLLI:  exec_SLLI(cpu, inst); break;
                case SLTI:  exec_SLTI(cpu, inst); break;
                case SLTIU: exec_SLTIU(cpu, inst); break;
                case XORI:  exec_XORI(cpu, inst); break;
                case SRI:
                    switch (funct7) {
                        case SRLI:  exec_XORI(cpu, inst); break;
                        case SRAI:  exec_XORI(cpu, inst); break;
                        default: ;
                    }
                case ORI:   exec_ORI(cpu, inst); break;
                case ANDI:  exec_ANDI(cpu, inst); break;
                default:
                    fprintf(stderr, 
                            "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct3:0x%x\n"
                            , opcode, funct3, funct7);
                    return 0;
            } break;

        case I_TYPE_64:
            switch (funct3) {
                case ADDIW: exec_ADDIW(cpu, inst); break;
                case SLLIW: exec_SLLIW(cpu, inst); break;
                case SRIW : 
                    switch (funct7) {
                        case SRLIW: exec_SRLIW(cpu, inst); break;
                        case SRAIW: exec_SRLIW(cpu, inst); break;
                    } break;
            } break;

        case R_TYPE_64:
            switch (funct3) {
                case ADDSUB:
                    switch (funct7) {
                        case ADDW:  exec_ADDW(cpu, inst); break;
                        case SUBW:  exec_SUBW(cpu, inst); break;
                        //case MULW:  exec_MULW(cpu, inst); break;
                    } break;
                //case DIVW:  exec_DIVW(cpu, inst); break;
                case SLLW:  exec_SLLW(cpu, inst); break;
                case SRW:
                    switch (funct7) {
                        case SRLW:  exec_SRLW(cpu, inst); break;
                        case SRAW:  exec_SRAW(cpu, inst); break;
                        //case DIVUW: exec_DIVUW(cpu, inst); break;
                    } break;
                //case REMW:  exec_REMW(cpu, inst); break;
                //case REMUW: exec_REMUW(cpu, inst); break;
            } break;

        default:
            fprintf(stderr, 
                    "[-] ERROR-> opcode:0x%x, funct3:0x%x, funct3:0x%x\n"
                    , opcode, funct3, funct7);
            return 0;
            /*exit(1);*/
    }
    return 1;
}

void dump_registers(CPU *cpu) {
    char* abi[] = { // Application Binary Interface registers
        "zero", " ra ", " sp ", " gp ",
        " tp ", " t0 ", " t1 ", " t2 ",
        " s0 ", " s1 ", " a0 ", " a1 ",
        " a2 ", " a3 ", " a4 ", " a5 ",
        " a6 ", " a7 ", " s2 ", " s3 ",
        " s4 ", " s5 ", " s6 ", " s7 ",
        " s8 ", " s9 ", " s10", " s11",
        " t3 ", " t4 ", " t5 ", " t6 ",
    };

    for (int i=0; i<8; i++) {
        printf("(%s)x%02d: %#-8.2lx\t", abi[i],    i,    cpu->regs[i]);
        printf("(%s)x%02d: %#-8.2lx\t", abi[i+8],  i+8,  cpu->regs[i+8]);
        printf("(%s)x%02d: %#-8.2lx\t", abi[i+16], i+16, cpu->regs[i+16]);
        printf("(%s)x%02d: %#-8.2lx\n", abi[i+24], i+24, cpu->regs[i+24]);
    }
}
