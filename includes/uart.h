#ifndef UART_H
#define UART_H

#define UART_BASE 0x10000000
#define UART_SIZE 0x100

typedef struct UART {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint8_t data[UART_SIZE];
    bool interrupting;
} UART;

void uart_init(UART* uart);

#endif
