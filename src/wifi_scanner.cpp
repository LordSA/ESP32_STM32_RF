#include "src/wifi_scanner.h"
#include <WiFi.h>
#include "src/uart_protocol.h"

static bool scanning = false;

void wifiScannerInit() {
    WiFi.mode(WIFI_AP_STA);
    // Station mode ready for scanning
}

void startWifiScan() {
    if (scanning) {
        sendUartMessage("$WIFI,SCAN,ALREADY_RUNNING");
        return;
    }
    WiFi.enableSTA(true);
    int16_t res = WiFi.scanNetworks(true, true); // true = async, true = show hidden
    scanning = true;
    Serial.println("[Wi-Fi] Scan started...");
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
        Serial.printf("[Wi-Fi] Scan failed with code %d\n", n);
        sendUartMessage("$WIFI,COUNT,0");
        scanning = false;
        WiFi.scanDelete();
        return;
    }

    // Directly matches STM32 sscanf(message, "$WIFI,%d", &count)
    Serial.printf("[Wi-Fi] Scan completed! Found %d APs\n", n);

    // Also send count and items for advanced viewers
    sendUartMessage("$WIFI,COUNT," + String(n));
    delay(20);

    int sendCount = (n > 15) ? 15 : n;
    for (int i = 0; i < sendCount; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "<Hidden>";
        if (ssid.length() > 14) ssid = ssid.substring(0, 14); // Keep short for OLED
        ssid.replace(',', '_');
        ssid.replace(' ', '_');
        int32_t rssi = WiFi.RSSI(i);

        // Compact format: $WIFI,ITEM,<idx>,<SSID>,<RSSI>
        String line = "$WIFI,ITEM," + String(i) + "," + ssid + "," + String(rssi);
        sendUartMessage(line);
        delay(25); // Gives STM32 time to process each line without RX buffer overflow
    }

    WiFi.scanDelete();
    scanning = false;
    
}
