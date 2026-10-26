#include "src/ble_scanner.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "src/uart_protocol.h"

static BLEScan* pBLEScan = nullptr;
static bool bleScanning = false;
static uint32_t scanDuration = 5;

void bleScannerInit() {
    BLEDevice::init("ESP32_Scanner");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

static void bleScanCompleteCB(BLEScanResults foundDevices) {
    int count = foundDevices.getCount();
    Serial.printf("[BLE] Discovery finished. Found %d devices.\n", count);

    sendUartMessage("$BT,COUNT," + String(count));
    delay(20);

    int sendCount = (count > 15) ? 15 : count;
    for (int i = 0; i < sendCount; ++i) {
        BLEAdvertisedDevice dev = foundDevices.getDevice(i);
        String addr = dev.getAddress().toString().c_str();
        int rssi = dev.getRSSI();
        String name = dev.getName().c_str();
        if (name.length() == 0) name = "<BLE_Dev>";
        if (name.length() > 14) name = name.substring(0, 14);
        name.replace(',', '_');
        name.replace(' ', '_');

        // Compact format matching STM32: $BT,ITEM,<idx>,<MAC>,<RSSI>,<NAME>
        String msg = "$BT,ITEM," + String(i) + "," + addr + "," + String(rssi) + "," + name;
        sendUartMessage(msg);
        delay(25);
    }

    sendUartMessage("$BT,SCAN,DONE");
    pBLEScan->clearResults();
    bleScanning = false;
}

void startBleScan(uint32_t duration_sec) {
    if (bleScanning) return;

    if (pBLEScan == nullptr) {
        bleScannerInit();
    }
    scanDuration = (duration_sec > 0 && duration_sec <= 10) ? duration_sec : 5;
    bleScanning = true;
    Serial.println("[BLE] Scan initiated by STM32 request.");
    pBLEScan->start(scanDuration, bleScanCompleteCB, false);
}

void bleScannerLoop() {
    // BLE scanning runs asynchronously with callback
}

bool isBleScanning() {
    return bleScanning;
}
