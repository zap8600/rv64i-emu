#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint64_t regs[32];
    uint64_t pc;
    uint8_t *code;
} CPU;

CPU cpu;

int main(int argc, char**argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s [binary]\n", argv[0]);
        return -1;
    }
    FILE* binary = fopen(argv[1], "rb");
    fseek(binary, 0, SEEK_END);
    unsigned long len = ftell(binary);
    fseek(binary, 0, SEEK_SET);
    cpu.code = (uint8_t *)malloc(len + 1);
    fread(cpu.code, len, 1, binary);
    cpu.regs[0] = 0;
    cpu.regs[2] = (128 * 1024 * 1024);
    cpu.pc = 0;
    while(cpu.pc < len) {
        uint32_t inst = (uint32_t)cpu.code[pc] | ((uint32_t)cpu.code[pc + 1]) << 8 | ((uint32_t)cpu.code[pc + 2]) << 16 | ((uint32_t)cpu.code[pc + 3]) << 24;
        cpu.pc += 4;
        uint32_t op = inst & 0x7f;
        switch(op) {
            case 0x13:
            {
                uint64_t imm = (((int64_t)(int32_t)(inst & 0xfe000000)));
                break;
            }
            case 0x33:
            {
                // ADD
            }
        }
    }
}
