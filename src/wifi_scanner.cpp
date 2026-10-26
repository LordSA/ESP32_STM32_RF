#include "src/wifi_scanner.h"
#include <WiFi.h>
#include "src/uart_protocol.h"

static bool scanning = false;

void wifiScannerInit() {
    // Station mode ready for scanning
}

void startWifiScan() {
    if (scanning) {
        sendUartMessage("$WIFI,SCAN,ALREADY_RUNNING");
        return;
    }
    int res = WiFi.scanNetworks(true, false, false, 300);
    if (res == WIFI_SCAN_RUNNING) {
        scanning = true;
        sendUartMessage("$WIFI,SCAN,STARTED");
    } else {
        sendUartMessage("$WIFI,SCAN,ERROR");
    }
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
        sendUartMessage("$WIFI,SCAN,FAILED");
        scanning = false;
        WiFi.scanDelete();
        return;
    }

    // Directly matches STM32 sscanf(message, "$WIFI,%d", &count)
    sendUartMessage("$WIFI," + String(n));

    // Also send count and items for advanced viewers
    sendUartMessage("$WIFI,COUNT," + String(n));

    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "<Hidden>";
        ssid.replace(',', '_');
        int32_t rssi = WiFi.RSSI(i);
        int32_t ch = WiFi.channel(i);
        wifi_auth_mode_t auth = WiFi.encryptionType(i);

        String line = "$WIFI,ITEM," + String(i) + "," + ssid + "," + String(rssi) + "," + String(ch) + "," + String((int)auth);
        sendUartMessage(line);
        delay(10); // Generous delay to prevent STM32 32-byte RX buffer overrun
    }

    sendUartMessage("$WIFI,SCAN,DONE");
    WiFi.scanDelete();
    scanning = false;
}
