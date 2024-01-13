#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
    /*
    int c;
    if ((c = fgetc(stdin)) != EOF) {
        pthread_mutex_lock(&(uart->data_mutex));
        while ((uart->data[UART_LSR - UART_BASE] & UART_LSR_RX) == 1) {
            pthread_cond_wait(&(uart->cond), &(uart->data_mutex));
        }
        uart->data[0] = (uint8_t)c;
        pthread_mutex_lock(&(uart->intr_mutex));
        uart->interrupting = true;
        pthread_mutex_unlock(&(uart->intr_mutex));
        uart->data[UART_LSR - UART_BASE] |= UART_LSR_RX;
        pthread_mutex_unlock(&(uart->data_mutex));
    }
    */
    while(1) {
        char c = 0;
        if ((read(fileno(stdin), (char*)&c, 1)) > 0) {
            pthread_mutex_lock(&(uart->data_mutex));
            while ((uart->data[UART_LSR - UART_BASE] & UART_LSR_RX) == 1) {
                pthread_cond_wait(&(uart->cond), &(uart->data_mutex));
            }
            uart->data[0] = c;
            pthread_mutex_lock(&(uart->intr_mutex));
            uart->interrupting = true;
            pthread_mutex_unlock(&(uart->intr_mutex));
            uart->data[UART_LSR - UART_BASE] |= UART_LSR_RX;
            pthread_mutex_unlock(&(uart->data_mutex));
        }
        pthread_mutex_lock(&(uart->intr_mutex));
        uart->interrupting = false;
        pthread_mutex_unlock(&(uart->intr_mutex));
    }
}

void uart_init(UART* uart) {
    uart->data = malloc(UART_SIZE);
    uart->data[UART_LSR - UART_BASE] |= UART_LSR_TX;
    uart->interrupting = false;
    pthread_create(&(uart->rx_thread), NULL, &uart_in, uart);
}

uint64_t uart_load_8(UART* uart, uint64_t addr) {
    uint8_t data;
    pthread_mutex_lock(&(uart->data_mutex));
    switch (addr) {
        case UART_RHR:
            pthread_cond_broadcast(&(uart->cond));
            uart->data[UART_LSR - UART_BASE] &= ~UART_LSR_RX;
            data = uart->data[UART_RHR - UART_BASE];
            pthread_mutex_unlock(&(uart->data_mutex));
            return data;
            break;
        default:
            pthread_mutex_unlock(&(uart->data_mutex));
            return uart->data[addr - UART_BASE];
            break;
    }
    pthread_mutex_unlock(&(uart->data_mutex));
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
        case UART_THR:
            printf("%c", (uint8_t)value);
            fflush(stdout);
            //fprintf(stderr, "%c", (uint8_t)value); // for debug so that reg & csr go to log
            //fflush(stderr); // and uart out goes to terminal
            break;
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
    pthread_mutex_unlock(&(uart->intr_mutex));
    return false;
}
