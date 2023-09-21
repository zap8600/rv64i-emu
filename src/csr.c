#include "../includes/csr.h"
#include <stdint.h>
#include <stdio.h>

uint64_t csr_read(CPU* cpu, uint64_t csr) {
    //return (uint64_t)(uint32_t)cpu->csr[csr];
    if (csr == SIE) {
        return ((uint64_t)(uint32_t)cpu->csr[MIE] & (uint64_t)(uint32_t)cpu->csr[MIDELEG]);
    } else {
        return (uint64_t)(uint32_t)cpu->csr[csr];
    }
}

void csr_write(CPU* cpu, uint64_t csr, uint64_t value) {
    //cpu->csr[csr] = value;
    if (csr == SIE) {
        cpu->csr[MIE] = (cpu->csr[MIE] & !(cpu->csr[MIDELEG])) | (value & cpu->csr[MIDELEG]);
    } else {
        cpu->csr[csr] = value;
    }
}