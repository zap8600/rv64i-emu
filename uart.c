#include <stdio.h>
#include <stdint.h>
#include "uart.h"

#define UART_RHR (UART_BASE + 0)
#define UART_THR (UART_BASE + 0)
#define UART_LCR (UART_BASE + 3)

#define UART_LSR (UART_BASE + 5)

#define UART_LSR_RX 1
#define UART_LSR_TX (1 << 5)

void uart_init
