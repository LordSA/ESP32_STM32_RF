#include "src/ble_scanner.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "src/uart_protocol.h"

static BLEScan* pBLEScan = nullptr;
static bool bleScanning = false;
static uint32_t scanDuration = 5;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Individual device callbacks can also be handled if needed
    }
};

void bleScannerInit() {
    BLEDevice::init("ESP32_Scanner");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}

static void bleScanCompleteCB(BLEScanResults foundDevices) {
    int count = foundDevices.getCount();
    sendUartMessage("$BT,COUNT," + String(count));

    for (int i = 0; i < count; ++i) {
        BLEAdvertisedDevice dev = foundDevices.getDevice(i);
        String addr = dev.getAddress().toString().c_str();
        int rssi = dev.getRSSI();
        String name = dev.getName().c_str();
        if (name.length() == 0) name = "<Unknown>";
        name.replace(',', '_');

        String msg = "$BT,ITEM," + String(i) + "," + addr + "," + String(rssi) + "," + name;
        sendUartMessage(msg);
        delay(5);
    }
    sendUartMessage("$BT,SCAN,DONE");
    pBLEScan->clearResults();
    bleScanning = false;
}

void startBleScan(uint32_t duration_sec) {
    if (bleScanning) {
        sendUartMessage("$BT,SCAN,ALREADY_RUNNING");
        return;
    }
    if (pBLEScan == nullptr) {
        bleScannerInit();
    }
    scanDuration = (duration_sec > 0 && duration_sec <= 30) ? duration_sec : 5;
    bleScanning = true;
    sendUartMessage("$BT,SCAN,STARTED");
    pBLEScan->start(scanDuration, bleScanCompleteCB, false);
}

void bleScannerLoop() {
    // BLE scan is handled asynchronously by callback
}

bool isBleScanning() {
    return bleScanning;
}
