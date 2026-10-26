#include "src/wifi_scanner.h"
#include <WiFi.h>
#include "src/uart_protocol.h"

static bool scanning = false;

void wifiScannerInit() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.enableSTA(true);
}

void startWifiScan() {
    if (scanning) {
        Serial.println("[Wi-Fi] Scan already in progress...");
        return;
    }
    WiFi.enableSTA(true);
    // Non-blocking asynchronous Wi-Fi scan with hidden networks enabled
    WiFi.scanNetworks(true, true);
    scanning = true;
    Serial.println("[Wi-Fi] Scan initiated by STM32 request.");
}

bool isWifiScanning() {
    return scanning;
}

void wifiScannerLoop() {
    if (!scanning) return;

    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
        return;
    }

    if (n < 0) {
        Serial.printf("[Wi-Fi] Scan error code: %d\n", n);
        sendUartMessage("$WIFI,COUNT,0");
        scanning = false;
        WiFi.scanDelete();
        return;
    }

    Serial.printf("[Wi-Fi] Scan complete! Found %d networks.\n", n);

    // Send total count to STM32
    sendUartMessage("$WIFI,COUNT," + String(n));
    delay(20);

    // Transmit up to 15 items in compact format matching STM32 sscanf
    int sendCount = (n > 15) ? 15 : n;
    for (int i = 0; i < sendCount; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "<Hidden>";
        if (ssid.length() > 14) ssid = ssid.substring(0, 14);
        ssid.replace(',', '_');
        ssid.replace(' ', '_');
        int32_t rssi = WiFi.RSSI(i);

        // Compact format: $WIFI,ITEM,<idx>,<SSID>,<RSSI>
        String line = "$WIFI,ITEM," + String(i) + "," + ssid + "," + String(rssi);
        sendUartMessage(line);
        delay(25); // Prevents buffer overflow on STM32 USART1
    }

    sendUartMessage("$WIFI,SCAN,DONE");
    WiFi.scanDelete();
    scanning = false;
}
