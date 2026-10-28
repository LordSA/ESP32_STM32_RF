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
#define RADIO_CLOCK_SPEED 10000000  // was 8000000    // 8MHz (max for nRF24)

// --- nRF24L01 Radio 2 (if you have a second module) ---
#define RADIO1_CE_PIN       15
#define RADIO1_CSN_PIN      4
#define RADIO1_SCK_PIN      18
#define RADIO1_MOSI_PIN     23
#define RADIO1_MISO_PIN     19
#define RADIO1_SPI_BUS      1
#define RADIO1_CLOCK_SPEED  8000000

#define RADIO_POLLING_MS    50

#define NRF_PA_EN_PIN  25
#define NRF_LNA_EN_PIN 27

#define RADIO_PA_EN_PIN   25
#define RADIO_LNA_EN_PIN  27   



// --- Jam payload ---
static const uint8_t JAM_TEXT[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- WiFi AP defaults ---
#define DEFAULT_SSID        "ESP32_Scanner"
#define DEFAULT_PASSWORD    "password123"
#define DEFAULT_CHANNEL     1
#define HIDDEN_SSID         0  // 0 = visible, 1 = hidden
#define BOARD_ADDRESS       IPAddress(192, 168, 4, 1)

// --- Preferences ---
#define NVS_NAME            "esp32_config"
#define DEFAULT_TXPOWER_METHOD  2  // 0=MIN, 1=MED, 2=MAX, 3=ERROR

// --- Serial ---
#define SERIAL_BAUD_RATE    115200

// --- Captive portal (optional) ---
#define CAPTIVE_PORTAL
#define DNS_PORT            53   