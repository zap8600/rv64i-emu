#include <stdio.h>
#include "./includes/cpu.h"
#include "fib.h"

void copy_bin(CPU* cpu) {
    memcpy(cpu->bus.dram.mem, fib_bin, fib_bin_len*sizeof(uint8_t));
}

int main() {
    struct CPU cpu;
    cpu_init(&cpu);
    copy_bin(&cpu);

    while (1) {
        // fetch
        uint32_t inst = cpu_fetch(&cpu);
        // Increment the program counter
        cpu.pc += 4;
        // execute
        if (!cpu_execute(&cpu, inst))
            break;

        if(cpu.pc==0)
            break;
    }
    dump_registers(&cpu);
    return 0;
}
