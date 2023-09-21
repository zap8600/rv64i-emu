#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "./includes/cpu.h"

struct CPU cpu;

void read_bin(CPU* cpu, char *filename)
{
    FILE *file;
    uint8_t *buffer;
    unsigned long fileLen;

    file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Unable to open file %s", filename);
    }

    fseek(file, 0, SEEK_END);
    fileLen=ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer=(uint8_t *)malloc(fileLen+1);
    if (!buffer)
    {
        fprintf(stderr, "Memory error!");
        fclose(file);
    }

    fread(buffer, fileLen, 1, file);
    fclose(file);

    // Print file contents in hex
    /*for (int i=0; i<fileLen; i+=2) {*/
        /*if (i%16==0) printf("\n%.8x: ", i);*/
        /*printf("%02x%02x ", *(buffer+i), *(buffer+i+1));*/
    /*}*/
    /*printf("\n");*/

    memcpy(cpu->bus.dram.mem, buffer, fileLen*sizeof(uint8_t));
    free(buffer);
}

void read_disk(CPU* cpu, char *filename)
{
    FILE *file;
    uint8_t *buffer;
    unsigned long fileLen;

    file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Unable to open file %s", filename);
    }

    fseek(file, 0, SEEK_END);
    fileLen=ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer=(uint8_t *)malloc(fileLen+1);
    if (!buffer)
    {
        fprintf(stderr, "Memory error!");
        fclose(file);
    }

    fread(buffer, fileLen, 1, file);
    fclose(file);

    // Print file contents in hex
    /*for (int i=0; i<fileLen; i+=2) {*/
        /*if (i%16==0) printf("\n%.8x: ", i);*/
        /*printf("%02x%02x ", *(buffer+i), *(buffer+i+1));*/
    /*}*/
    /*printf("\n");*/

    memcpy(cpu->bus.virtio.disk, buffer, fileLen*sizeof(uint8_t));
    free(buffer);
}

void exitEmu() {
    free(cpu.bus.dram.mem);
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filename> <disk>\n", argv[0]);
        return 1;
    }

    cpu_init(&cpu);
    read_bin(&cpu, argv[1]);
    read_disk(&cpu, argv[2]);

    signal(SIGINT, exitEmu);

    while (1) {
        // fetch
        uint32_t inst = cpu_fetch(&cpu);
        // Increment the program counter
        cpu.pc += 4;
        // execute
        if (!cpu_execute(&cpu, inst)) {
            take_trap(&cpu, false);
            if (is_fatal(&cpu)) {
                break;
            }
        }

        cpu_check_interrupt(&cpu);
        switch (cpu.intr) {
            case -1: ; break;
            default: take_trap(&cpu, true); break;
        }

        if(cpu.pc==0)
            break;

        /*
        if (cpu.enable_paging) {
            printf("paging on!\n");
        } else {
            printf("paging off!\n");
        }
        */
        dump_registers(&cpu);
        dump_csr(&cpu);
    }
    dump_registers(&cpu);
    printf("\n");
    dump_csr(&cpu);
    return 0;
}
