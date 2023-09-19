#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "../includes/uart.h"

#define UART_RHR (UART_BASE + 0)
#define UART_THR (UART_BASE + 0)
#define UART_LCR (UART_BASE + 3)

#define UART_LSR (UART_BASE + 5)

#define UART_LSR_RX 1
#define UART_LSR_TX (1 << 5)

void *uart_in(void *ptr) {
    UART* uart = (UART*)ptr;
    pthread_mutex_lock(&(uart->data_mutex));
    int c;
    if ((c = fgetc(stdin)) != EOF) {
        while ((uart->data[UART_LSR - UART_BASE] & UART_LSR_RX) == 1) {
            pthread_cond_wait(&(uart->cond), &(uart->data_mutex));
        }
        uart->data[0] = (uint8_t)c;
        pthread_mutex_lock(&(uart->intr_mutex));
        uart->interrupting = true;
        pthread_mutex_unlock(&(uart->intr_mutex));
        uart->data[UART_LSR - UART_BASE] |= UART_LSR_RX;
    }
    pthread_mutex_lock(&(uart->intr_mutex));
    uart->interrupting = false;
    pthread_mutex_unlock(&(uart->intr_mutex));
    pthread_mutex_unlock(&(uart->data_mutex));
}

void uart_init(UART* uart) {
    pthread_create(&(uart->rx_thread), NULL, &uart_in, uart);
}

uint64_t uart_load_8(UART* uart, uint64_t addr) {
    pthread_mutex_lock(&(uart->data_mutex));
    switch (addr) {
        case UART_RHR: 
            uart->data[UART_LSR - UART_BASE] &= !UART_LSR_RX;
            pthread_mutex_unlock(&(uart->data_mutex));
            return uart->data[UART_RHR - UART_BASE];
            break;
        default:
            pthread_mutex_unlock(&(uart->data_mutex));
            return uart->data[addr - UART_BASE];
            break;
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
    pthread_mutex_lock(&(uart->data_mutex));
    switch (addr) {
        case UART_THR: printf("%c", (uint8_t)value); break;
        default: uart->data[addr - UART_BASE] = (uint8_t)value; break;
    }
    pthread_mutex_unlock(&(uart->data_mutex));
}

void uart_store(UART* uart, uint64_t addr, uint64_t size, uint64_t value) {
    switch (size) {
        case 8: uart_store_8(uart, addr, value); break;
        default: ;
    }
}

bool uart_interrupting(UART* uart) {
    pthread_mutex_lock(&(uart->intr_mutex));
    if(uart->interrupting) {
        uart->interrupting = false;
        return true;
    }
    return false;
}
