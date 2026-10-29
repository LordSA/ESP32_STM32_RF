// src/config.h
#pragma once

// --- UART ---
#define UART_STM32_BAUD     9600
#define UART_STM32_RX_PIN   16
#define UART_STM32_TX_PIN   17
#define UART_BUF_SIZE       128

// --- nRF24L01 Radio 1 ---
#define RADIO_CE_PIN        5
#define RADIO_CSN_PIN       4
#define RADIO_SCK_PIN       14
#define RADIO_MOSI_PIN      13
#define RADIO_MISO_PIN      12
#define RADIO_SPI_BUS       2
#define RADIO_CLOCK_SPEED   10000000

// --- nRF24L01 Radio 2 (spare, unused) ---
#define RADIO1_CE_PIN       15
#define RADIO1_CSN_PIN      4
#define RADIO1_SCK_PIN      18
#define RADIO1_MOSI_PIN     23
#define RADIO1_MISO_PIN     19
#define RADIO1_SPI_BUS      1
#define RADIO1_CLOCK_SPEED  8000000

#define RADIO_POLLING_MS    50

#define RADIO_HAS_PA_EN     0

// --- Jam payload: 32 x 0xFF, ~264us of air at 1Mbps ---
static const uint8_t JAM_TEXT[32] = {
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

// --- Boot-time radio self-test (comment out once working) ---
#define RADIO_SELFTEST      1

// --- WiFi AP defaults ---
#define DEFAULT_SSID        "ESP32_Scanner"
#define DEFAULT_PASSWORD    "password123"
#define DEFAULT_CHANNEL     1
#define HIDDEN_SSID         0
#define BOARD_ADDRESS       IPAddress(192, 168, 4, 1)

// --- Preferences ---
#define NVS_NAME            "esp32_config"
#define DEFAULT_TXPOWER_METHOD  2   // 0=MIN 1=LOW 2=HIGH 3=MAX

// --- Serial ---
#define SERIAL_BAUD_RATE    115200

// --- Captive portal ---
#define CAPTIVE_PORTAL
#define DNS_PORT            53