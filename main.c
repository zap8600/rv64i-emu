#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint64_t regs[32];
    uint64_t pc;
    uint8_t *code;
} CPU;

int main(int argc, char**argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s [binary]\n", argv[0]);
        return -1;
    }
    FILE* binary = fopen(argv[1], "rb");
}
