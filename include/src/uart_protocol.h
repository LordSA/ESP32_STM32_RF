#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <Arduino.h>

// Matches STM32 huart1.Init.BaudRate (9600 in STM32 main.c)
#define UART_STM32_BAUD 9600
#define UART_STM32_RX_PIN 16
#define UART_STM32_TX_PIN 17
#define UART_BUF_SIZE 128

void uartProtocolInit();
void uartProtocolLoop();
void sendUartMessage(const String &msg);

#endif // UART_PROTOCOL_H
