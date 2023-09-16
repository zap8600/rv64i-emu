#ifndef UART_H
#define UART_H

#define UART_BASE 0x10000000
#define UART_SIZE 0x100

typedef struct UART {
    uint8_t data[UART_SIZE];
} UART;

uint64_t uart_load(UART* uart, uint64_t addr, uint64_t size);
void uart_store(UART* uart, uint64_t addr, uint64_t size, uint64_t value);

#endif
