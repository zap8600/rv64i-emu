#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <pthread.h>

#define UART_IRQ 10

#define UART_BASE 0x10000000
#define UART_SIZE 0x100

typedef struct UART {
    uint8_t *data;
    pthread_t rx_thread;
    pthread_mutex_t data_mutex;
    pthread_cond_t cond;
    pthread_mutex_t intr_mutex;
    bool interrupting;
} UART;

void uart_init(UART* uart);
uint64_t uart_load(UART* uart, uint64_t addr, uint64_t size);
void uart_store(UART* uart, uint64_t addr, uint64_t size, uint64_t value);
bool uart_interrupting(UART* uart);

#endif
