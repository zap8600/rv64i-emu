#include <stdio.h>
#include <stdint.h>
#include "uart.h"

#define UART_RHR (UART_BASE + 0)
#define UART_THR (UART_BASE + 0)
#define UART_LCR (UART_BASE + 3)

#define UART_LSR (UART_BASE + 5)

#define UART_LSR_RX 1
#define UART_LSR_TX (1 << 5)

uint64_t uart_load_8(UART* uart, uint64_t addr) {
    switch (addr) {
        case UART_RHR: 
            uart->data[UART_LSR - UART_BASE] &= !UART_LSR_RX;
            return uart->data[UART_RHR - UART_BASE];
            break;
        default: return uart->data[addr - UART_BASE]; break;
    }
}

uint64_t uart_load(UART* uart, uint64_t addr, uint64_t size) {
    switch (size) {
        case 8: return uart_load_8(uart, addr); break;
        default: ;
    }
    return 1;
}

void uart_store_8(UART* uart, uint64_t addr, uint64_t value) {
    switch (addr) {
        case UART_THR: printf("%s", (char)value); break;
        default: uart->data[addr - UART_BASE] = (uint8_t)value; break;
    }
}

void uart_store(UART* uart, uint64_t addr, uint64_t size, uint64_t value) {
    switch (size) {
        case 8: uart_store_8(uart, addr, value); break;
        default: ;
    }
}
