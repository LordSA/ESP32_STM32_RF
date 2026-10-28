/*
 * Dual MCU Centralized Architecture - ESP32 Co-Processor
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_bt.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <DNSServer.h>

#include "src/main.h"
#include "src/jammer.h"
#include "src/server.h"
#include "src/radio.h"
#include "src/uart_protocol.h"
#include "src/wifi_scanner.h"
#include "src/ble_scanner.h"
#include "src/config.h"

volatile bool jammerStopFlag = false;
volatile bool isSleeping = false; // Prevents radio.cpp linker error

int channel_method;
String ssid_method, password_method;

TaskHandle_t jammerTaskHandle = NULL;
SemaphoreHandle_t jammerSemaphore;
Preferences preferences;
DNSServer dnsServer;

void halt_esp(){
    Serial.println("ESP halt requested, ignoring to stay alive for STM32.");
}

void loadPreferences() {
    preferences.begin(NVS_NAME, false);
    bluetooth_jam_method = preferences.getInt("nvs_bluetooth_jam", 0);
    drone_jam_method = preferences.getInt("nvs_drone_jam", 0);
    nrf24_module_config_method = preferences.getInt("nvs_nrf24_cfg", 0);
    misc_jam_method = preferences.getInt("nvs_misc_jam", 0);
    radio_txpower_method = preferences.getInt("nvs_txpower", DEFAULT_TXPOWER_METHOD);

    ssid_method = preferences.getString("wifi_ssid", DEFAULT_SSID);
    password_method = preferences.getString("wifi_pass", DEFAULT_PASSWORD);
    channel_method = preferences.getInt("wifi_channel", DEFAULT_CHANNEL);
    preferences.end();
}

/*
 * Retained to satisfy server.cpp references while keeping ESP32 safe
 */
// Add near the top, next to jammerStopFlag:
volatile int jam_mode = 0;  // 0 = wifi, 1 = bluetooth, 2 = ble, etc.

void jammerTask(void* pvParameters) {
    Serial.printf("[jammer] Task started, mode=%d\n", jam_mode);
    jammerStopFlag = false;

    unsigned long t0 = millis();

    switch (jam_mode) {
        case 0:
            Serial.println("[jammer] Calling wifi_jam()");
            wifi_jam();
            break;
        case 1:
            Serial.println("[jammer] Calling bluetooth_jam()");
            bluetooth_jam();
            break;
        case 2:
            Serial.println("[jammer] Calling ble_jam()");
            ble_jam();
            break;
        default:
            Serial.println("[jammer] Unknown mode, defaulting to wifi_jam()");
            wifi_jam();
            break;
    }

    Serial.printf("[jammer] Jam function returned after %lu ms\n", millis() - t0);

    if (xSemaphoreTake(jammerSemaphore, portMAX_DELAY) == pdTRUE) {
        jammerTaskHandle = NULL;
        xSemaphoreGive(jammerSemaphore);
    }
    vTaskDelete(NULL);
}      

void stopJamHandler() {
    Serial.printf("[jammer] Stop requested at %lu ms\n", millis());
    if (xSemaphoreTake(jammerSemaphore, portMAX_DELAY) == pdTRUE) {
        if (jammerTaskHandle != NULL) {
            jammerStopFlag = true;
            Serial.printf("[jammer] Stop flag SET (task=%p still alive)\n", jammerTaskHandle);
        } else {
            Serial.println("[jammer] Stop requested but task already NULL.");
        }
        xSemaphoreGive(jammerSemaphore);
    } else {
        Serial.println("[jammer] ERROR: Could not acquire semaphore for stop.");
    }
}   

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println("\n[BOOT] ESP32 Wireless Co-Processor Booting...");

    // Initialize UART communication with STM32
    uartProtocolInit();
    radio_txpower_method = 3;

    SPI_init();

    if (!SPIFFS.begin(true)) {
        Serial.println("Warning: SPIFFS mount failed, continuing.");
    } else {
        Serial.println("SPIFFS mounted successfully.");
    }

    loadPreferences();
    

    if (ssid_method.length() == 0) {
        ssid_method = "ESP32_Scanner";
    }
    if (channel_method < 1 || channel_method > 11) {
        channel_method = 1;
    }

    // Set dual mode (WIFI_AP_STA) so Wi-Fi scanning functions reliably
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(BOARD_ADDRESS, BOARD_ADDRESS, IPAddress(255, 255, 255, 0));
    
    if (WiFi.softAP(ssid_method.c_str(), password_method.c_str(), channel_method, HIDDEN_SSID)) {
        Serial.printf("Started access point: %s on CH %d\n", ssid_method.c_str(), channel_method);
    } else {
        Serial.println("Warning: AP start failed, continuing in scanner mode.");
    }

    #if defined(CAPTIVE_PORTAL)
    dnsServer.start(DNS_PORT, "*", BOARD_ADDRESS);
    #endif

    setupWebServer();
    Serial.println("HTTP server started.");

    jammerSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(jammerSemaphore);

    wifiScannerInit();
    bleScannerInit();

    Serial.println("[READY] ESP32 Ready for STM32 Commands.");
}


void loop() {
    #if defined(CAPTIVE_PORTAL)
    dnsServer.processNextRequest();
    #endif
    server.handleClient();
    
    uartProtocolLoop();
    wifiScannerLoop();
    bleScannerLoop();

    // Feeds FreeRTOS Idle task and prevents TG1WDT Watchdog resets
    delay(2);
}
